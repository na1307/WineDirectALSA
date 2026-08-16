module;

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

module realtime_thread;

void make_realtime_thread() {
    sd_bus *bus = nullptr;

    int r = sd_bus_open_system(&bus);

    if (r < 0) {
        return;
    }

    auto cleanup = [&] {
        sd_bus_unref(bus);
    };

    auto error = SD_BUS_ERROR_NULL;
    int64_t rttime_usec_max = 0;

    r = sd_bus_get_property_trivial(
        bus,
        "org.freedesktop.RealtimeKit1",
        "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1",
        "RTTimeUSecMax",
        &error,
        'x',
        &rttime_usec_max);

    if (r < 0) {
        sd_bus_error_free(&error);
        cleanup();

        return;
    }

    sd_bus_error_free(&error);

    rlimit current{};

    if (getrlimit(RLIMIT_RTTIME, &current) < 0) {
        cleanup();

        return;
    }

    auto target = static_cast<rlim_t>(rttime_usec_max);

    if (current.rlim_max != RLIM_INFINITY) {
        target = std::min(target, current.rlim_max);
    }

    if (target < 1) {
        cleanup();

        return;
    }

    const rlimit limit{
        .rlim_cur = target,
        .rlim_max = target
    };

    if (setrlimit(RLIMIT_RTTIME, &limit) < 0) {
        cleanup();

        return;
    }

    int32_t max_priority = 0;

    r = sd_bus_get_property_trivial(
        bus,
        "org.freedesktop.RealtimeKit1",
        "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1",
        "MaxRealtimePriority",
        &error,
        'i',
        &max_priority);

    if (r < 0) {
        sd_bus_error_free(&error);
        cleanup();

        return;
    }

    sd_bus_error_free(&error);

    const uint32_t priority = std::min<uint32_t>(20, static_cast<uint32_t>(max_priority));
    const auto tid = static_cast<uint64_t>(syscall(SYS_gettid));

    r = sd_bus_call_method(
        bus,
        "org.freedesktop.RealtimeKit1",
        "/org/freedesktop/RealtimeKit1",
        "org.freedesktop.RealtimeKit1",
        "MakeThreadRealtime",
        &error,
        nullptr,
        "tu",
        tid,
        priority);

    if (r < 0) {
        sd_bus_error_free(&error);
        cleanup();

        return;
    }

    sd_bus_error_free(&error);
    cleanup();
}
