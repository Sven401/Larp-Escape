#pragma once

#include <cstdint>

namespace larp {
namespace ledcontrol {

// This core records device-side software state. A submitted black frame is
// never evidence that the physical LEDs are dark.
enum class Effect : std::uint8_t { Solid = 0, Breathe = 1, Chase = 2 };
enum class Phase : std::uint8_t { Idle, Running, OffSubmitted, Fault };
enum class Outcome : std::uint8_t {
    Accepted, Stopped, Expired, RejectedBusy, RejectedInvalid,
    RejectedStale, DriverFault
};

struct Selection {
    std::uint8_t first = 0;
    std::uint8_t count = 23;
    std::uint8_t red = 0, green = 0, blue = 0;
    std::uint8_t brightness = 16;
    Effect effect = Effect::Solid;
    std::uint16_t durationMs = 3000;
};

struct Snapshot {
    Phase phase = Phase::Idle;
    Outcome outcome = Outcome::RejectedInvalid;
    std::uint32_t requestId = 0;
    std::uint32_t runId = 0;
    std::uint32_t startedAtMs = 0;
    std::uint32_t deadlineMs = 0;
    bool blackSubmitted = false;
};

class LedControlV1 {
public:
    static constexpr std::uint8_t LedCount = 23;
    static constexpr std::uint16_t MinimumDurationMs = 500;
    static constexpr std::uint16_t MaximumDurationMs = 10000;

    static bool valid(const Selection &) noexcept;
    Snapshot start(std::uint32_t requestId, std::uint32_t runId,
                   const Selection &, std::uint32_t nowMs) noexcept;
    Snapshot stop(std::uint32_t requestId, std::uint32_t runId,
                  std::uint32_t nowMs) noexcept;
    // Must be called by an independent timer/off path, including after a link loss.
    bool offDue(std::uint32_t nowMs) const noexcept;
    Snapshot blackSubmitted(std::uint32_t runId, std::uint32_t nowMs) noexcept;
    Snapshot fault(std::uint32_t runId) noexcept;
    const Snapshot &snapshot() const noexcept { return state_; }

private:
    Snapshot state_;
    std::uint32_t lastRequestId_ = 0;
    std::uint32_t lastRunId_ = 0;
    bool offRequested_ = false;
    static bool reached(std::uint32_t nowMs, std::uint32_t deadlineMs) noexcept {
        return static_cast<std::int32_t>(nowMs - deadlineMs) >= 0;
    }
};

} // namespace ledcontrol
} // namespace larp
