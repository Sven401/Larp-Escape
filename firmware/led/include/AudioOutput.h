#pragma once
#include <array>
#include <cstdint>

namespace larp { namespace audio {

using CommandFrame = std::array<std::uint8_t, 10>;
enum class TxResult { Busy, Complete, Failed };

// Sole-loop-owned UART. Complete means bytes left the ESP UART, never that the
// player accepted the command or that a person heard the requested recording.
class AudioBus {
public:
    virtual ~AudioBus() = default;
    virtual bool begin() = 0;
    virtual bool submit(const CommandFrame &frame) = 0;
    virtual TxResult poll() = 0;
    virtual bool sampleBusyLow() = 0;
};

struct OutputSnapshot {
    std::uint32_t revision = 0, appliedRevision = 0;
    std::uint16_t track = 0;
    std::uint8_t volume = 5;
    bool ready = false, pending = false, fault = false;
    // Valid means an initialized GPIO sample; it does not verify the wire.
    bool busyValid = false, busyLow = false;
    bool active = false, paused = false, completed = false, timedOut = false;
};

class AudioOutput {
public:
    using Clock = std::uint32_t (*)();
    AudioOutput();
    AudioOutput(AudioBus &bus, Clock clock) : bus_(bus), clock_(clock) {}
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;
    bool begin();
    bool start(std::uint16_t track, std::uint8_t volume = 5);
    // Stop supersedes any pending Start, including volume/play preparation.
    bool stop();
    bool pause();
    bool resume();
    bool volume(std::uint8_t value);
    void service();
    void tick() { service(); }
    OutputSnapshot snapshot() const { return status_; }

    static constexpr std::uint16_t MaximumTrack = 255;
    static constexpr std::uint8_t DefaultVolume = 5, MaximumVolume = 10;
    static constexpr std::uint8_t RxPin = 25, TxPin = 26, BusyPin = 14;
    static constexpr std::uint32_t CommandGapMilliseconds = 100;
    static constexpr std::uint32_t TransferTimeoutMilliseconds = 500;
    static constexpr std::uint32_t MaximumPlaybackMilliseconds = 30000;
    static CommandFrame encode(std::uint8_t command, std::uint16_t parameter);
private:
    enum class Phase { Stop, Volume, Play, Pause, Resume, AdjustVolume };
    bool requestStop(bool timedOut);
    bool requestSingle(Phase phase);
    void fail();
    void completeTransfer(std::uint32_t now);
    AudioBus &bus_;
    Clock clock_;
    OutputSnapshot status_;
    bool initialized_ = false, inFlight_ = false, wantedPlay_ = false;
    bool haveCompletedTransfer_ = false, seenBusyLow_ = false, bounded_ = false;
    bool faultStopRequested_ = false, recoverySubmitted_ = false;
    Phase phase_ = Phase::Stop, sentPhase_ = Phase::Stop;
    std::uint16_t wantedTrack_ = 0, sentTrack_ = 0;
    std::uint8_t wantedVolume_ = DefaultVolume, sentVolume_ = DefaultVolume;
    std::uint32_t sentRevision_ = 0, sentAt_ = 0, completedAt_ = 0, playedAt_ = 0;
};

AudioBus &dfPlayerBus();

}} // namespace larp::audio
