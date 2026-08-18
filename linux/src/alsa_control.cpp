module;

#include <vector>
#include <cstring>
#include <format>
#include <chrono>
#include <thread>

#include <alsa/asoundlib.h>

#include "linux_functions.h"

module alsa_control;

namespace {
    int open_pcm_with_retry(snd_pcm_t **pcm, const char *device_name, const snd_pcm_stream_t stream) {
        constexpr auto retry_interval = std::chrono::milliseconds(5);
        constexpr auto retry_timeout = std::chrono::milliseconds(500);
        const auto deadline = std::chrono::steady_clock::now() + retry_timeout;

        int err;

        do {
            err = snd_pcm_open(pcm, device_name, stream, 0);

            if (err != -EBUSY) {
                return err;
            }

            std::this_thread::sleep_for(retry_interval);
        } while (std::chrono::steady_clock::now() < deadline);

        return err;
    }
}

alsa_control::alsa_control(const char *playback_device_name, const char *capture_device_name) : playback_pcm(nullptr), capture_pcm(nullptr),
                                                                                                playback_initialized(false),
                                                                                                capture_initialized(false),
                                                                                                linked(false),
                                                                                                capture_enabled(false) {
    snd_pcm_t *playback_pcm = nullptr;
    int err = open_pcm_with_retry(&playback_pcm, playback_device_name, SND_PCM_STREAM_PLAYBACK);

    if (err < 0) {
        // Failed
        return;
    }

    this->playback_pcm = playback_pcm;
    playback_initialized = true;

    if (strcmp(capture_device_name, "(None)") != 0) {
        snd_pcm_t *capture_pcm = nullptr;
        err = open_pcm_with_retry(&capture_pcm, capture_device_name, SND_PCM_STREAM_CAPTURE);

        if (err < 0) {
            // Failed
            return;
        }

        this->capture_pcm = capture_pcm;
        capture_initialized = true;
    }
}

alsa_control::~alsa_control() {
    if (playback_initialized) {
        stop();
        snd_pcm_close(playback_pcm);

        playback_initialized = false;
    }

    if (capture_initialized) {
        snd_pcm_close(capture_pcm);

        capture_initialized = false;
    }
}

bool alsa_control::get_devices(devices &devices) {
    int card = -1;

    if (snd_card_next(&card) < 0 || card < 0) {
        return false;
    }

    std::vector<::device> v_devices;

    while (card >= 0) {
        const auto card_name = std::format("hw:{}", card);

        snd_ctl_t *ctl;
        std::string card_display_name;

        if (snd_ctl_open(&ctl, card_name.c_str(), 0) == 0) {
            std::string card_id;

            snd_ctl_card_info_t *card_info;
            snd_ctl_card_info_alloca(&card_info);

            if (snd_ctl_card_info(ctl, card_info) >= 0) {
                card_id = snd_ctl_card_info_get_id(card_info);
                const char *card_longname = snd_ctl_card_info_get_longname(card_info);

                card_display_name.append(card_longname);
            }

            int dev = -1;

            while (true) {
                if (snd_ctl_pcm_next_device(ctl, &dev) < 0 || dev < 0) {
                    break;
                }

                bool has_playback = false;
                bool has_capture = false;
                std::string pcm_name;

                {
                    snd_pcm_info_t *info;
                    snd_pcm_info_alloca(&info);

                    snd_pcm_info_set_device(info, dev);
                    snd_pcm_info_set_subdevice(info, 0);
                    snd_pcm_info_set_stream(info, SND_PCM_STREAM_PLAYBACK);

                    if (snd_ctl_pcm_info(ctl, info) >= 0) {
                        has_playback = true;

                        if (const char *name = snd_pcm_info_get_name(info)) {
                            pcm_name = name;
                        }
                    }
                }

                {
                    snd_pcm_info_t *info;
                    snd_pcm_info_alloca(&info);

                    snd_pcm_info_set_device(info, dev);
                    snd_pcm_info_set_subdevice(info, 0);
                    snd_pcm_info_set_stream(info, SND_PCM_STREAM_CAPTURE);

                    if (snd_ctl_pcm_info(ctl, info) >= 0) {
                        has_capture = true;

                        if (pcm_name.empty()) {
                            if (const char *name = snd_pcm_info_get_name(info)) {
                                pcm_name = name;
                            }
                        }
                    }
                }

                if (!has_playback && !has_capture) {
                    continue;
                }

                ::device d;

                // id
                const auto pcm_device = std::format("hw:CARD={},DEV={}", card_id, dev);

                std::snprintf(d.id, sizeof(d.id), "%s", pcm_device.c_str());

                // display name
                std::string display_name;

                if (!card_display_name.empty() && !pcm_name.empty()) {
                    display_name =
                        std::format("{} - {}", card_display_name, pcm_name);
                } else if (!pcm_name.empty()) {
                    display_name = pcm_name;
                } else if (!card_display_name.empty()) {
                    display_name = card_display_name;
                } else {
                    display_name = pcm_device;
                }

                std::snprintf(d.display_name, sizeof(d.display_name), "%s", display_name.c_str());

                // type
                if (has_playback && has_capture) {
                    d.type = device_type_inout;
                } else if (has_playback) {
                    d.type = device_type_output;
                } else {
                    d.type = device_type_input;
                }

                v_devices.push_back(d);
            }

            snd_ctl_close(ctl);
        }

        if (snd_card_next(&card) < 0) {
            break;
        }
    }

    const auto length = static_cast<uint32_t>(v_devices.size());

    devices.length = length;

    if (devices.capacity == 0 || devices.devices == nullptr) {
        return true;
    }

    if (length > devices.capacity) {
        return false;
    }

    std::memcpy(devices.devices, v_devices.data(), length * sizeof(device));

    return true;
}

bool alsa_control::is_playback_initialized() const {
    return playback_initialized;
}

bool alsa_control::is_capture_initialized() const {
    return capture_initialized;
}

bool alsa_control::set_buffer(const int sample_rate, const int period_size, const int buffer_size, const bool enable_capture) {
    if (!playback_initialized || sample_rate <= 0 || period_size <= 0 || buffer_size <= 0) {
        return false;
    }

    const auto requested_rate = static_cast<unsigned int>(sample_rate);
    const auto requested_period = static_cast<snd_pcm_uframes_t>(period_size);
    const auto requested_buffer = static_cast<snd_pcm_uframes_t>(buffer_size);

    capture_enabled = enable_capture && capture_initialized;

    if (linked) {
        if (snd_pcm_unlink(playback_pcm) < 0) {
            return false;
        }

        linked = false;
    }

    if (playback_initialized) {
        snd_pcm_hw_params_t *playback_params;

        snd_pcm_hw_params_alloca(&playback_params);

        if (snd_pcm_hw_params_any(playback_pcm, playback_params) < 0 ||
            snd_pcm_hw_params_set_access(playback_pcm, playback_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(playback_pcm, playback_params, SND_PCM_FORMAT_S32_LE) < 0 ||
            snd_pcm_hw_params_set_channels(playback_pcm, playback_params, 2) < 0 ||
            snd_pcm_hw_params_set_rate(playback_pcm, playback_params, requested_rate, 0) < 0 ||
            snd_pcm_hw_params_set_period_size(playback_pcm, playback_params, requested_period, 0) < 0 ||
            snd_pcm_hw_params_set_buffer_size(playback_pcm, playback_params, requested_buffer) < 0 ||
            snd_pcm_hw_params(playback_pcm, playback_params) < 0) {
            return false;
        }

        snd_pcm_uframes_t playback_actual_period = 0;
        snd_pcm_uframes_t playback_actual_buffer = 0;
        unsigned int playback_actual_rate = 0;
        int playback_dir = 0;

        if (snd_pcm_hw_params_get_period_size(playback_params, &playback_actual_period, &playback_dir) < 0 ||
            snd_pcm_hw_params_get_buffer_size(playback_params, &playback_actual_buffer) < 0 ||
            snd_pcm_hw_params_get_rate(playback_params, &playback_actual_rate, &playback_dir) < 0) {
            return false;
        }

        if (playback_actual_period != requested_period || playback_actual_buffer != requested_buffer || playback_actual_rate != requested_rate) {
            return false;
        }

        snd_pcm_sw_params_t *sw;

        snd_pcm_sw_params_alloca(&sw);

        if (snd_pcm_sw_params_current(playback_pcm, sw) < 0) {
            return false;
        }

        if (snd_pcm_sw_params_set_avail_min(playback_pcm, sw, requested_period) < 0) {
            return false;
        }

        snd_pcm_uframes_t boundary;

        if (snd_pcm_sw_params_get_boundary(sw, &boundary) < 0) {
            return false;
        }

        if (snd_pcm_sw_params_set_start_threshold(playback_pcm, sw, requested_buffer + 1) < 0) {
            return false;
        }

        if (snd_pcm_sw_params(playback_pcm, sw) < 0) {
            return false;
        }
    }

    if (capture_enabled) {
        snd_pcm_hw_params_t *capture_params;

        snd_pcm_hw_params_alloca(&capture_params);

        if (snd_pcm_hw_params_any(capture_pcm, capture_params) < 0 ||
            snd_pcm_hw_params_set_access(capture_pcm, capture_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(capture_pcm, capture_params, SND_PCM_FORMAT_S32_LE) < 0 ||
            snd_pcm_hw_params_set_channels(capture_pcm, capture_params, 2) < 0 ||
            snd_pcm_hw_params_set_rate(capture_pcm, capture_params, requested_rate, 0) < 0 ||
            snd_pcm_hw_params_set_period_size(capture_pcm, capture_params, requested_period, 0) < 0 ||
            snd_pcm_hw_params_set_buffer_size(capture_pcm, capture_params, requested_buffer) < 0 ||
            snd_pcm_hw_params(capture_pcm, capture_params) < 0) {
            return false;
        }

        snd_pcm_uframes_t capture_actual_period = 0;
        snd_pcm_uframes_t capture_actual_buffer = 0;
        unsigned int capture_actual_rate = 0;
        int capture_dir = 0;

        if (snd_pcm_hw_params_get_period_size(capture_params, &capture_actual_period, &capture_dir) < 0 ||
            snd_pcm_hw_params_get_buffer_size(capture_params, &capture_actual_buffer) < 0 ||
            snd_pcm_hw_params_get_rate(capture_params, &capture_actual_rate, &capture_dir) < 0) {
            return false;
        }

        if (capture_actual_period != requested_period || capture_actual_buffer != requested_buffer || capture_actual_rate != requested_rate) {
            return false;
        }

        if (snd_pcm_link(playback_pcm, capture_pcm) >= 0) {
            linked = true;
        }
    }

    return true;
}

bool alsa_control::start() const {
    if (linked) {
        return snd_pcm_start(playback_pcm) >= 0;
    }

    if (capture_enabled) {
        const int err = snd_pcm_start(capture_pcm);

        if (err < 0) {
            return false;
        }
    }

    return snd_pcm_start(playback_pcm) >= 0;
}

audio_result alsa_control::wait_period() const {
    while (true) {
        const int result = snd_pcm_wait(playback_pcm, SND_PCM_WAIT_INFINITE);

        if (result > 0) {
            return audio_result_ok;
        }

        if (result == -EINTR) {
            continue;
        }

        if (result == -EPIPE) {
            return audio_result_xrun;
        }

        return audio_result_error;
    }
}

audio_result alsa_control::read(int32_t *samples, const int frames) const {
    int read_frames = 0;

    while (read_frames < frames) {
        constexpr int channels = 2;
        const auto r = snd_pcm_readi(capture_pcm, samples + read_frames * channels, frames - read_frames);

        if (r > 0) {
            read_frames += static_cast<int>(r);

            continue;
        }

        if (r == -EINTR) {
            continue;
        }

        if (r == -EPIPE) {
            return audio_result_xrun;
        }

        return audio_result_error;
    }

    return audio_result_ok;
}

audio_result alsa_control::write(const int32_t *samples, const int frames) const {
    int written = 0;

    while (written < frames) {
        constexpr int channels = 2;
        const auto r = snd_pcm_writei(playback_pcm, samples + written * channels, frames - written);

        if (r > 0) {
            written += static_cast<int>(r);

            continue;
        }

        if (r == -EINTR) {
            continue;
        }

        if (r == -EPIPE) {
            return audio_result_xrun;
        }

        return audio_result_error;
    }

    return audio_result_ok;
}

bool alsa_control::prepare() const {
    if (snd_pcm_prepare(playback_pcm) < 0) {
        return false;
    }

    if (!linked && capture_enabled && snd_pcm_prepare(capture_pcm) < 0) {
        return false;
    }

    return true;
}

bool alsa_control::stop() const {
    if (!playback_initialized) {
        return true;
    }

    if (linked) {
        return snd_pcm_drop(playback_pcm) >= 0;
    }

    auto ok = snd_pcm_drop(playback_pcm) >= 0;

    if (capture_enabled) {
        ok = snd_pcm_drop(capture_pcm) >= 0 && ok;
    }

    return ok;
}
