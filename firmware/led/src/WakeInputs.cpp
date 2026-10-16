#include "WakeInputs.h"

namespace larp {
namespace wake {
bool WakeInputs::update(std::uint8_t reg, std::uint8_t set,
                        std::uint8_t clear) noexcept {
    std::uint8_t before = 0, after = 0;
    if (!bus_.read(Address, reg, before)) return false;
    const auto wanted = static_cast<std::uint8_t>((before | set) & ~clear);
    if (wanted != before && !bus_.write(Address, reg, wanted)) return false;
    return bus_.read(Address, reg, after) && after == wanted;
}
bool WakeInputs::configure() noexcept {
    // Never change IOCON/BANK or whole ports: the other pins may be matrix pins.
    // BANK=1 IOCON has bit7 set at both 0x05 and 0x15. Reject that layout first,
    // including an ambiguous BANK=0 configuration, without any register writes.
    std::uint8_t first = 0, second = 0;
    if (!bus_.read(Address, 0x05, first) || !bus_.read(Address, 0x15, second) ||
        (first == second && (first & 0x80))) return false;
    if (!bus_.read(Address, 0x0a, first) || !bus_.read(Address, 0x0b, second) ||
        first != second || (first & 0x80)) return false;
    // Original input/pullup assumption. IPOL is cleared only for these four
    // pins so GPIOB reports raw HIGH. B7 suitability still requires chip review.
    return update(0x01, PinMask, 0) && update(0x03, 0, PinMask) &&
           update(0x0d, PinMask, 0);
}
bool WakeInputs::begin(std::uint32_t now) noexcept {
    state_ = {};
    initialized_ = false;
    lastInit_ = now;
    lastPoll_ = now - PollMs;
    initialized_ = bus_.begin() && configure();
    return initialized_;
}
void WakeInputs::service(std::uint32_t now) noexcept {
    if (!initialized_) {
        if (now - lastInit_ < RetryMs || !begin(now)) return;
    }
    if (now - lastPoll_ < PollMs) return;
    lastPoll_ = now;
    // One GPIOB snapshot preserves simultaneous states; there is no matrix scan
    // and no debounce/filter that could hide short raw-level changes in the test.
    std::uint8_t raw = 0;
    if (!bus_.read(Address, 0x13, raw)) {
        state_ = {};
        initialized_ = false;
        lastInit_ = now;
        return;
    }
    state_.activeMask = ((raw >> 7) & 1) | ((raw >> 5) & 2) |
                        ((raw >> 2) & 4) | (raw & 8);
    state_.valid = true;
}
} // namespace wake
} // namespace larp
