module;

#include <thread>
#include <systemd/sd-bus.h>

module audio_reservation;

namespace {
    struct reservation {
        int priority;
        const char *application_name;
        const char *application_device_name;
        bool release_requested;
    };

    sd_bus *bus = nullptr;
    sd_bus_slot *object_slot = nullptr;
    std::jthread dbus_thread;

    reservation reservation_data = {
        .priority = 100,
        .application_name = "WineDirectALSA",
        .application_device_name = "ALSA hw:0",
        .release_requested = false
    };

    int request_release(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        const auto r = static_cast<reservation*>(userdata);
        int requested_priority;
        int k = sd_bus_message_read(m, "i", &requested_priority);

        if (k < 0) {
            return k;
        }

        bool accept = requested_priority > r->priority;

        if (accept) {
            //r->release_requested = true;
        }

        return sd_bus_reply_method_return(m, "b", /*accept*/ false);
    }

    int get_priority(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply, void *userdata,
        sd_bus_error *ret_error) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "i", r->priority);
    }

    int get_application_name(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply, void *userdata,
        sd_bus_error *ret_error) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "s", r->application_name);
    }

    int get_application_device_name(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply,
        void *userdata,
        sd_bus_error *ret_error) {
        const auto r = static_cast<reservation*>(userdata);

        return sd_bus_message_append(reply, "s", r->application_device_name);
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

    void dbus_loop(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            int r;

            while ((r = sd_bus_process(bus, nullptr)) > 0) {}

            if (r < 0) {
                break;
            }

            if (reservation_data.release_requested) {
                // 장치 해제 절차 요청
                reservation_data.release_requested = false;
            }

            sd_bus_wait(bus, 100'000); // 100 ms
        }
    }
}

bool request_acqiure() {
    int r = sd_bus_open_user(&bus);

    if (r < 0) {
        return false;
    }

    r = sd_bus_add_object_vtable(
        bus,
        &object_slot,

        "/org/freedesktop/ReserveDevice1/Audio0",
        "org.freedesktop.ReserveDevice1",

        reserve_vtable,
        &reservation_data
    );

    if (r < 0) {
        sd_bus_unref(bus);

        return false;
    }

    r = sd_bus_request_name(bus, "org.freedesktop.ReserveDevice1.Audio0", SD_BUS_NAME_ALLOW_REPLACEMENT);

    if (r == -EEXIST) {
        sd_bus_message *reply = nullptr;
        auto error = SD_BUS_ERROR_NULL;
        constexpr int priority = 100;

        r = sd_bus_call_method(
            bus,
            "org.freedesktop.ReserveDevice1.Audio0",
            "/org/freedesktop/ReserveDevice1/Audio0",
            "org.freedesktop.ReserveDevice1",
            "RequestRelease",
            &error,
            &reply,
            "i",
            priority
        );

        if (r < 0) {
            sd_bus_error_free(&error);
            sd_bus_message_unref(reply);

            return false;
        }

        int accepted;

        r = sd_bus_message_read(reply, "b", &accepted);

        if (r < 0 || !accepted) {
            sd_bus_error_free(&error);
            sd_bus_message_unref(reply);
            sd_bus_slot_unref(object_slot);
            sd_bus_unref(bus);

            return false;
        }

        r = sd_bus_request_name(
            bus,
            "org.freedesktop.ReserveDevice1.Audio0",
            SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING
        );

        if (r < 0) {
            sd_bus_error_free(&error);
            sd_bus_message_unref(reply);
            sd_bus_slot_unref(object_slot);
            sd_bus_unref(bus);

            return false;
        }

        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);
    } else if (r < 0) {
        sd_bus_slot_unref(object_slot);
        sd_bus_unref(bus);

        return false;
    }

    dbus_thread = std::jthread(dbus_loop);

    return true;
}

void release() {
    if (bus == nullptr) {
        return;
    }

    dbus_thread.request_stop();
    dbus_thread.join();

    sd_bus_release_name(bus, "org.freedesktop.ReserveDevice1.Audio0");

    if (object_slot != nullptr) {
        sd_bus_slot_unref(object_slot);

        object_slot = nullptr;
    }

    sd_bus_unref(bus);

    bus = nullptr;
    reservation_data.release_requested = false;
}
