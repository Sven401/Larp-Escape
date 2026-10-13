#pragma once

#include "Companion/PeerLinkSecurity.h"
#include <array>
#include <cstdint>

namespace larp {
namespace companion {

// Mechanical gate for a future SDK-backed bond re-encryption intake. It cannot
// attest that a mode came from persistent storage and never grants a role.
class PeerReencryptionAttempt {
  public:
    enum class Outcome : std::uint8_t {
        Started, Duplicate, Observed, Invalid, Mismatch, Failed, Expired, Consumed
    };
    static const std::uint32_t WindowMilliseconds = 10000;

    Outcome begin(const PeerTransportBinding &birth, std::uint16_t hciHandle,
                  std::uint8_t selectedBondMode, std::uint32_t now) noexcept {
        if (!birth.valid() || hciHandle >= 0x0fff ||
            (selectedBondMode != 0x0d && selectedBondMode != 0x1d))
            return Outcome::Invalid;
        const auto word = hciHandle / 8;
        const auto bit = static_cast<std::uint8_t>(1u << (hciHandle % 8));
        if (phase_ == Phase::Pending)
            return birth == birth_ && hciHandle == hciHandle_
                       ? Outcome::Duplicate : Outcome::Mismatch;
        if (phase_ == Phase::Ready && birth == birth_)
            return Outcome::Consumed;
        if (seenHandles_[word] & bit)
            return Outcome::Consumed; // A late HCI event cannot identify a recycled birth.
        seenHandles_[word] |= bit;
        birth_ = birth;
        hciHandle_ = hciHandle;
        selectedMode_ = selectedBondMode;
        startedAt_ = now;
        phase_ = Phase::Pending;
        return Outcome::Started;
    }

    Outcome observe(const PeerTransportBinding &birth, std::uint16_t hciHandle,
                    std::uint8_t status, std::uint8_t enabled,
                    std::uint32_t now) noexcept {
        if (phase_ != Phase::Pending)
            return Outcome::Consumed;
        if (!(birth == birth_) || hciHandle != hciHandle_)
            return Outcome::Mismatch;
        phase_ = Phase::Failed; // Consume before interpreting the event.
        if (now - startedAt_ >= WindowMilliseconds)
            return Outcome::Expired;
        if (status != 0 || enabled != 1)
            return Outcome::Failed;
        phase_ = Phase::Ready;
        return Outcome::Observed;
    }

    bool observed(const PeerTransportBinding &birth, std::uint32_t now) const noexcept {
        return phase_ == Phase::Ready && birth == birth_ &&
               now - startedAt_ < WindowMilliseconds &&
               (selectedMode_ == 0x0d || selectedMode_ == 0x1d);
    }

    void close() noexcept { phase_ = Phase::Failed; }

  private:
    enum class Phase : std::uint8_t { Empty, Pending, Ready, Failed };
    // HCI connection handles use 12 bits. Never reuse a seen handle in this boot.
    // Saturation deliberately closes access until restart.
    std::array<std::uint8_t, 512> seenHandles_ = {};
    PeerTransportBinding birth_;
    std::uint16_t hciHandle_ = 0xffff;
    std::uint8_t selectedMode_ = 0;
    std::uint32_t startedAt_ = 0;
    Phase phase_ = Phase::Empty;
};
} // namespace companion
} // namespace larp