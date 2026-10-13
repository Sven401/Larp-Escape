#include "FreeRtosCommandFrameQueue.h"
#include <cstring>
namespace larp {
namespace companion {
bool FreeRtosCommandFrameQueue::begin() noexcept {
    if (!queue_)
        queue_ = xQueueCreateStatic(CommandFrameQueueLimits::Depth, sizeof(CopiedCommandFrame),
                                    storage_, &control_);
    return queue_ != nullptr;
}
FrameCopyOutcome FreeRtosCommandFrameQueue::copyWrite(const char *bytes, std::size_t count,
                                                      std::uint32_t principal, std::uint32_t epoch,
                                                      std::uint32_t receivedAt, bool prepared,
                                                      std::uint16_t writeOffset) noexcept {
    if (!queue_)
        return FrameCopyOutcome::Uninitialized;
    if (prepared || writeOffset)
        return FrameCopyOutcome::InvalidWrite;
    TrustedAuthority current;
    if (!principal || !epoch || !authority_.readAuthority(current) || !current.gameMaster ||
        current.principal != principal || current.epoch != epoch)
        return FrameCopyOutcome::Unauthorized;
    CommandFrame decoded;
    const auto validation = decodeCommandFrame(bytes, count, decoded);
    if (validation != FrameDecodeResult::Success)
        return validation == FrameDecodeResult::Unsupported ? FrameCopyOutcome::UnsupportedFrame
                                                            : FrameCopyOutcome::InvalidWrite;
    CopiedCommandFrame frame;
    frame.principal = principal;
    frame.epoch = epoch;
    frame.receivedAt = receivedAt;
    frame.count = static_cast<std::uint16_t>(count);
    std::memcpy(frame.bytes.data(), bytes, count);
    // A revoke after the authority observation may still leave an old copied
    // frame. Stream and final command admission recheck it; no lifecycle FIFO.
    return xQueueSendToBack(queue_, &frame, 0) == pdTRUE ? FrameCopyOutcome::Copied
                                                         : FrameCopyOutcome::QueueFull;
}
bool FreeRtosCommandFrameQueue::take(CopiedCommandFrame &frame) noexcept {
    return queue_ && xQueueReceive(queue_, &frame, 0) == pdTRUE;
}
} // namespace companion
} // namespace larp
