#pragma once
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
struct PeerTransportBinding {
    Identity device;
    BoundConnection connection;
    std::array<std::uint8_t, 6> address = {};
    std::uint32_t registration = 0, lifecycle = 0;
    std::uint8_t interface = 0xff;
    bool valid() const noexcept;
    bool operator==(const PeerTransportBinding &other) const noexcept {
        return device == other.device && connection == other.connection &&
               address == other.address && registration == other.registration &&
               lifecycle == other.lifecycle && interface == other.interface;
    }
};
struct PeerPairingResult {
    std::uint8_t reason = 0xff, securityLevel = 0, cancelled = 0, overBr = 0, mode = 0;
    bool operator==(const PeerPairingResult &other) const noexcept {
        return reason == other.reason && securityLevel == other.securityLevel &&
               cancelled == other.cancelled && overBr == other.overBr && mode == other.mode;
    }
};
struct PeerLinkState {
    PeerLinkState(std::uint8_t flags = 0, std::uint8_t keyBytes = 0)
        : flags(flags), keyBytes(keyBytes) {}
    std::uint8_t flags = 0, keyBytes = 0;
};
struct PeerLinkSecurityLimits {
    static const std::uint32_t PairMilliseconds = 30000;
};
enum class PeerPairingOutcome : std::uint8_t {
    Started,
    Duplicate,
    Completed,
    Invalid,
    Mismatch,
    Failed,
    Expired,
    Consumed
};
// Sole original SDK actor. A completion cannot attach to a current connection
// unless an earlier original pairing start was captured on that exact lifetime.
// Fresh pairing only in this cut; persisted bond-mode restore is a separate port.
class PeerLinkSecurity {
  public:
    PeerPairingOutcome start(const PeerTransportBinding &, std::uint32_t now) noexcept;
    PeerPairingOutcome complete(const PeerTransportBinding &, const PeerPairingResult &,
                                std::uint32_t now) noexcept;
    void close() noexcept;
    // No attempt token is supplied by SMP. Drain the abandoned terminal event
    // before a new lifetime can capture another attempt on the same address.
    bool discardAbandonedCompletion(const std::array<std::uint8_t, 6> &) noexcept;
    bool select(const PeerTransportBinding &, const PeerProofBinding &, const PeerLinkState &,
                OriginalPeerSecurity &) const noexcept;

  private:
    enum class Phase : std::uint8_t { Empty, Pending, Ready, Failed, Abandoned };
    Phase phase_ = Phase::Empty;
    PeerTransportBinding original_;
    PeerPairingResult completed_;
    std::uint32_t startedAt_ = 0;
};
} // namespace companion
} // namespace larp
