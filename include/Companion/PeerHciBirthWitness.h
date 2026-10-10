#pragma once

#include "Companion/PeerLinkSecurity.h"
#include <array>
#include <cstdint>

namespace larp {
namespace companion {

// Captures the original BTU actor's current GATT birth and ACL handle before
// an HCI encryption callback can be deferred. No role or bond mode is inferred.
class PeerHciBirthWitness {
  public:
    bool capture(const PeerTransportBinding &birth, std::uint16_t hciHandle) noexcept {
        if (!birth.valid() || hciHandle >= 0x0fff || active_)
            return false;
        const auto word = hciHandle / 8;
        const auto bit = static_cast<std::uint8_t>(1u << (hciHandle % 8));
        if (seen_[word] & bit)
            return false;
        seen_[word] |= bit;
        birth_ = birth;
        hciHandle_ = hciHandle;
        active_ = true;
        return true;
    }

    bool matches(const PeerTransportBinding &birth, std::uint16_t hciHandle) const noexcept {
        return active_ && hciHandle == hciHandle_ && birth == birth_;
    }

    void close() noexcept {
        active_ = false;
    }

  private:
    std::array<std::uint8_t, 512> seen_ = {};
    PeerTransportBinding birth_;
    std::uint16_t hciHandle_ = 0xffff;
    bool active_ = false;
};

} // namespace companion
} // namespace larp
