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
    result.principal = frame.principal;
    result.epoch = frame.epoch;
    CommandFrame decoded;
    if (decodeCommandFrame(frame.bytes.data(), frame.count, decoded) ==
        FrameDecodeResult::Success) {
        result.input.message = decoded.message;
        result.input.total = decoded.total;
        result.input.offset = decoded.offset;
        result.input.count = static_cast<std::uint16_t>(decoded.count);
    }
    result.stream = stream.receive(frame.bytes.data(), frame.count, frame.principal, frame.epoch,
                                   frame.receivedAt, now);
    return result;
}
} // namespace companion
} // namespace larp
