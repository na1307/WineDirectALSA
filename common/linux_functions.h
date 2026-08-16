#pragma once

#include <stdint.h>

typedef enum : uint32_t {
    request_rt_thread,
    init_alsa,
    capture_available,
    set_alsa_buffer,
    read_alsa,
    write_alsa,
    start_alsa,
    wait_alsa_period,
    prepare_alsa,
    stop_alsa,
    uninit_alsa,
    get_devices,

    linux_funcs_count
} linux_functions;

constexpr uint32_t device_id_size = 128;
constexpr uint32_t device_name_size = 256;

typedef struct {
    char input[device_id_size];
    char output[device_id_size];
} init_data;

typedef struct {
    const int32_t sample_rate;
    const int32_t period_size;
    const int32_t buffer_size;
    const bool enable_capture;
} buffer_setting;

typedef struct {
    int32_t *samples;
    int32_t frames;
} read_buffer;

typedef struct {
    const int32_t *samples;
    int32_t frames;
} write_buffer;

typedef enum : uint32_t {
    audio_result_ok,
    audio_result_xrun,
    audio_result_error
} audio_result;

typedef enum : uint32_t {
    device_type_input,
    device_type_output,
    device_type_inout
} device_type;

typedef struct {
    char id[device_id_size];
    char display_name[device_name_size];
    device_type type;
} device;

typedef struct {
    device* devices;
    uint32_t capacity;
    uint32_t length;
} devices;
