#pragma once

#include <cstdint>

namespace larp {
namespace diagnostic {

// State behind the test image's independent LED-off timer. All calls must be
// serialized by the platform adapter. A completed off submission is software
// evidence only; it does not establish that the physical LEDs are dark.
class DevelopmentLedDeadlineCore {
  public:
    bool arm(std::uint32_t ticket, std::uint64_t startedUs) noexcept;
    bool requestStop(std::uint32_t ticket) noexcept;
    bool timerFired(std::uint32_t ticket) noexcept;
    bool needsBlack() const noexcept { return active_ && !blackSubmitted_ && blackRequested_; }
    bool blackSubmitted(std::uint32_t ticket, std::uint64_t finishedUs) noexcept;
    void fault(std::uint32_t ticket) noexcept;
    bool ready() const noexcept { return !faulted_ && active_ == 0; }
    bool faulted() const noexcept { return faulted_; }
    std::uint32_t activeTicket() const noexcept { return active_; }
    std::uint32_t lastCompletedTicket() const noexcept { return completed_; }
    bool blackSubmittedFor(std::uint32_t ticket) const noexcept {
        return ticket && submitted_ == ticket;
    }
    bool missedFor(std::uint32_t ticket) const noexcept { return ticket && missed_ == ticket; }

    static const std::uint64_t TimerUs = 400000;
    static const std::uint64_t MaximumOffUs = 500000;

  private:
    void finish() noexcept;
    std::uint32_t active_ = 0, completed_ = 0, submitted_ = 0, missed_ = 0;
    std::uint64_t startedUs_ = 0;
    bool timerFired_ = false, blackRequested_ = false, blackSubmitted_ = false;
    bool faulted_ = false;
};

} // namespace diagnostic
} // namespace larp
