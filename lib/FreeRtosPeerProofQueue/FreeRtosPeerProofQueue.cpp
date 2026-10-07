#include "FreeRtosPeerProofQueue.h"
#include <cstring>
namespace larp {
namespace companion {
bool FreeRtosPeerProofQueue::begin() noexcept {
    if (!queue_)
        queue_ = xQueueCreateStatic(PeerProofQueueLimits::Depth, sizeof(CopiedPeerProofFrame),
                                    storage_, &control_);
    return queue_ != nullptr;
}
PeerProofCopy FreeRtosPeerProofQueue::copyWrite(const PeerProofBinding &caller, const char *bytes,
                                                std::size_t count, std::uint32_t receivedAt,
                                                bool prepared, std::uint16_t offset) noexcept {
    CopiedPeerProofFrame value;
    value.original = caller;
    value.receivedAt = receivedAt;
    if (!queue_)
        return PeerProofCopy::Unavailable;
    if (!bytes || count <= CommandFrameLimits::Header || count > value.bytes.size() || prepared ||
        offset)
        return PeerProofCopy::Invalid;
    value.count = static_cast<std::uint16_t>(count);
    std::memcpy(value.bytes.data(), bytes, count);
    CommandFrame frame;
    if (decodePeerProofFrame(value.bytes.data(), count, frame) != FrameDecodeResult::Success ||
        frame.total != PeerProofLimits::ProofBytes)
        return PeerProofCopy::Invalid;
    if (clock_.nowMilliseconds() - value.receivedAt >= PeerProofQueueLimits::MaxAgeMilliseconds)
        return PeerProofCopy::Expired;
    if (!value.original.valid() || !context_.originalLive(value.original))
        return PeerProofCopy::Stale;
    return xQueueSendToBack(queue_, &value, 0) == pdTRUE ? PeerProofCopy::Copied
                                                         : PeerProofCopy::Full;
}
bool FreeRtosPeerProofQueue::takeCurrent(CopiedPeerProofFrame &destination) noexcept {
    if (!queue_ || taking_)
        return false;
    taking_ = true;
    CopiedPeerProofFrame value;
    bool found = false;
    for (std::size_t n = 0; n < PeerProofQueueLimits::Depth; ++n) {
        if (xQueueReceive(queue_, &value, 0) != pdTRUE)
            break;
        if (clock_.nowMilliseconds() - value.receivedAt <
                PeerProofQueueLimits::MaxAgeMilliseconds &&
            context_.originalLive(value.original)) {
            destination = value;
            found = true;
            break;
        }
    }
    taking_ = false;
    return found;
}
PeerProofDispatch dispatchPeerProofFrame(FreeRtosPeerProofQueue &queue,
                                         PeerProofReceiver &receiver) noexcept {
    PeerProofDispatch result;
    CopiedPeerProofFrame frame;
    if (queue.takeCurrent(frame)) {
        result.received = true;
        result.result = receiver.receive(frame.original, frame.bytes.data(), frame.count);
    }
    return result;
}
} // namespace companion
} // namespace larp
