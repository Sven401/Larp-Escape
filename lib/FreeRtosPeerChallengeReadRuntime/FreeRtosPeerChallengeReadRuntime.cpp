#include "FreeRtosPeerChallengeReadRuntime.h"
namespace larp {
namespace companion {
bool FreeRtosPeerChallengeReadRuntime::beginWorker() noexcept {
    const auto task = xTaskGetCurrentTaskHandle();
    if (attempted_)
        return workerNow() && queue_;
    attempted_ = true;
    if (!task || !grant_.workerNow())
        return false;
    worker_ = task;
    queue_ = xQueueCreateStatic(PeerChallengeReadLimits::Depth, sizeof(PeerChallengeReadRequest),
                                storage_, &control_);
    return queue_ != nullptr;
}
bool FreeRtosPeerChallengeReadRuntime::workerNow() const noexcept {
    return worker_ && worker_ == xTaskGetCurrentTaskHandle() && queue_ && grant_.workerNow();
}
bool FreeRtosPeerChallengeReadRuntime::current(
    const PeerChallengeReadRequest &value) const noexcept {
    return clock_.nowMilliseconds() - value.receivedAt <
               PeerChallengeReadLimits::MaxAgeMilliseconds &&
           source_.challengeReadCurrent(value) &&
           clock_.nowMilliseconds() - value.receivedAt <
               PeerChallengeReadLimits::MaxAgeMilliseconds;
}
bool FreeRtosPeerChallengeReadRuntime::copyRead(const PeerChallengeReadRequest &caller) noexcept {
    const auto value = caller;
    return queue_ && source_.challengeReadProducerNow(value) && current(value) &&
           source_.challengeReadProducerNow(value) && xQueueSendToBack(queue_, &value, 0) == pdTRUE;
}
PeerChallengeReadDispatch FreeRtosPeerChallengeReadRuntime::service() noexcept {
    if (!workerNow() || taking_)
        return PeerChallengeReadDispatch::Unavailable;
    taking_ = true;
    PeerChallengeReadReply reply;
    for (std::size_t i = 0; i < PeerChallengeReadLimits::Depth; ++i) {
        if (xQueueReceive(queue_, &reply.request, 0) != pdTRUE)
            break;
        if (!current(reply.request))
            continue;
        if ((!reply.request.isLong && reply.request.offset) ||
            reply.request.offset > PeerProofLimits::ChallengeBytes)
            reply.status = 7;
        else if (owner_.challengeSnapshot(reply.request.original, reply.snapshot))
            reply.status = 0;
        const bool posted = current(reply.request) && source_.postChallengeReadReply(reply);
        taking_ = false;
        return posted ? PeerChallengeReadDispatch::Posted : PeerChallengeReadDispatch::Rejected;
    }
    taking_ = false;
    return PeerChallengeReadDispatch::Empty;
}
} // namespace companion
} // namespace larp
