#include "Esp32GattsSource.h"
#include <cstring>
namespace larp {
namespace companion {
bool Esp32GattsSource::configurePeerAuthNow(std::uint16_t proof, std::uint16_t result,
                                            IPeerAuthIngress &ingress) noexcept {
    if (!sourceTask() || !owner_ || sawConnection_ || !grantConfigured_ || !issueAttribute_ ||
        !challengeAttribute_ || proofAttribute_ || resultAttribute_ || !proof || !result ||
        proof == result || proof == command_ || proof == profile_ || proof == challengeAttribute_ ||
        proof == issueAttribute_ || result == command_ || result == profile_ ||
        result == challengeAttribute_ || result == issueAttribute_)
        return false;
    portENTER_CRITICAL(&mutex_);
    proofAttribute_ = proof;
    resultAttribute_ = result;
    authIngress_ = &ingress;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::authScopeCurrent(const PeerChallengeReadRequest &caller,
                                        bool proof) const noexcept {
    const auto v = caller;
    portENTER_CRITICAL(&mutex_);
    const bool bound = authIngress_ &&
                       v.attribute == (proof ? proofAttribute_ : resultAttribute_) &&
                       proofAttribute_ && resultAttribute_;
    const auto challenge = challengeAttribute_;
    portEXIT_CRITICAL(&mutex_);
    auto scope = v;
    scope.attribute = challenge;
    return bound && (!proof || (!v.offset && !v.isLong)) && challengeReadCurrent(scope);
}
bool Esp32GattsSource::authProducerNow(const PeerChallengeReadRequest &caller,
                                       bool proof) const noexcept {
    const auto v = caller;
    auto scope = v;
    portENTER_CRITICAL(&mutex_);
    scope.attribute = challengeAttribute_;
    portEXIT_CRITICAL(&mutex_);
    return authScopeCurrent(v, proof) && challengeReadProducerNow(scope) &&
           authScopeCurrent(v, proof);
}
bool Esp32GattsSource::peerProofWriteCurrent(const PeerProofWriteRequest &caller) const noexcept {
    const auto v = caller;
    return v.count > CommandFrameLimits::Header && v.count <= v.bytes.size() &&
           authScopeCurrent(v.scope, true);
}
bool Esp32GattsSource::peerProofWriteProducerNow(
    const PeerProofWriteRequest &caller) const noexcept {
    const auto v = caller;
    return peerProofWriteCurrent(v) && authProducerNow(v.scope, true) && peerProofWriteCurrent(v);
}
bool Esp32GattsSource::peerAuthReadCurrent(const PeerChallengeReadRequest &caller) const noexcept {
    return authScopeCurrent(caller, false);
}
bool Esp32GattsSource::peerAuthReadProducerNow(
    const PeerChallengeReadRequest &caller) const noexcept {
    return authProducerNow(caller, false);
}
bool Esp32GattsSource::authAckCurrent(const PeerAuthReadReply &v) const noexcept {
    const auto &job = v.job;
    const auto &ack = v.ack;
    const auto matches = [&]() {
        portENTER_CRITICAL(&mutex_);
        const bool current = grant_.state == 6 && grant_.value == job && grant_.ack == ack &&
                             identitySnapshot_.authority == ack &&
                             identitySnapshot_.connection == job.proof.original.connection &&
                             lifecycle_ == job.original.lifecycle && peerProofOpen_ &&
                             !registryFenced_;
        portEXIT_CRITICAL(&mutex_);
        return current;
    };
    TrustedAuthority observed;
    return validPeerProofGrantJob(job) && job.proof.original == v.request.original &&
           job.proof.credential == v.snapshot.credential &&
           job.proof.sequence == v.result.sequence && job.proof.issuedAt == v.snapshot.issuedAt &&
           ack.epoch && ack.gameMaster == (v.result.state == PeerAuthState::Granted) &&
           ack.principal == v.result.principal && matches() && channel_.readAuthority(observed) &&
           observed == ack && matches();
}
bool Esp32GattsSource::validAuthReply(const PeerAuthReadReply &v) const noexcept {
    const auto fresh = [&]() {
        return clock_.nowMilliseconds() - v.request.receivedAt <
                   PeerAuthLimits::MaxAgeMilliseconds &&
               peerAuthReadCurrent(v.request) &&
               clock_.nowMilliseconds() - v.request.receivedAt <
                   PeerAuthLimits::MaxAgeMilliseconds &&
               peerAuthReadCurrent(v.request);
    };
    if (!fresh())
        return false;
    if (v.status == 0) {
        std::array<std::uint8_t, 32> bytes;
        if ((!v.request.isLong && v.request.offset) || v.request.offset > bytes.size() ||
            !encodePeerAuthResult(v.result, bytes) ||
            !(v.snapshot.challenge.original == v.request.original) ||
            !(v.snapshot.challenge.peer == v.snapshot.credential.peer) ||
            !(v.result.peer == v.snapshot.credential.peer) ||
            v.result.sequence != v.snapshot.challenge.sequence || !proofContext_ ||
            !proofContext_->credentialCurrent(v.snapshot.credential) ||
            clock_.nowMilliseconds() - v.snapshot.issuedAt >= PeerProofLimits::MaxAgeMilliseconds ||
            clock_.nowMilliseconds() - v.frozenAt >= PeerAuthLimits::MaxAgeMilliseconds)
            return false;
        if ((v.result.state == PeerAuthState::Granted ||
             v.result.state == PeerAuthState::Authenticated) &&
            !authAckCurrent(v))
            return false;
        return fresh() && proofContext_->credentialCurrent(v.snapshot.credential) &&
               clock_.nowMilliseconds() - v.snapshot.issuedAt <
                   PeerProofLimits::MaxAgeMilliseconds &&
               clock_.nowMilliseconds() - v.frozenAt < PeerAuthLimits::MaxAgeMilliseconds &&
               peerAuthReadCurrent(v.request) &&
               ((v.result.state != PeerAuthState::Granted &&
                 v.result.state != PeerAuthState::Authenticated) ||
                authAckCurrent(v));
    }
    return (v.status == 7 || v.status == 8) && fresh();
}
} // namespace companion
} // namespace larp
