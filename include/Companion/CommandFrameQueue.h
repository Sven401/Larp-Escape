#pragma once
#include "Companion/CommandStream.h"
#include <array>
namespace larp {
namespace companion {
struct CommandFrameQueueLimits {
    static const std::size_t Depth = 4;
};
struct CopiedCommandFrame {
    std::uint32_t principal = 0, epoch = 0, receivedAt = 0;
    std::uint16_t count = 0;
    std::array<char, CommandFrameLimits::Frame> bytes = {};
};
enum class FrameCopyOutcome : std::uint8_t {
    Copied,
    QueueFull,
    Unauthorized,
    InvalidWrite,
    UnsupportedFrame,
    Uninitialized
};
class ICommandFrameQueue {
  public:
    virtual ~ICommandFrameQueue() = default;
    virtual bool take(CopiedCommandFrame &frame) noexcept = 0;
};
struct FrameDispatchResult {
    bool received = false;
    CommandStreamResult stream;
};
// Sole stream owner polls even with an empty FIFO and handles at most one copied
// frame. receivedAt/now are trusted values of the same monotonic uint32 ms clock.
// This never retries QueueFull or runs an application/game tick.
FrameDispatchResult dispatchCommandFrame(ICommandFrameQueue &queue, CommandStream &stream,
                                         std::uint32_t now) noexcept;
} // namespace companion
} // namespace larp
