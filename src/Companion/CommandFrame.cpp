#include "Companion/CommandFrame.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
bool valid(std::uint16_t message, std::uint16_t total, std::uint16_t offset, std::size_t count) {
    return message && total && total <= maxGameCommandDocumentBytes && count &&
           count <= CommandFrameLimits::Payload && offset < total &&
           count <= static_cast<std::size_t>(total - offset);
}
std::uint16_t read16(const char *bytes) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1]) << 8);
}
void write16(char *bytes, std::uint16_t value) {
    bytes[0] = static_cast<char>(value & 0xff);
    bytes[1] = static_cast<char>(value >> 8);
}
} // namespace
FrameDecodeResult decodeCommandFrame(const char *bytes, std::size_t count,
                                     CommandFrame &destination) noexcept {
    if (!bytes || count <= CommandFrameLimits::Header || count > CommandFrameLimits::Frame)
        return FrameDecodeResult::Invalid;
    if (static_cast<unsigned char>(bytes[0]) != CommandFrameLimits::Version ||
        static_cast<unsigned char>(bytes[1]) != CommandFrameLimits::Kind)
        return FrameDecodeResult::Unsupported;
    CommandFrame candidate;
    candidate.message = read16(bytes + 2);
    candidate.total = read16(bytes + 4);
    candidate.offset = read16(bytes + 6);
    candidate.payload = bytes + CommandFrameLimits::Header;
    candidate.count = count - CommandFrameLimits::Header;
    if (!valid(candidate.message, candidate.total, candidate.offset, candidate.count))
        return FrameDecodeResult::Invalid;
    destination = candidate;
    return FrameDecodeResult::Success;
}
bool encodeCommandFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                        const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                        std::size_t &written) noexcept {
    written = 0;
    if (bytes && capacity)
        bytes[0] = 0;
    if (!bytes || !payload || !valid(message, total, offset, count) ||
        capacity < CommandFrameLimits::Header + count)
        return false;
    bytes[0] = CommandFrameLimits::Version;
    bytes[1] = CommandFrameLimits::Kind;
    write16(bytes + 2, message);
    write16(bytes + 4, total);
    write16(bytes + 6, offset);
    std::memcpy(bytes + CommandFrameLimits::Header, payload, count);
    written = CommandFrameLimits::Header + count;
    return true;
}
} // namespace companion
} // namespace larp
