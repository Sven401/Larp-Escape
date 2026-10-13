#pragma once
#include "Companion/CommandFrame.h"
namespace larp {
namespace companion {
// Identical bounded header layout, separate kind and JSON length bound.
// The command decoder continues rejecting kind 2.
using ReplyFrame = CommandFrame;
struct ReplyFrameLimits {
    static const std::uint8_t Kind = 2;
    static const std::size_t Document = maxGameReplyJsonBytes - 1;
};
FrameDecodeResult decodeReplyFrame(const char *bytes, std::size_t count,
                                   ReplyFrame &destination) noexcept;
bool encodeReplyFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                      const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                      std::size_t &written) noexcept;
} // namespace companion
} // namespace larp
