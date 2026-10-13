#pragma once

#include <cstdint>

namespace larp {
namespace domain {
class KeystoneMonitor {
  public:
    static const std::uint8_t ErrorThreshold = 5;

    KeystoneMonitor();

    // A match clears pending misses and any active error; a miss counts toward the threshold.
    void observe(bool matchesCurrentKeystone);
    bool hasError() const;
    void resetMisses();
    void clearError();
    void reset();

  private:
    std::uint8_t consecutiveMisses_;
    bool errorActive_;
};
} // namespace domain
} // namespace larp
