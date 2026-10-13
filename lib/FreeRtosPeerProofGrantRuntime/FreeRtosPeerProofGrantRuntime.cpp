#include "FreeRtosPeerProofGrantRuntime.h"
namespace larp {
namespace companion {
bool FreeRtosPeerProofGrantRuntime::beginWorker() noexcept {
    const auto worker = xTaskGetCurrentTaskHandle();
    if (attempted_)
        return worker_ && worker_ == worker;
    attempted_ = true;
    if (!worker || !boot_.valid())
        return false;
    GattsSourceRegistration registration;
    if (!source_.readRegistration(registration))
        return false;
    worker_ = worker;
    return true;
}
bool FreeRtosPeerProofGrantRuntime::originalLive(const PeerProofBinding &caller) const noexcept {
    const auto value = caller;
    return value.boot == boot_ && source_.closedPeerConnectionCurrent(value);
}
bool FreeRtosPeerProofGrantRuntime::credentialCurrent(const PeerCredential &caller) const noexcept {
    const auto value = caller;
    return registry_.credentialCurrent(value);
}
PeerProofGrantState
FreeRtosPeerProofGrantRuntime::submitVerified(PeerProofOwner &owner,
                                              const PeerProofBinding &caller) noexcept {
    const auto original = caller;
    if (!worker_ || worker_ != xTaskGetCurrentTaskHandle() || busy_)
        return PeerProofGrantState::Unavailable;
    busy_ = true;
    VerifiedPeerProof proof;
    PeerProofGrantState result = PeerProofGrantState::Rejected;
    if (owner.readVerified(original, proof) && originalLive(original) &&
        credentialCurrent(proof.credential)) {
        if (hasJob_ && job_.proof.original == proof.original &&
            job_.proof.sequence == proof.sequence && job_.proof.credential == proof.credential &&
            job_.proof.issuedAt == proof.issuedAt && job_.proof.verifiedAt == proof.verifiedAt) {
            const auto posted = source_.postPeerProofGrant(job_);
            if (posted == PeerProofGrantPost::Duplicate)
                result = PeerProofGrantState::Pending;
        } else if (sequence_ != UINT32_MAX) {
            PeerProofGrantJob next;
            next.proof = proof;
            next.sequence = sequence_ + 1;
            next.receivedAt = clock_.nowMilliseconds();
            if (source_.readPeerProofGrantOrigin(original, next.original) &&
                validPeerProofGrantJob(next)) {
                const auto posted = source_.postPeerProofGrant(next);
                if (posted != PeerProofGrantPost::Busy) {
                    // A rejected post may already occupy the Source slot. Consume
                    // the sequence and retain its original for exact cancellation.
                    sequence_ = next.sequence;
                    job_ = next;
                    hasJob_ = true;
                    if (posted == PeerProofGrantPost::Posted ||
                        posted == PeerProofGrantPost::Duplicate)
                        result = PeerProofGrantState::Pending;
                    else
                        source_.cancelPeerProofGrant(next);
                }
            }
        }
    }
    busy_ = false;
    return result;
}
PeerProofGrantState FreeRtosPeerProofGrantRuntime::service(TrustedAuthority &out) noexcept {
    if (!worker_ || worker_ != xTaskGetCurrentTaskHandle() || busy_ || !hasJob_)
        return PeerProofGrantState::Unavailable;
    busy_ = true;
    const auto result = source_.peerProofGrantState(job_, out);
    if (result == PeerProofGrantState::Rejected || result == PeerProofGrantState::Unavailable)
        source_.cancelPeerProofGrant(job_);
    busy_ = false;
    return result;
}
void FreeRtosPeerProofGrantRuntime::cancel() noexcept {
    if (worker_ == xTaskGetCurrentTaskHandle() && !busy_ && hasJob_)
        source_.cancelPeerProofGrant(job_);
}
bool FreeRtosPeerProofGrantRuntime::job(PeerProofGrantJob &out) const noexcept {
    if (!hasJob_ || worker_ != xTaskGetCurrentTaskHandle())
        return false;
    out = job_;
    return true;
}
} // namespace companion
} // namespace larp
