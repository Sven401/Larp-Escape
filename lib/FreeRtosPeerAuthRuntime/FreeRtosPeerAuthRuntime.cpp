#include "FreeRtosPeerAuthRuntime.h"
namespace larp {
namespace companion {
bool FreeRtosPeerAuthRuntime::workerNow() const noexcept {
    return proofs_ && reads_ && worker_ && worker_ == xTaskGetCurrentTaskHandle() &&
           grant_.workerNow();
}
bool FreeRtosPeerAuthRuntime::beginWorker() noexcept {
    if (attempted_)
        return workerNow();
    attempted_ = true;
    const auto task = xTaskGetCurrentTaskHandle();
    if (!task || !grant_.workerNow())
        return false;
    worker_ = task;
    proofs_ = xQueueCreateStatic(PeerAuthLimits::Depth, sizeof(PeerProofWriteRequest),
                                 proofStorage_, &proofControl_);
    reads_ = xQueueCreateStatic(PeerAuthLimits::Depth, sizeof(PeerChallengeReadRequest),
                                readStorage_, &readControl_);
    return proofs_ && reads_;
}
bool FreeRtosPeerAuthRuntime::current(const PeerProofWriteRequest &v) const noexcept {
    return clock_.nowMilliseconds() - v.scope.receivedAt < PeerAuthLimits::MaxAgeMilliseconds &&
           source_.peerProofWriteCurrent(v) &&
           clock_.nowMilliseconds() - v.scope.receivedAt < PeerAuthLimits::MaxAgeMilliseconds;
}
bool FreeRtosPeerAuthRuntime::current(const PeerChallengeReadRequest &v) const noexcept {
    return clock_.nowMilliseconds() - v.receivedAt < PeerAuthLimits::MaxAgeMilliseconds &&
           source_.peerAuthReadCurrent(v) &&
           clock_.nowMilliseconds() - v.receivedAt < PeerAuthLimits::MaxAgeMilliseconds;
}
bool FreeRtosPeerAuthRuntime::copyProof(const PeerProofWriteRequest &caller) noexcept {
    const auto v = caller;
    return proofs_ && source_.peerProofWriteProducerNow(v) && current(v) &&
           source_.peerProofWriteProducerNow(v) && xQueueSendToBack(proofs_, &v, 0) == pdTRUE;
}
bool FreeRtosPeerAuthRuntime::copyResultRead(const PeerChallengeReadRequest &caller) noexcept {
    const auto v = caller;
    return reads_ && source_.peerAuthReadProducerNow(v) && current(v) &&
           source_.peerAuthReadProducerNow(v) && xQueueSendToBack(reads_, &v, 0) == pdTRUE;
}
bool FreeRtosPeerAuthRuntime::resultCurrent(
    const PeerAuthReadReply &v, const PeerChallengeReadRequest &request) const noexcept {
    return v.snapshot.challenge.original == request.original &&
           clock_.nowMilliseconds() - v.snapshot.issuedAt < PeerProofLimits::MaxAgeMilliseconds &&
           grant_.credentialCurrent(v.snapshot.credential) && current(request) &&
           clock_.nowMilliseconds() - v.snapshot.issuedAt < PeerProofLimits::MaxAgeMilliseconds;
}
PeerAuthDispatch FreeRtosPeerAuthRuntime::service() noexcept {
    if (!workerNow() || taking_)
        return PeerAuthDispatch::Unavailable;
    taking_ = true;
    if (waiting_) {
        TrustedAuthority ack;
        const auto state = grant_.service(ack);
        if (state != PeerProofGrantState::Pending) {
            waiting_ = false;
            terminal_ = true;
            latest_.result.state = PeerAuthState::Rejected;
            latest_.result.reason = PeerAuthReason::GrantRejected;
            if (state == PeerProofGrantState::Confirmed && grant_.job(latest_.job) &&
                latest_.job.proof.original == latest_.snapshot.challenge.original &&
                latest_.job.proof.sequence == latest_.result.sequence) {
                latest_.ack = ack;
                latest_.result.reason = PeerAuthReason::None;
                latest_.result.principal = ack.principal;
                latest_.result.state =
                    ack.gameMaster ? PeerAuthState::Granted : PeerAuthState::Authenticated;
            }
            taking_ = false;
            return PeerAuthDispatch::GrantObserved;
        }
    }
    PeerProofWriteRequest value;
    for (std::size_t n = 0; n < PeerAuthLimits::Depth; ++n) {
        if (xQueueReceive(proofs_, &value, 0) != pdTRUE)
            break;
        if (!current(value))
            continue;
        if (haveResult_ && latest_.snapshot.challenge.original == value.scope.original &&
            (waiting_ || terminal_)) {
            taking_ = false;
            return PeerAuthDispatch::ProofProcessed;
        }
        PeerChallengeSnapshot snapshot;
        CommandFrame frame;
        if (!owner_.challengeSnapshot(value.scope.original, snapshot) ||
            decodePeerProofFrame(value.bytes.data(), value.count, frame) !=
                FrameDecodeResult::Success ||
            frame.total != PeerProofLimits::ProofBytes || !current(value)) {
            taking_ = false;
            return PeerAuthDispatch::Rejected;
        }
        const auto received =
            receiver_.receive(value.scope.original, value.bytes.data(), value.count);
        latest_ = {};
        latest_.snapshot = snapshot;
        latest_.result.peer = snapshot.credential.peer;
        latest_.result.sequence = snapshot.challenge.sequence;
        latest_.result.message = frame.message;
        latest_.result.received = received.received;
        haveResult_ = true;
        terminal_ = waiting_ = false;
        if (received.kind == PeerProofReceiveKind::Receiving ||
            received.kind == PeerProofReceiveKind::DuplicateFragment) {
            latest_.result.state = PeerAuthState::Receiving;
            latest_.result.reason = PeerAuthReason::None;
        } else if (received.kind == PeerProofReceiveKind::Completed) {
            latest_.result.reason = PeerAuthReason::ProofRejected;
            if (received.proof == PeerProofOutcome::Verified ||
                received.proof == PeerProofOutcome::Duplicate) {
                const auto submitted = grant_.submitVerified(owner_, value.scope.original);
                latest_.result.reason = PeerAuthReason::GrantRejected;
                if (submitted == PeerProofGrantState::Pending) {
                    latest_.result.state = PeerAuthState::GrantPending;
                    latest_.result.reason = PeerAuthReason::None;
                    waiting_ = true;
                } else
                    terminal_ = true;
            }
        }
        taking_ = false;
        return PeerAuthDispatch::ProofProcessed;
    }
    PeerChallengeReadRequest request;
    for (std::size_t n = 0; n < PeerAuthLimits::Depth; ++n) {
        if (xQueueReceive(reads_, &request, 0) != pdTRUE)
            break;
        if (!current(request))
            continue;
        PeerAuthReadReply reply;
        if ((!request.isLong && request.offset) || request.offset > PeerAuthLimits::Bytes)
            reply.status = 7;
        else if (!request.isLong) {
            havePin_ = false;
            if (haveResult_ && resultCurrent(latest_, request)) {
                pin_ = latest_;
                pin_.frozenAt = clock_.nowMilliseconds();
                havePin_ = true;
            }
        }
        if (reply.status != 7 && havePin_ && resultCurrent(pin_, request) &&
            clock_.nowMilliseconds() - pin_.frozenAt < PeerAuthLimits::MaxAgeMilliseconds) {
            reply = pin_;
            reply.status = 0;
        }
        reply.request = request;
        const bool posted = current(request) && source_.postPeerAuthReply(reply);
        taking_ = false;
        return posted ? PeerAuthDispatch::ReadPosted : PeerAuthDispatch::Rejected;
    }
    taking_ = false;
    return PeerAuthDispatch::Empty;
}
} // namespace companion
} // namespace larp
