#pragma once

#include <cstdint>

namespace larp {
namespace runtime {
// Owned by the application producer. A failed enqueue does not consume a step.
class BrightnessStepSchedule {
  public:
    BrightnessStepSchedule() : initialized(false), lastQueuedMilliseconds(0U) {}

    bool due(std::uint32_t now, std::uint16_t intervalMilliseconds) {
        if (!initialized) {
            initialized = true;
            lastQueuedMilliseconds = now;
            return false;
        }
        return (now - lastQueuedMilliseconds) >= intervalMilliseconds;
    }

    void markQueued(std::uint32_t now) {
        lastQueuedMilliseconds = now;
    }

  private:
    bool initialized;
    std::uint32_t lastQueuedMilliseconds;
};
} // namespace runtime
} // namespace larp
