#pragma once
#include "LedOutput.h"
#include <array>
#include <cstdint>

namespace larp { namespace led { namespace seal {

constexpr std::uint32_t Scale = 1000000U;
enum class State : std::uint8_t { Empty, Active, Stopping, Stopped };
enum class Operation : std::uint8_t { Start, Update, Stop };
enum class Cause : std::uint8_t { Snapshot, Progress };

struct Parameters {
    std::uint8_t count = 0;
    bool valid = true;
    Cause cause = Cause::Snapshot;
    Pixel colour, peak;
    std::uint16_t sweepMs = 600, pixelMs = 150, transitionMs = 300, pulseMs = 800;
    Parameters();
};
bool valid(const Parameters &parameters);
bool equal(const Parameters &a, const Parameters &b);

struct Command {
    std::uint32_t round = 0, generation = 0, sequence = 0;
    Operation operation = Operation::Start;
    Parameters parameters;
    bool inheritedConsumed = false, authorizePulse = false;
};

struct Snapshot {
    State state = State::Empty;
    std::uint32_t round = 0, generation = 0, sequence = 0;
    Parameters parameters;
    bool pulseConsumed = false, pulsePending = false;
};

// One serialized owner supplies monotone millisecond values; adjacent calls must
// be less than 2^31 ms apart. There is no heap, hardware, or clock I/O here.
// pulsePending includes both a scheduled pulse and its active interval.
class Scene {
public:
    const char *apply(const Command &command, std::uint32_t now);
    void service(std::uint32_t now);
    Frame render(std::uint32_t now);
    const Snapshot &snapshot() const { return snapshot_; }
    // Clear the contribution immediately, preserving round/generation and
    // pulse consumption as a tombstone. A new owner session uses Scene{}.
    void reset();

private:
    using Channels = std::array<std::uint32_t, 3>;
    using Values = std::array<Channels, PixelCount>;
    struct Ramp {
        Channels from{}, to{};
        std::uint32_t startedAt = 0;
        std::uint16_t delayMs = 0, durationMs = 0;
        bool moving = false;
    };
    Snapshot snapshot_{};
    Command lastCommand_{};
    std::array<Ramp, PixelCount> ramps_{};
    std::uint32_t pulseAnchor_ = 0;
    std::uint16_t pulseDelayMs_ = 0;

    Values value(std::uint32_t now) const;
    void transition(const Values &from, const Parameters &parameters, bool dark,
                    std::uint32_t now);
    void sweep(const Values &from, const Parameters &parameters, std::uint32_t now);
    void schedulePulse(std::uint32_t now);
};

}}} // namespace larp::led::seal
