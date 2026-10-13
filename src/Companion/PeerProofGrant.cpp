#include "Companion/PeerProofGrant.h"
namespace larp {
namespace companion {
bool PeerProofGrantJob::operator==(const PeerProofGrantJob &v) const noexcept {
    return original == v.original && proof.original == v.proof.original &&
           proof.credential == v.proof.credential && proof.sequence == v.proof.sequence &&
           proof.issuedAt == v.proof.issuedAt && proof.verifiedAt == v.proof.verifiedAt &&
           sequence == v.sequence && receivedAt == v.receivedAt;
}
bool validPeerProofGrantJob(const PeerProofGrantJob &j) noexcept {
    // Pure structural checks have no external observation ports.
    const auto &v = j;
    return v.sequence && v.original.valid() && v.original.authority.epoch &&
           !v.original.authority.gameMaster && !v.original.authority.principal &&
           v.proof.original.valid() && v.proof.sequence && v.proof.credential.peer.valid() &&
           v.proof.credential.generation && v.proof.credential.key[0] == 4 &&
           v.original.device == v.proof.original.device &&
           v.original.registration == v.proof.original.registration &&
           v.original.interface == v.proof.original.interface &&
           v.original.authority.epoch >= v.proof.original.connection.token &&
           v.proof.verifiedAt - v.proof.issuedAt < PeerProofLimits::MaxAgeMilliseconds &&
           v.receivedAt - v.proof.issuedAt < PeerProofLimits::MaxAgeMilliseconds &&
           v.proof.verifiedAt - v.proof.issuedAt <= v.receivedAt - v.proof.issuedAt;
}
bool peerProofGrantFresh(const PeerProofGrantJob &j, std::uint32_t now) noexcept {
    return now - j.receivedAt < PeerProofGrantLimits::QueueMilliseconds &&
           now - j.proof.issuedAt < PeerProofLimits::MaxAgeMilliseconds;
}
} // namespace companion
} // namespace larp
