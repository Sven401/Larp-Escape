#pragma once
#include "Companion/PeerProofFrame.h"
namespace larp {
namespace companion {
enum class PeerProofReceiveKind : std::uint8_t {
    Receiving,
    DuplicateFragment,
    Completed,
    Stale,
    InvalidFrame,
    UnsupportedFrame,
    Busy,
    OutOfOrder,
    Conflict,
    DecodeRejected
};
struct PeerProofReceive {
    PeerProofReceiveKind kind = PeerProofReceiveKind::InvalidFrame;
    PeerProofOutcome proof = PeerProofOutcome::Consumed;
    std::uint16_t message = 0, received = 0;
};
// Sole worker, fixed 72-byte document. Original binding is caller metadata copied
// at the SDK source, not parsed from the client's frame. Deadline belongs to
// the original challenge, so fragments/duplicates never restart it.
class PeerProofReceiver {
  public:
    explicit PeerProofReceiver(PeerProofOwner &owner) : owner_(owner) {}
    PeerProofReceive receive(const PeerProofBinding &, const char *, std::size_t) noexcept;

  private:
    PeerProofOwner &owner_;
    PeerProofBinding original_;
    std::array<char, PeerProofLimits::ProofBytes> bytes_ = {};
    std::uint16_t message_ = 0, received_ = 0;
    bool busy_ = false;
};
} // namespace companion
} // namespace larp
