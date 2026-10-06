#include "Companion/CommandFrameQueue.h"
namespace larp {
namespace companion {
FrameDispatchResult dispatchCommandFrame(ICommandFrameQueue &queue, CommandStream &stream,
                                         std::uint32_t now) noexcept {
    FrameDispatchResult result;
    CopiedCommandFrame frame;
    if (!queue.take(frame)) {
        result.stream = stream.poll(now);
        return result;
    }
    result.received = true;
    result.stream = stream.receive(frame.bytes.data(), frame.count, frame.principal, frame.epoch,
                                   frame.receivedAt, now);
    return result;
}
} // namespace companion
} // namespace larp
