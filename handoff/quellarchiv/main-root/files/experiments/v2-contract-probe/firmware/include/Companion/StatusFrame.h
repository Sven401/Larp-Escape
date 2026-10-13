#pragma once
#include "Companion/CommandFrame.h"
namespace larp {
namespace companion {
using StatusFrame = CommandFrame;
struct StatusFrameLimits {
    static const std::uint8_t Kind = 4;
    static const std::size_t Document = maxConfiguredStatusJsonBytes - 1;
    // Policy bounds, not measured RF throughput. Different from command expiry.
    static const std::uint32_t TimeoutMilliseconds = 30000;
    static const std::uint32_t FreshMilliseconds = 1000;
};
FrameDecodeResult decodeStatusFrame(const char *bytes, std::size_t count,
                                    StatusFrame &destination) noexcept;
bool encodeStatusFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                       const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                       std::size_t &written) noexcept;
} // namespace companion
} // namespace larp
