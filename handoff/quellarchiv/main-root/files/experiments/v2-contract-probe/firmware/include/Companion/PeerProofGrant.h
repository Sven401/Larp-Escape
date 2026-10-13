#pragma once
#include "Companion/PeerProof.h"
#include "Companion/PeerRegistryActor.h"
namespace larp {
namespace companion {
struct PeerProofGrantLimits {
    static const std::uint32_t QueueMilliseconds = 5000;
};
struct PeerProofGrantJob {
    PeerRegistryActorOrigin original;
    VerifiedPeerProof proof;
    std::uint32_t sequence = 0, receivedAt = 0;
    bool operator==(const PeerProofGrantJob &) const noexcept;
};
bool validPeerProofGrantJob(const PeerProofGrantJob &) noexcept;
bool peerProofGrantFresh(const PeerProofGrantJob &, std::uint32_t now) noexcept;
enum class PeerProofGrantPost : std::uint8_t { Posted, Duplicate, Busy, Rejected };
enum class PeerProofGrantState : std::uint8_t { Pending, Confirmed, Rejected, Unavailable };
} // namespace companion
} // namespace larp
