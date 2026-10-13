#include "GameDomain/KeystoneMonitor.h"

namespace larp {
namespace domain {
KeystoneMonitor::KeystoneMonitor() : consecutiveMisses_(0), errorActive_(false) {}

void KeystoneMonitor::observe(bool matchesCurrentKeystone) {
    if (matchesCurrentKeystone) {
        consecutiveMisses_ = 0;
        errorActive_ = false;
        return;
    }

    ++consecutiveMisses_;
    if (consecutiveMisses_ >= ErrorThreshold) {
        errorActive_ = true;
        consecutiveMisses_ = 0;
    }
}

bool KeystoneMonitor::hasError() const {
    return errorActive_;
}

void KeystoneMonitor::resetMisses() {
    consecutiveMisses_ = 0;
}

void KeystoneMonitor::clearError() {
    errorActive_ = false;
}

void KeystoneMonitor::reset() {
    consecutiveMisses_ = 0;
    errorActive_ = false;
}
} // namespace domain
} // namespace larp
