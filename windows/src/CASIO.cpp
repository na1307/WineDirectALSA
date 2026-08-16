module;

#include <atomic>
#include <thread>
#include <filesystem>

#include <windows.h>

#include "linux_functions.h"

module casio;

import globals;
import asio;
import alsa_backend;

constexpr int asio_buffer_size = 64;

constexpr int alsa_period_size = 32;
constexpr int alsa_buffer_size = 128;
constexpr int alsa_buffer_periods = alsa_buffer_size / alsa_period_size;

constexpr int asio_blocks_per_alsa_buffer = alsa_buffer_size / asio_buffer_size;

static_assert(alsa_buffer_size % alsa_period_size == 0);

static_assert(alsa_buffer_size % asio_buffer_size == 0);

CASIO::CASIO() : ref_count(0), state(State::Created), sys_handle(nullptr), is_capture_available(false), capture_active(false), sample_rate(48000.0),
                 buffer_size(asio_buffer_size), callbacks({}), callbacks_valid(false), input_active({}), output_active({}), sample_position(0) {
    global_ref_count++;
}

CASIO::~CASIO() {
    uninit_backend();

    global_ref_count--;
}

void CASIO::interleave(const long index) noexcept {
    const auto dst = interleaved_buffer.data();

    const auto left = output_active[0] ? output_buffers[0][index].data() : nullptr;
    const auto right = output_active[1] ? output_buffers[1][index].data() : nullptr;

    for (long i = 0; i < buffer_size; ++i) {
        dst[i * 2 + 0] = left ? left[i] : 0;
        dst[i * 2 + 1] = right ? right[i] : 0;
    }
}

void CASIO::deinterleave(const std::int32_t *src, const int index) noexcept {
    const auto left = input_active[0] ? input_buffers[0][index].data() : nullptr;
    const auto right = input_active[1] ? input_buffers[1][index].data() : nullptr;

    for (int i = 0; i < asio_buffer_size; ++i) {
        if (left) {
            left[i] = src[i * 2];
        }

        if (right) {
            right[i] = src[i * 2 + 1];
        }
    }
}

void CASIO::latch_start_timestamp() noexcept {
    stream_start_timestamp_ns = static_cast<std::uint64_t>(timeGetTime()) * 1'000'000ULL;
}

bool CASIO::recover_xrun() noexcept {
    // ASIO host가 overload notification을 지원하면 알림
    if (callbacks.asioMessage != nullptr) {
        const long supported = callbacks.asioMessage(ASIO::MessageSelector::SelectorSupported,
            static_cast<std::int32_t>(ASIO::MessageSelector::Overload), nullptr, nullptr);

        if (supported) {
            callbacks.asioMessage(ASIO::MessageSelector::Overload, 0, nullptr, nullptr);
        }
    }

    if (!prepare_backend()) {
        return false;
    }

    constexpr int recovery_blocks = alsa_buffer_size / asio_buffer_size;

    static_assert(alsa_buffer_size % asio_buffer_size == 0);

    for (int i = 0; i < recovery_blocks; ++i) {
        if (write_backend(silence_buffer.data(), asio_buffer_size) != audio_result_ok) {
            return false;
        }
    }

    sample_position.fetch_add(alsa_buffer_size, std::memory_order::relaxed);

    if (!start_backend()) {
        return false;
    }

    return true;
}

HRESULT CASIO::QueryInterface(REFIID riid, void **ppvObject) {
    HRESULT hr = S_OK;
    *ppvObject = nullptr;

    if (IsEqualIID(riid, CLSID_CASIO)) {
        *ppvObject = static_cast<IASIO*>(this);
    } else if (IsEqualIID(riid, IID_IUnknown)) {
        *ppvObject = static_cast<IUnknown*>(this);
    } else {
        hr = E_NOINTERFACE;
    }

    if (hr == S_OK) {
        static_cast<IUnknown*>(*ppvObject)->AddRef();
    }

    return hr;
}

ULONG CASIO::AddRef() {
    return ++ref_count;
}

ULONG CASIO::Release() {
    const auto ret = --ref_count;

    if (ref_count == 0) {
        delete this;
    }

    return ret;
}

ASIO::Bool CASIO::init(void *sysHandle) {
    sys_handle = static_cast<HWND>(sysHandle);
    HKEY wdaKey;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\WineDirectALSA", 0, KEY_READ, &wdaKey) != ERROR_SUCCESS) {
        return ASIO::Bool::False;
    }

    std::array<char, device_id_size> input_id{};
    std::array<char, device_id_size> output_id{};

    DWORD input_id_length = input_id.size();
    DWORD output_id_length = output_id.size();

    if (RegQueryValueExA(wdaKey, "Input", nullptr, nullptr, reinterpret_cast<LPBYTE>(input_id.data()),
        &input_id_length) != ERROR_SUCCESS) {
        last_error = "Input device is not set";

        RegCloseKey(wdaKey);

        return ASIO::Bool::False;
    }

    if (RegQueryValueExA(wdaKey, "Output", nullptr, nullptr, reinterpret_cast<LPBYTE>(output_id.data()),
        &output_id_length) != ERROR_SUCCESS) {
        last_error = "Output device is not set";

        RegCloseKey(wdaKey);

        return ASIO::Bool::False;
    }

    RegCloseKey(wdaKey);

    if (!init_backend(input_id.data(), output_id.data())) {
        return ASIO::Bool::False;
    }

    is_capture_available = backend_capture_available();
    state = State::Initialized;
    last_error.clear();

    return ASIO::Bool::True;
}

void CASIO::getDriverName(char *name) {
    if (name == nullptr) {
        return;
    }

    strncpy_s(name, 16, "WineDirectALSA", 31);

    name[31] = '\0';
}

long CASIO::getDriverVersion() {
    return 1;
}

void CASIO::getErrorMessage(char *string) {
    if (string == nullptr) {
        return;
    }

    strncpy_s(string, 123, last_error.c_str(), 123);

    string[123] = '\0';
}

ASIO::Error CASIO::start() {
    if (state != State::Prepared) {
        return ASIO::Error::InvalidMode;
    }

    if (!prepare_backend()) {
        return ASIO::Error::HWMalfunction;
    }

    state = State::Running;

    audio_thread = std::jthread([this](std::stop_token stop) {
        make_realtime_thread();
        sample_position.store(0, std::memory_order::relaxed);

        if (capture_active) {
            for (int ch = 0; ch < 2; ++ch) {
                std::fill(input_buffers[ch][0].begin(), input_buffers[ch][0].end(), 0);
            }
        }

        callbacks.bufferSwitch(0, ASIO::Bool::True);

        interleave(1);

        auto result = write_backend(interleaved_buffer.data(), buffer_size);

        if (result != audio_result_ok) {
            return;
        }

        interleave(0);

        result = write_backend(interleaved_buffer.data(), buffer_size);

        if (result != audio_result_ok) {
            return;
        }

        if (!start_backend()) {
            return;
        }

        latch_start_timestamp();

        long index = 1;

        while (!stop.stop_requested()) {
            auto ar = capture_active
                          ? read_backend(capture_interleaved_buffer.data(), asio_buffer_size)
                          : wait_backend_period();

            if (ar == audio_result_xrun) {
                if (!recover_xrun()) {
                    break;
                }

                continue;
            }

            if (ar != audio_result_ok) {
                break;
            }

            if (capture_active) {
                deinterleave(capture_interleaved_buffer.data(), index);
            }

            sample_position.fetch_add(asio_buffer_size, std::memory_order::relaxed);
            callbacks.bufferSwitch(index, ASIO::Bool::True);
            interleave(index);

            ar = write_backend(interleaved_buffer.data(), buffer_size);

            if (ar == audio_result_xrun) {
                index ^= 1;

                if (!recover_xrun()) {
                    break;
                }

                continue;
            }

            if (ar != audio_result_ok) {
                break;
            }

            index ^= 1;
        }
    });

    state = State::Running;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::stop() {
    if (state != State::Running) {
        return ASIO::Error::OK;
    }

    audio_thread.request_stop();
    audio_thread.join();
    stop_backend();

    state = State::Prepared;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::getChannels(std::int32_t *numInputChannels, std::int32_t *numOutputChannels) {
    if (numInputChannels == nullptr || numOutputChannels == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    *numInputChannels = is_capture_available ? 2 : 0;
    *numOutputChannels = 2;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::getLatencies(std::int32_t *inputLatency, std::int32_t *outputLatency) {
    if (inputLatency == nullptr || outputLatency == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    *inputLatency = 64;
    *outputLatency = alsa_buffer_size - alsa_period_size;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::getBufferSize(std::int32_t *minSize, std::int32_t *maxSize, std::int32_t *preferredSize, std::int32_t *granularity) {
    if (minSize == nullptr || maxSize == nullptr || preferredSize == nullptr || granularity == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    *minSize = asio_buffer_size;
    *maxSize = asio_buffer_size;
    *preferredSize = asio_buffer_size;
    *granularity = 0;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::canSampleRate(ASIO::SampleRate sampleRate) {
    if (sampleRate != floor(sampleRate)) {
        return ASIO::Error::NoClock;
    }

    const auto r = static_cast<unsigned>(sampleRate);

    switch (r) {
    case 44100:
    case 48000:
    case 96000:
    case 192000:
        return ASIO::Error::OK;

    default:
        return ASIO::Error::NoClock;
    }
}

ASIO::Error CASIO::getSampleRate(ASIO::SampleRate *sampleRate) {
    if (sampleRate == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    *sampleRate = sample_rate;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::setSampleRate(ASIO::SampleRate sampleRate) {
    if (state == State::Running) {
        return ASIO::Error::InvalidMode;
    }

    if (canSampleRate(sampleRate) != ASIO::Error::OK) {
        return ASIO::Error::NoClock;
    }

    sample_rate = sampleRate;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::getClockSources(ASIO::ClockSource *clocks, std::int32_t *numSources) {
    if (clocks == nullptr || numSources == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    if (*numSources < 1) {
        return ASIO::Error::InvalidParameter;
    }

    clocks[0].index = 0;
    clocks[0].associatedChannel = -1;
    clocks[0].associatedGroup = -1;
    clocks[0].isCurrentSource = ASIO::Bool::True;

    strncpy_s(clocks[0].name, 9, "Internal", 31);

    clocks[0].name[31] = '\0';
    *numSources = 1;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::setClockSource(std::int32_t reference) {
    return reference == 0 ? ASIO::Error::OK : ASIO::Error::InvalidParameter;
}

ASIO::Error CASIO::getSamplePosition(ASIO::Samples *sPos, ASIO::TimeStamp *tStamp) {
    if (sPos == nullptr || tStamp == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    if (state != State::Running) {
        return ASIO::Error::SPNotAdvancing;
    }

    const std::uint64_t pos = sample_position.load(std::memory_order::relaxed);
    const auto rate = static_cast<std::uint64_t>(sample_rate);

    // pos * 1e9를 바로 하면 장시간 실행 시 overflow가 빨리 올 수 있으므로
    // quotient/remainder로 나눠 계산
    const std::uint64_t seconds = pos / rate;
    const std::uint64_t remainder = pos % rate;

    const std::uint64_t timestamp = stream_start_timestamp_ns + seconds * 1'000'000'000ULL + remainder * 1'000'000'000ULL / rate;
    *sPos = pos;
    *tStamp = timestamp;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::getChannelInfo(ASIO::ChannelInfo *info) {
    if (info == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    if (info->channel < 0 || info->channel >= 2) {
        return ASIO::Error::InvalidParameter;
    }

    if (static_cast<bool>(info->isInput)) {
        if (!is_capture_available) {
            return ASIO::Error::InvalidParameter;
        }

        info->isActive = input_active[info->channel] ? ASIO::Bool::True : ASIO::Bool::False;
        info->channelGroup = 1;
        info->type = ASIO::SampleType::Int32LSB;

        std::snprintf(info->name, sizeof(info->name), "ALSA In %ld", info->channel + 1);
    } else {
        info->isActive = output_active[info->channel] ? ASIO::Bool::True : ASIO::Bool::False;
        info->channelGroup = 0;
        info->type = ASIO::SampleType::Int32LSB;

        std::snprintf(info->name, sizeof(info->name), "ALSA Out %ld", info->channel + 1);
    }

    return ASIO::Error::OK;
}

ASIO::Error CASIO::createBuffers(ASIO::BufferInfo *bufferInfos, std::int32_t numChannels, std::int32_t bufferSize, ASIO::Callbacks *callbacks) {
    if (bufferInfos == nullptr || callbacks == nullptr) {
        return ASIO::Error::InvalidParameter;
    }

    if (state != State::Initialized) {
        return ASIO::Error::InvalidMode;
    }

    if (bufferSize != asio_buffer_size) {
        return ASIO::Error::InvalidMode;
    }

    if (numChannels <= 0 || numChannels > 4) {
        return ASIO::Error::InvalidMode;
    }

    input_active.fill(false);
    output_active.fill(false);

    for (long i = 0; i < numChannels; ++i) {
        auto &info = bufferInfos[i];

        if (info.channelNum < 0 || info.channelNum >= 2) {
            return ASIO::Error::InvalidMode;
        }

        if (static_cast<bool>(info.isInput)) {
            if (!is_capture_available) {
                return ASIO::Error::InvalidMode;
            }

            if (input_active[info.channelNum]) {
                return ASIO::Error::InvalidMode;
            }

            input_buffers[info.channelNum][0].assign(bufferSize, std::int32_t{0});
            input_buffers[info.channelNum][1].assign(bufferSize, std::int32_t{0});

            info.buffers[0] = input_buffers[info.channelNum][0].data();
            info.buffers[1] = input_buffers[info.channelNum][1].data();
            input_active[info.channelNum] = true;
        } else {
            if (output_active[info.channelNum]) {
                return ASIO::Error::InvalidMode;
            }

            output_buffers[info.channelNum][0].assign(bufferSize, std::int32_t{0});
            output_buffers[info.channelNum][1].assign(bufferSize, std::int32_t{0});

            info.buffers[0] = output_buffers[info.channelNum][0].data();
            info.buffers[1] = output_buffers[info.channelNum][1].data();
            output_active[info.channelNum] = true;
        }
    }

    this->callbacks = *callbacks;
    callbacks_valid = true;
    buffer_size = bufferSize;
    capture_active = input_active[0] || input_active[1];

    if (!set_backend_buffer(sample_rate, alsa_period_size, alsa_buffer_size, capture_active)) {
        last_error = "Failed to configure ALSA buffer";

        return ASIO::Error::HWMalfunction;
    }

    capture_interleaved_buffer.resize(buffer_size * 2);
    interleaved_buffer.resize(buffer_size * 2);
    silence_buffer.resize(buffer_size * 2, 0);

    state = State::Prepared;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::disposeBuffers() {
    if (state == State::Running) {
        stop();
    }

    if (state != State::Prepared) {
        return ASIO::Error::InvalidMode;
    }

    for (auto &channel : input_buffers) {
        for (auto &half : channel) {
            half.clear();
        }
    }

    for (auto &channel : output_buffers) {
        for (auto &half : channel) {
            half.clear();
        }
    }

    input_active.fill(false);
    output_active.fill(false);
    callbacks = {};
    callbacks_valid = false;

    state = State::Initialized;

    return ASIO::Error::OK;
}

ASIO::Error CASIO::controlPanel() {
    std::array<char, MAX_PATH> path{};

    GetModuleFileNameA(this_module, path.data(), path.size());

    const auto fs_path = std::filesystem::path(path.data()).parent_path();
    const auto ctrl_path = fs_path / "wdactrl.exe";

    ShellExecuteA(nullptr, "open", ctrl_path.string().c_str(), nullptr, fs_path.string().c_str(), SW_SHOWNORMAL);

    return ASIO::Error::OK;
}

ASIO::Error CASIO::future(std::int32_t selector, void *opt) {
    return ASIO::Error::InvalidParameter;
}

ASIO::Error CASIO::outputReady() {
    return ASIO::Error::NotPresent;
}
