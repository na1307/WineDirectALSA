#include <memory>

#include <unixlib.h>

#include "linux_functions.h"

import realtime_thread;
import audio_reservation;
import alsa_control;

static NTSTATUS request_rt_thread_func(void *);
static NTSTATUS init_alsa_func(void *);
static NTSTATUS capture_available_func(void *);
static NTSTATUS set_alsa_buffer_func(void *);
static NTSTATUS read_alsa_func(void *);
static NTSTATUS write_alsa_func(void *);
static NTSTATUS start_alsa_func(void *);
static NTSTATUS wait_alsa_period_func(void *);
static NTSTATUS prepare_alsa_func(void *);
static NTSTATUS stop_alsa_func(void *);
static NTSTATUS uninit_alsa_func(void *);
static NTSTATUS get_devices_func(void *);

const unixlib_entry_t __wine_unix_call_funcs[] = {
    request_rt_thread_func,
    init_alsa_func,
    capture_available_func,
    set_alsa_buffer_func,
    read_alsa_func,
    write_alsa_func,
    start_alsa_func,
    wait_alsa_period_func,
    prepare_alsa_func,
    stop_alsa_func,
    uninit_alsa_func,
    get_devices_func
};

static std::unique_ptr<alsa_control> ac(nullptr);

NTSTATUS request_rt_thread_func(void *args) {
    make_realtime_thread();

    return 0;
}

NTSTATUS init_alsa_func(void *args) {
    if (ac != nullptr && ac->is_playback_initialized()) {
        return 1;
    }

    const auto data = static_cast<init_data*>(args);

    if (!request_acquire(data->output, data->input)) {
        return 1;
    }

    ac = std::make_unique<alsa_control>(data->output, data->input);

    if (!ac->is_playback_initialized()) {
        ac.reset();
        release();

        return 1;
    }

    return 0;
}

NTSTATUS capture_available_func(void *args) {
    return ac != nullptr && ac->is_capture_initialized() ? 0 : 1;
}

NTSTATUS set_alsa_buffer_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized()) {
        return 1;
    }

    const auto str = static_cast<buffer_setting*>(args);

    if (!ac->set_buffer(str->sample_rate, str->period_size, str->buffer_size, str->enable_capture)) {
        return 1;
    }

    return 0;
}

NTSTATUS read_alsa_func(void *args) {
    if (ac == nullptr || !ac->is_capture_initialized()) {
        return audio_result_error;
    }

    const auto str = static_cast<read_buffer*>(args);

    return ac->read(str->samples, str->frames);
}

NTSTATUS write_alsa_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized()) {
        return audio_result_error;
    }

    const auto str = static_cast<write_buffer*>(args);

    return ac->write(str->samples, str->frames);
}

NTSTATUS start_alsa_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized()) {
        return 1;
    }

    if (!ac->start()) {
        return 1;
    }

    return 0;
}

NTSTATUS wait_alsa_period_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized())
        return audio_result_error;

    return ac->wait_period();
}

NTSTATUS prepare_alsa_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized()) {
        return 1;
    }

    if (!ac->prepare()) {
        return 1;
    }

    return 0;
}

NTSTATUS stop_alsa_func(void *args) {
    if (ac == nullptr || !ac->is_playback_initialized()) {
        return 1;
    }

    if (!ac->stop()) {
        return 1;
    }

    return 0;
}

NTSTATUS uninit_alsa_func(void *args) {
    if (ac == nullptr) {
        return 1;
    }

    ac.reset();
    release();

    return 0;
}

NTSTATUS get_devices_func(void *args) {
    const auto d = static_cast<devices*>(args);

    return alsa_control::get_devices(*d) ? 0 : 1;
}
