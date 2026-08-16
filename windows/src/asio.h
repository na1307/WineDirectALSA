/*
 * asio.h
 *
 * Reimplementation of Steinberg ASIO SDK
 *
 * Not all ASIO APIs are implemented.
 * Only the APIs required for this project have been implemented.
 */

#pragma once

#if !defined(__cplusplus) || __cplusplus < 201402L
#error "This code is for C++14 or later"
#else

#include <cstdint>

#include <unknwn.h>

#define DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(type) [[deprecated("Use ASIO::SampleType::" #type " instead.")]] constexpr auto ASIOST ## type = ASIO::SampleType::type
#define DEFINE_ASIO_ERROR_CONSTEXPR(error) [[deprecated("Use ASIO::Error::" #error " instead.")]] constexpr auto ASE_ ## error = ASIO::Error::error
#define DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(flag) [[deprecated("Use ASIO::TimeCodeFlags::" #flag " instead.")]] constexpr auto kTc ## flag = ASIO::TimeCodeFlags::flag
#define DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(flag) [[deprecated("Use ASIO::TimeInfoFlags::" #flag " instead.")]] constexpr auto k ## flag = ASIO::TimeInfoFlags::flag
#define DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(selector) [[deprecated("Use ASIO::MessageSelector::" #selector " instead.")]] constexpr auto kAsio ## selector = ASIO::MessageSelector::selector

#pragma pack(push,4)

namespace ASIO {
    typedef std::int64_t Samples;
    typedef std::int64_t TimeStamp;
    typedef double SampleRate;

    enum class Bool : std::int32_t {
        False = 0,
        True = 1
    };

    enum class SampleType : std::int32_t {
        Int16MSB = 0,
        Int24MSB = 1,
        Int32MSB = 2,
        Float32MSB = 3,
        Float64MSB = 4,

        Int32MSB16 = 8,
        Int32MSB18 = 9,
        Int32MSB20 = 10,
        Int32MSB24 = 11,

        Int16LSB = 16,
        Int24LSB = 17,
        Int32LSB = 18,
        Float32LSB = 19,
        Float64LSB = 20,

        Int32LSB16 = 24,
        Int32LSB18 = 25,
        Int32LSB20 = 26,
        Int32LSB24 = 27,

        DSDInt8LSB1 = 32,
        DSDInt8MSB1 = 33,
        DSDInt8NER8 = 40,

        LastEntry
    };

    enum class Error : std::int32_t {
        OK = 0,
        SUCCESS = 0x3f4847a0,
        NotPresent = -1000,
        HWMalfunction,
        InvalidParameter,
        InvalidMode,
        SPNotAdvancing,
        NoClock,
        NoMemory
    };

    enum class TimeCodeFlags : std::uint32_t {
        Valid = 1,
        Running = 1 << 1,
        Reverse = 1 << 2,
        Onspeed = 1 << 3,
        Still = 1 << 4,
        SpeedValid = 1 << 8
    };

    struct TimeCode {
        double speed;
        Samples timeCodeSamples;
        TimeCodeFlags flags;
        char future[64];
    };

    enum class TimeInfoFlags : std::uint32_t {
        SystemTimeValid = 1,
        SamplePositionValid = 1 << 1,
        SampleRateValid = 1 << 2,
        SpeedValid = 1 << 3,
        SampleRateChanged = 1 << 4,
        ClockSourceChanged = 1 << 5
    };

    struct TimeInfo {
        double speed;
        TimeStamp systemTime;
        Samples samplePosition;
        SampleRate sampleRate;
        TimeInfoFlags flags;
        char reserved[12];
    };

    struct Time {
        std::int32_t reserved[4];
        TimeInfo timeInfo;
        TimeCode timeCode;
    };

    enum class MessageSelector {
        SelectorSupported = 1,
        EngineVersion,
        ResetRequest,
        BufferSizeChange,
        ResyncRequest,
        LatenciesChanged,
        SupportsTimeInfo,
        SupportsTimeCode,
        MMCCommand,
        SupportsInputMonitor,
        SupportsInputGain,
        SupportsInputMeter,
        SupportsOutputGain,
        SupportsOutputMeter,
        Overload,

        NumMessageSelectors
    };

    struct Callbacks {
        void (*bufferSwitch)(std::int32_t doubleBufferIndex, Bool directProcess);
        void (*sampleRateDidChange)(SampleRate sRate);
        long (*asioMessage)(MessageSelector selector, std::int32_t value, void *message, double *opt);
        Time* (*bufferSwitchTimeInfo)(Time *params, std::int32_t doubleBufferIndex, Bool directProcess);
    };

    struct ClockSource {
        std::int32_t index;
        std::int32_t associatedChannel;
        std::int32_t associatedGroup;
        Bool isCurrentSource;
        char name[32];
    };

    struct ChannelInfo {
        std::int32_t channel;
        Bool isInput;
        Bool isActive;
        std::int32_t channelGroup;
        SampleType type;
        char name[32];
    };

    struct BufferInfo {
        Bool isInput;
        std::int32_t channelNum;
        void *buffers[2];
    };

    struct IASIO : IUnknown {
        virtual Bool init(void *sysHandle) = 0;
        virtual void getDriverName(char *name) = 0;
        virtual long getDriverVersion() = 0;
        virtual void getErrorMessage(char *string) = 0;
        virtual Error start() = 0;
        virtual Error stop() = 0;
        virtual Error getChannels(std::int32_t *numInputChannels, std::int32_t *numOutputChannels) = 0;
        virtual Error getLatencies(std::int32_t *inputLatency, std::int32_t *outputLatency) = 0;
        virtual Error getBufferSize(std::int32_t *minSize, std::int32_t *maxSize, std::int32_t *preferredSize, std::int32_t *granularity) = 0;
        virtual Error canSampleRate(SampleRate sampleRate) = 0;
        virtual Error getSampleRate(SampleRate *sampleRate) = 0;
        virtual Error setSampleRate(SampleRate sampleRate) = 0;
        virtual Error getClockSources(ClockSource *clocks, std::int32_t *numSources) = 0;
        virtual Error setClockSource(std::int32_t reference) = 0;
        virtual Error getSamplePosition(Samples *sPos, TimeStamp *tStamp) = 0;
        virtual Error getChannelInfo(ChannelInfo *info) = 0;
        virtual Error createBuffers(BufferInfo *bufferInfos, std::int32_t numChannels, std::int32_t bufferSize, Callbacks *callbacks) = 0;
        virtual Error disposeBuffers() = 0;
        virtual Error controlPanel() = 0;
        virtual Error future(std::int32_t selector, void *opt) = 0;
        virtual Error outputReady() = 0;
    };
}

[[deprecated("Use ASIO::Bool::False instead.")]]
constexpr auto ASIOFalse = ASIO::Bool::False;

[[deprecated("Use ASIO::Bool::True instead.")]]
constexpr auto ASIOTrue = ASIO::Bool::True;

DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int16MSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int24MSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32MSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Float32MSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Float64MSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32MSB16);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32MSB18);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32MSB20);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32MSB24);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int16LSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int24LSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32LSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Float32LSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Float64LSB);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32LSB16);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32LSB18);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32LSB20);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(Int32LSB24);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(DSDInt8LSB1);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(DSDInt8MSB1);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(DSDInt8NER8);
DEFINE_ASIO_SAMPLE_TYPE_CONSTEXPR(LastEntry);

DEFINE_ASIO_ERROR_CONSTEXPR(OK);
DEFINE_ASIO_ERROR_CONSTEXPR(SUCCESS);
DEFINE_ASIO_ERROR_CONSTEXPR(NotPresent);
DEFINE_ASIO_ERROR_CONSTEXPR(HWMalfunction);
DEFINE_ASIO_ERROR_CONSTEXPR(InvalidParameter);
DEFINE_ASIO_ERROR_CONSTEXPR(InvalidMode);
DEFINE_ASIO_ERROR_CONSTEXPR(SPNotAdvancing);
DEFINE_ASIO_ERROR_CONSTEXPR(NoClock);
DEFINE_ASIO_ERROR_CONSTEXPR(NoMemory);

DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(Valid);
DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(Running);
DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(Reverse);
DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(Onspeed);
DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(Still);
DEFINE_ASIO_TIME_CODE_FLAGS_CONSTEXPR(SpeedValid);

DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(SystemTimeValid);
DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(SamplePositionValid);
DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(SampleRateValid);
DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(SpeedValid);
DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(SampleRateChanged);
DEFINE_ASIO_TIME_INFO_FLAGS_CONSTEXPR(ClockSourceChanged);

DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SelectorSupported);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(EngineVersion);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(ResetRequest);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(BufferSizeChange);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(ResyncRequest);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(LatenciesChanged);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsTimeInfo);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsTimeCode);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(MMCCommand);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsInputMonitor);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsInputGain);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsInputMeter);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsOutputGain);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(SupportsOutputMeter);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(Overload);
DEFINE_ASIO_MESSAGE_SELECTOR_CONSTEXPR(NumMessageSelectors);

#pragma pack(pop)
#endif
