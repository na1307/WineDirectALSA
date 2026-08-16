module;

#include <alsa/asoundlib.h>

#include "linux_functions.h"

export module alsa_control;

export class alsa_control final {
    snd_pcm_t *playback_pcm;
    snd_pcm_t *capture_pcm;
    bool playback_initialized;
    bool capture_initialized;
    bool linked;
    bool capture_enabled;

public:
    alsa_control(const char* playback_device_name, const char* capture_device_name);

    alsa_control(const alsa_control &) = delete;

    ~alsa_control();

    static bool get_devices(devices &devices);

    bool is_playback_initialized() const;

    bool is_capture_initialized() const;

    bool set_buffer(int, int, int, bool);

    bool start() const;

    audio_result wait_period() const;

    audio_result read(int32_t *, int) const;

    audio_result write(const int32_t *, int) const;

    bool prepare() const;

    bool stop() const;
};
