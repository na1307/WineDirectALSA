module;

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>
#include <systemd/sd-bus.h>

module audio_reservation;

namespace {
    constexpr auto reserve_interface = "org.freedesktop.ReserveDevice1";
    constexpr int reservation_priority = std::numeric_limits<int>::max();

    struct reservation {
        int priority = reservation_priority;
        std::string application_name = "WineDirectALSA";
        std::string application_device_name;
        std::string service_name;
        std::string object_path;
        sd_bus_slot *object_slot = nullptr;
    };

    sd_bus *bus = nullptr;
    std::vector<std::unique_ptr<reservation>> reservations;
    std::jthread dbus_thread;

    int request_release(sd_bus_message *m, void *, sd_bus_error *) {
        int requested_priority;
        const int r = sd_bus_message_read(m, "i", &requested_priority);

        if (r < 0) {
            return r;
        }

        // WineDirectALSA cannot safely transfer an active ASIO stream to
        // another process. Use a non-preemptible reservation instead of
        // promising a release that cannot be completed atomically.
        const bool accept = requested_priority > reservation_priority;

        return sd_bus_reply_method_return(m, "b", accept);
    }

    int get_priority(sd_bus *, const char *, const char *, const char *, sd_bus_message *reply, void *userdata, sd_bus_error *) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "i", r->priority);
    }

    int get_application_name(sd_bus *, const char *, const char *, const char *, sd_bus_message *reply, void *userdata, sd_bus_error *) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "s", r->application_name.c_str());
    }

    int get_application_device_name(sd_bus *, const char *, const char *, const char *, sd_bus_message *reply, void *userdata,
                                    sd_bus_error *) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "s", r->application_device_name.c_str());
    }

    const sd_bus_vtable reserve_vtable[] = {
        SD_BUS_VTABLE_START(0),

        SD_BUS_METHOD(
            "RequestRelease",
            "i",
            "b",
            request_release,
            0
        ),

        SD_BUS_PROPERTY(
            "Priority",
            "i",
            get_priority,
            0,
            SD_BUS_VTABLE_PROPERTY_CONST
        ),

        SD_BUS_PROPERTY(
            "ApplicationName",
            "s",
            get_application_name,
            0,
            SD_BUS_VTABLE_PROPERTY_CONST
        ),

        SD_BUS_PROPERTY(
            "ApplicationDeviceName",
            "s",
            get_application_device_name,
            0,
            SD_BUS_VTABLE_PROPERTY_CONST
        ),

        SD_BUS_VTABLE_END
    };

    int get_card_index(const std::string_view device_name) {
        const auto separator = device_name.find(':');

        if (separator == std::string_view::npos) {
            return -1;
        }

        auto card_name = device_name.substr(separator + 1);

        if (card_name.starts_with("CARD=")) {
            card_name.remove_prefix(5);
        }

        if (const auto comma = card_name.find(','); comma != std::string_view::npos) {
            card_name = card_name.substr(0, comma);
        }

        if (card_name.empty()) {
            return -1;
        }

        const std::string card_name_string(card_name);

        return snd_card_get_index(card_name_string.c_str());
    }

    void cleanup() {
        if (dbus_thread.joinable()) {
            dbus_thread.request_stop();
            dbus_thread.join();
        }

        if (bus != nullptr) {
            for (const auto &r : reservations) {
                sd_bus_release_name(bus, r->service_name.c_str());
            }
        }

        for (const auto &r : reservations) {
            if (r->object_slot != nullptr) {
                sd_bus_slot_unref(r->object_slot);
                r->object_slot = nullptr;
            }
        }

        reservations.clear();

        if (bus != nullptr) {
            sd_bus_unref(bus);
            bus = nullptr;
        }
    }

    bool acquire(const int card_index, const std::string_view device_name) {
        auto r = std::make_unique<reservation>();
        const auto reservation_name = std::string("Audio") + std::to_string(card_index);

        const auto fail = [&r] {
            if (r->object_slot != nullptr) {
                sd_bus_slot_unref(r->object_slot);
                r->object_slot = nullptr;
            }

            return false;
        };

        r->application_device_name = std::string("ALSA ") + std::string(device_name);
        r->service_name = std::string(reserve_interface) + "." + reservation_name;
        r->object_path = std::string("/org/freedesktop/ReserveDevice1/") + reservation_name;

        int result = sd_bus_add_object_vtable(
            bus,
            &r->object_slot,
            r->object_path.c_str(),
            reserve_interface,
            reserve_vtable,
            r.get()
        );

        if (result < 0) {
            return fail();
        }

        result = sd_bus_request_name(bus, r->service_name.c_str(), SD_BUS_NAME_ALLOW_REPLACEMENT);

        if (result == -EEXIST) {
            sd_bus_message *reply = nullptr;
            sd_bus_error error{};

            result = sd_bus_call_method(
                bus,
                r->service_name.c_str(),
                r->object_path.c_str(),
                reserve_interface,
                "RequestRelease",
                &error,
                &reply,
                "i",
                r->priority
            );

            int accepted = false;

            if (result >= 0) {
                result = sd_bus_message_read(reply, "b", &accepted);
            }

            sd_bus_error_free(&error);
            sd_bus_message_unref(reply);

            if (result < 0 || !accepted) {
                return fail();
            }

            result = sd_bus_request_name(
                bus,
                r->service_name.c_str(),
                SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING
            );
        }

        if (result < 0) {
            return fail();
        }

        reservations.push_back(std::move(r));

        return true;
    }

    void dbus_loop(const std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            int r;

            while ((r = sd_bus_process(bus, nullptr)) > 0) {}

            if (r < 0) {
                break;
            }

            sd_bus_wait(bus, 100'000); // 100 ms
        }
    }
}

bool request_acquire(const char *playback_device_name, const char *capture_device_name) {
    cleanup();

    std::vector<std::pair<int, std::string_view>> cards;

    const auto add_card = [&cards](const char *device_name) {
        if (device_name == nullptr || std::string_view(device_name) == "(None)") {
            return;
        }

        const int card_index = get_card_index(device_name);

        if (card_index < 0) {
            return;
        }

        const auto duplicate = std::ranges::find_if(cards, [card_index](const auto &card) {
            return card.first == card_index;
        });

        if (duplicate == cards.end()) {
            cards.emplace_back(card_index, device_name);
        }
    };

    add_card(playback_device_name);
    add_card(capture_device_name);

    if (cards.empty()) {
        return true;
    }

    if (sd_bus_open_user(&bus) < 0) {
        bus = nullptr;

        return false;
    }

    for (const auto &[card_index, device_name] : cards) {
        if (!acquire(card_index, device_name)) {
            cleanup();

            return false;
        }
    }

    dbus_thread = std::jthread(dbus_loop);

    return true;
}

void release() {
    cleanup();
}
