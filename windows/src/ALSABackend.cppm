module;

#include <cstdint>

#include "linux_functions.h"

export module alsa_backend;

export void make_realtime_thread();

export bool init_backend(const char* input, const char* output);

export bool backend_capture_available();

export bool set_backend_buffer(double, std::int32_t, std::int32_t, bool);

export audio_result read_backend(std::int32_t *, std::int32_t);

export audio_result write_backend(const std::int32_t *, std::int32_t);

export bool start_backend();

export audio_result wait_backend_period();

export bool prepare_backend();

export bool stop_backend();

export void uninit_backend();
