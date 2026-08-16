module;

#include <cstdint>
#include <array>
#include <vector>
#include <atomic>
#include <string>
#include <thread>

#include <unknwn.h>

export module casio;

import asio;

// ReSharper disable once CppUseInternalLinkage
enum class State {
    Created,
    Initialized,
    Prepared,
    Running
};

export class CASIO final : public ASIO::IASIO {
    ULONG ref_count;
    State state;
    HWND sys_handle;
    bool is_capture_available;
    bool capture_active;
    ASIO::SampleRate sample_rate;
    long buffer_size;
    ASIO::Callbacks callbacks;
    bool callbacks_valid;
    std::array<bool, 2> input_active;
    std::array<bool, 2> output_active;
    std::array<std::array<std::vector<std::int32_t>, 2>, 2> input_buffers;
    std::array<std::array<std::vector<std::int32_t>, 2>, 2> output_buffers;
    std::atomic<std::uint64_t> sample_position{};
    std::string last_error;
    std::jthread audio_thread;
    std::vector<std::int32_t> capture_interleaved_buffer;
    std::vector<std::int32_t> interleaved_buffer;
    std::uint64_t stream_start_timestamp_ns = 0;
    std::vector<std::int32_t> silence_buffer;

    void interleave(long index) noexcept;

    void deinterleave(const int32_t *src, int index) noexcept;

    void latch_start_timestamp() noexcept;

    bool recover_xrun() noexcept;

public:
    CASIO();

    CASIO(const CASIO &) = delete;

    ~CASIO();

    HRESULT QueryInterface(REFIID, void **) override;

    ULONG AddRef() override;

    ULONG Release() override;

    ASIO::Bool init(void *) override;

    void getDriverName(char *) override;

    long getDriverVersion() override;

    void getErrorMessage(char *) override;

    ASIO::Error start() override;

    ASIO::Error stop() override;

    ASIO::Error getChannels(std::int32_t *, std::int32_t *) override;

    ASIO::Error getLatencies(std::int32_t *, std::int32_t *) override;

    ASIO::Error getBufferSize(std::int32_t *, std::int32_t *, std::int32_t *, std::int32_t *) override;

    ASIO::Error canSampleRate(ASIO::SampleRate) override;

    ASIO::Error getSampleRate(ASIO::SampleRate *) override;

    ASIO::Error setSampleRate(ASIO::SampleRate) override;

    ASIO::Error getClockSources(ASIO::ClockSource *, std::int32_t *) override;

    ASIO::Error setClockSource(std::int32_t) override;

    ASIO::Error getSamplePosition(ASIO::Samples *, ASIO::TimeStamp *) override;

    ASIO::Error getChannelInfo(ASIO::ChannelInfo *) override;

    ASIO::Error createBuffers(ASIO::BufferInfo *, std::int32_t, std::int32_t, ASIO::Callbacks *) override;

    ASIO::Error disposeBuffers() override;

    ASIO::Error controlPanel() override;

    ASIO::Error future(std::int32_t, void *) override;

    ASIO::Error outputReady() override;
};
