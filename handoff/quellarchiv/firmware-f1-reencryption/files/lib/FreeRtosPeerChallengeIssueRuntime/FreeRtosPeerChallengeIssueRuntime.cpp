#include "FreeRtosPeerChallengeIssueRuntime.h"
namespace larp {
namespace companion {
bool FreeRtosPeerChallengeIssueRuntime::workerNow() const noexcept {
    return queue_ && worker_ && worker_ == xTaskGetCurrentTaskHandle() && grant_.workerNow();
}
bool FreeRtosPeerChallengeIssueRuntime::beginWorker() noexcept {
    if (attempted_)
        return workerNow();
    attempted_ = true;
    const auto task = xTaskGetCurrentTaskHandle();
    if (!task || !grant_.workerNow())
        return false;
    worker_ = task;
    queue_ = xQueueCreateStatic(PeerChallengeIssueLimits::Depth, sizeof(PeerChallengeIssueRequest),
                                storage_, &control_);
    return queue_ != nullptr;
}
bool FreeRtosPeerChallengeIssueRuntime::current(
    const PeerChallengeIssueRequest &value) const noexcept {
    return clock_.nowMilliseconds() - value.scope.receivedAt <
               PeerChallengeIssueLimits::MaxAgeMilliseconds &&
           source_.challengeIssueCurrent(value) &&
           clock_.nowMilliseconds() - value.scope.receivedAt <
               PeerChallengeIssueLimits::MaxAgeMilliseconds;
}
bool FreeRtosPeerChallengeIssueRuntime::copyIssue(
    const PeerChallengeIssueRequest &caller) noexcept {
    const auto value = caller;
    return queue_ && source_.challengeIssueProducerNow(value) && current(value) &&
           source_.challengeIssueProducerNow(value) &&
           xQueueSendToBack(queue_, &value, 0) == pdTRUE;
}
PeerChallengeIssueResult FreeRtosPeerChallengeIssueRuntime::service() noexcept {
    PeerChallengeIssueResult result;
    if (!workerNow() || taking_) {
        result.dispatch = PeerChallengeIssueDispatch::Unavailable;
        return result;
    }
    taking_ = true;
    PeerChallengeIssueRequest value;
    for (std::size_t n = 0; n < PeerChallengeIssueLimits::Depth; ++n) {
        if (xQueueReceive(queue_, &value, 0) != pdTRUE)
            break;
        if (!current(value))
            continue;
        result.dispatch = PeerChallengeIssueDispatch::Processed;
        Identity peer;
        PeerCredential credential;
        if (decodePeerChallengeIssue(value.bytes.data(), value.bytes.size(), peer) &&
            registry_.credential(peer, credential) && current(value))
            result.outcome = owner_.issue(value.scope.original, credential);
        taking_ = false;
        return result;
    }
    taking_ = false;
    return result;
}
} // namespace companion
} // namespace larp
