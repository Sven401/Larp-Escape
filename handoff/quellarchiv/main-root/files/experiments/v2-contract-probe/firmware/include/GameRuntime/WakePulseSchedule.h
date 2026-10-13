#pragma once

#include <cstdint>

namespace larp {
namespace runtime {
// Time-only policy; the adapter owns the physical pin and supplies periodic holds.
// A late service tick produces at most one edge, preserving pulse widths instead of
// compressing missed edges into a burst.
class WakePulseSchedule {
  public:
    WakePulseSchedule() : started(false), high(false), remaining(0U), changedAt(0U), hold(0U) {}

    void startPeriodic(std::uint32_t now, std::uint32_t initialLowHold) {
        started = true;
        high = false;
        remaining = 0U;
        changedAt = now;
        hold = initialLowHold;
    }

    bool requestWake(std::uint32_t now) {
        if (isWaking())
            return false;
        started = true;
        high = true;
        remaining = 8U;
        changedAt = now;
        hold = 100U;
        return true;
    }

    bool transitionDue(std::uint32_t now) const {
        return started && (now - changedAt) >= hold;
    }

    bool advance(std::uint32_t now, std::uint32_t nextPeriodicHold) {
        if (!transitionDue(now))
            return false;
        // Eight 100ms holds form HIGH/LOW/HIGH/LOW/HIGH/LOW/HIGH/LOW.
        // The final hold completes without an eighth physical edge.
        if (remaining > 0U) {
            --remaining;
            if (remaining > 0U)
                high = !high;
        } else
            high = !high;
        changedAt = now;
        hold = isWaking() ? 100U : nextPeriodicHold;
        return true;
    }

    bool isHigh() const {
        return high;
    }
    bool isWaking() const {
        return remaining > 0U;
    }

  private:
    bool started;
    bool high;
    std::uint8_t remaining;
    std::uint32_t changedAt;
    std::uint32_t hold;
};
} // namespace runtime
} // namespace larp
