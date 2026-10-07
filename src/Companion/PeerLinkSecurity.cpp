#include "Companion/PeerLinkSecurity.h"
#include <algorithm>
namespace larp {
namespace companion {
bool PeerTransportBinding::valid() const noexcept {
    return device.valid() && connection.token && registration && lifecycle && interface &&
           interface != 0xff && (connection.handle & 0xff) == interface &&
           std::any_of(address.begin(), address.end(), [](std::uint8_t b) { return b != 0; });
}
PeerPairingOutcome PeerLinkSecurity::start(const PeerTransportBinding &caller,
                                           std::uint32_t now) noexcept {
    const auto original = caller;
    if (!original.valid())
        return PeerPairingOutcome::Invalid;
    if (phase_ == Phase::Abandoned || (phase_ == Phase::Pending && !(original == original_)))
        return PeerPairingOutcome::Mismatch;
    if (phase_ != Phase::Empty && original == original_) {
        if (now - startedAt_ >= PeerLinkSecurityLimits::PairMilliseconds &&
            phase_ == Phase::Pending) {
            phase_ = Phase::Abandoned;
            return PeerPairingOutcome::Expired;
        }
        if (phase_ == Phase::Pending)
            return PeerPairingOutcome::Duplicate;
        phase_ = Phase::Abandoned; // A re-pair cannot borrow the previously completed mode.
        return PeerPairingOutcome::Consumed;
    }
    if (phase_ != Phase::Empty && original_.registration &&
        (!(original.device == original_.device) ||
         original.registration != original_.registration ||
         original.interface != original_.interface || original.lifecycle <= original_.lifecycle ||
         original.connection.token <= original_.connection.token))
        return PeerPairingOutcome::Mismatch;
    original_ = original;
    startedAt_ = now;
    completed_ = {};
    phase_ = Phase::Pending;
    return PeerPairingOutcome::Started;
}
PeerPairingOutcome PeerLinkSecurity::complete(const PeerTransportBinding &caller,
                                              const PeerPairingResult &input,
                                              std::uint32_t now) noexcept {
    const auto original = caller;
    const auto result = input;
    if (phase_ == Phase::Empty || !(original == original_))
        return PeerPairingOutcome::Mismatch;
    if (phase_ == Phase::Ready) {
        if (result == completed_)
            return PeerPairingOutcome::Duplicate;
        phase_ = Phase::Failed;
        return PeerPairingOutcome::Failed;
    }
    if (phase_ != Phase::Pending)
        return PeerPairingOutcome::Consumed;
    phase_ = Phase::Failed; // Consume before interpreting the result.
    if (now - startedAt_ >= PeerLinkSecurityLimits::PairMilliseconds)
        return PeerPairingOutcome::Expired;
    // Values are byte copies from the pinned SDK callback, never client flags.
    // reason0, authenticated4, no cancel/BR, SC+MITM+bond; keypress may be present.
    if (result.reason || result.securityLevel != 4 || result.cancelled || result.overBr ||
        (result.mode & 0x0d) != 0x0d || (result.mode & ~0x1d))
        return PeerPairingOutcome::Failed;
    completed_ = result;
    phase_ = Phase::Ready;
    return PeerPairingOutcome::Completed;
}
void PeerLinkSecurity::close() noexcept {
    if (phase_ != Phase::Abandoned)
        phase_ = phase_ == Phase::Pending ? Phase::Abandoned : Phase::Failed;
}
bool PeerLinkSecurity::discardAbandonedCompletion(
    const std::array<std::uint8_t, 6> &address) noexcept {
    const auto original = address;
    if (phase_ != Phase::Abandoned || original != original_.address)
        return false;
    phase_ = Phase::Failed;
    return true;
}
bool PeerLinkSecurity::select(const PeerTransportBinding &caller, const PeerProofBinding &proof,
                              const PeerLinkState &input,
                              OriginalPeerSecurity &destination) const noexcept {
    const auto original = caller;
    const auto binding = proof;
    const auto link = input;
    if (phase_ != Phase::Ready || !(original == original_) || !binding.valid() ||
        !(binding.device == original.device) || !(binding.connection == original.connection) ||
        binding.registration != original.registration || binding.interface != original.interface ||
        link.keyBytes != 16 || (link.flags & 0x16) != 0x16)
        return false;
    OriginalPeerSecurity out;
    out.original = binding;
    out.encrypted = out.mitm = out.secureConnections = out.bonded = true;
    destination = out;
    return true;
}
} // namespace companion
} // namespace larp
