module;

#include <cstdint>
#include <cstring>

#include "wine_unixlib.h"

module alsa_backend;

void make_realtime_thread() {
    linux_call(request_rt_thread, nullptr);
}

bool init_backend(const char* input, const char* output) {
    init_data data{};

    strcpy_s(data.input, input);
    strcpy_s(data.output, output);

    return linux_call(init_alsa, &data) == 0;
}

bool backend_capture_available() {
    return linux_call(capture_available, nullptr) == 0;
}

bool set_backend_buffer(const double sample_rate, const std::int32_t period_size, const std::int32_t buffer_size, const bool enable_capture) {
    buffer_setting args{
        .sample_rate = static_cast<int>(sample_rate),
        .period_size = period_size,
        .buffer_size = buffer_size,
        .enable_capture = enable_capture
    };

    return linux_call(set_alsa_buffer, &args) == 0;
}

audio_result read_backend(std::int32_t *samples, const std::int32_t frames) {
    read_buffer args{
        .samples = samples,
        .frames = frames
    };

    return static_cast<audio_result>(linux_call(read_alsa, &args));
}

audio_result write_backend(const std::int32_t *samples, const std::int32_t frames) {
    write_buffer args{
        .samples = samples,
        .frames = frames
    };

    return static_cast<audio_result>(linux_call(write_alsa, &args));
}

bool start_backend() {
    return linux_call(start_alsa, nullptr) == 0;
}

audio_result wait_backend_period() {
    return static_cast<audio_result>(linux_call(wait_alsa_period, nullptr));
}

bool prepare_backend() {
    return linux_call(prepare_alsa, nullptr) == 0;
}

bool stop_backend() {
    return linux_call(stop_alsa, nullptr) == 0;
}

void uninit_backend() {
    linux_call(uninit_alsa, nullptr);
}
