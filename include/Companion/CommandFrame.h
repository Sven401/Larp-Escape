#pragma once
#include "Companion/GameCommandReader.h"
namespace larp {
namespace companion {
struct CommandFrameLimits {
    static const std::uint8_t Version = 1, Kind = 1;
    static const std::size_t Header = 8, Payload = 120, Frame = Header + Payload;
    static const std::uint32_t TimeoutMilliseconds = 2000;
};
// Borrowed immutable payload; valid only while the input buffer is unchanged.
struct CommandFrame {
    std::uint16_t message = 0, total = 0, offset = 0;
    const char *payload = nullptr;
    std::size_t count = 0;
};
enum class FrameDecodeResult : std::uint8_t { Success, Invalid, Unsupported };
// Header: version=1, kind=1 (GameCommand v2), message/total/offset as LE uint16.
// No packed C++ struct, padding or host byte order is used on the wire.
// Failed decode preserves destination. Source/destination buffers must not overlap.
FrameDecodeResult decodeCommandFrame(const char *bytes, std::size_t count,
                                     CommandFrame &destination) noexcept;
bool encodeCommandFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                        const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                        std::size_t &written) noexcept;
} // namespace companion
} // namespace larp
