#include "Companion/CommandFrame.h"
#include "Companion/PeerProofFrame.h"
#include "Companion/ReplyFrame.h"
#include "Companion/StatusFrame.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
bool valid(std::uint16_t message, std::uint16_t total, std::uint16_t offset, std::size_t count,
           std::size_t maximum) {
    return message && total && total <= maximum && count && count <= CommandFrameLimits::Payload &&
           offset < total && count <= static_cast<std::size_t>(total - offset);
}
std::uint16_t read16(const char *bytes) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1]) << 8);
}
void write16(char *bytes, std::uint16_t value) {
    bytes[0] = static_cast<char>(value & 0xff);
    bytes[1] = static_cast<char>(value >> 8);
}
FrameDecodeResult decodeFrame(std::uint8_t kind, std::size_t maximum, const char *bytes,
                              std::size_t count, CommandFrame &destination) noexcept {
    if (!bytes || count <= CommandFrameLimits::Header || count > CommandFrameLimits::Frame)
        return FrameDecodeResult::Invalid;
    if (static_cast<unsigned char>(bytes[0]) != CommandFrameLimits::Version ||
        static_cast<unsigned char>(bytes[1]) != kind)
        return FrameDecodeResult::Unsupported;
    CommandFrame candidate;
    candidate.message = read16(bytes + 2);
    candidate.total = read16(bytes + 4);
    candidate.offset = read16(bytes + 6);
    candidate.payload = bytes + CommandFrameLimits::Header;
    candidate.count = count - CommandFrameLimits::Header;
    if (!valid(candidate.message, candidate.total, candidate.offset, candidate.count, maximum))
        return FrameDecodeResult::Invalid;
    destination = candidate;
    return FrameDecodeResult::Success;
}
FrameDecodeResult decodeFixedPeerFrame(std::uint8_t kind, std::size_t exact, const char *bytes,
                                       std::size_t count, CommandFrame &destination) noexcept {
    CommandFrame candidate;
    const auto decoded = decodeFrame(kind, exact, bytes, count, candidate);
    if (decoded != FrameDecodeResult::Success)
        return decoded;
    if (candidate.total != exact)
        return FrameDecodeResult::Invalid;
    destination = candidate;
    return FrameDecodeResult::Success;
}
bool encodeFrame(std::uint8_t kind, std::size_t maximum, std::uint16_t message, std::uint16_t total,
                 std::uint16_t offset, const char *payload, std::size_t count, char *bytes,
                 std::size_t capacity, std::size_t &written) noexcept {
    written = 0;
    if (bytes && capacity)
        bytes[0] = 0;
    if (!bytes || !payload || !valid(message, total, offset, count, maximum) ||
        capacity < CommandFrameLimits::Header + count)
        return false;
    bytes[0] = CommandFrameLimits::Version;
    bytes[1] = kind;
    write16(bytes + 2, message);
    write16(bytes + 4, total);
    write16(bytes + 6, offset);
    std::memcpy(bytes + CommandFrameLimits::Header, payload, count);
    written = CommandFrameLimits::Header + count;
    return true;
}
} // namespace
FrameDecodeResult decodeCommandFrame(const char *bytes, std::size_t count,
                                     CommandFrame &destination) noexcept {
    return decodeFrame(CommandFrameLimits::Kind, maxGameCommandDocumentBytes, bytes, count,
                       destination);
}
bool encodeCommandFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                        const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                        std::size_t &written) noexcept {
    return encodeFrame(CommandFrameLimits::Kind, maxGameCommandDocumentBytes, message, total,
                       offset, payload, count, bytes, capacity, written);
}
FrameDecodeResult decodeReplyFrame(const char *bytes, std::size_t count,
                                   ReplyFrame &destination) noexcept {
    return decodeFrame(ReplyFrameLimits::Kind, ReplyFrameLimits::Document, bytes, count,
                       destination);
}
bool encodeReplyFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                      const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                      std::size_t &written) noexcept {
    return encodeFrame(ReplyFrameLimits::Kind, ReplyFrameLimits::Document, message, total, offset,
                       payload, count, bytes, capacity, written);
}
FrameDecodeResult decodeStatusFrame(const char *bytes, std::size_t count,
                                    StatusFrame &destination) noexcept {
    return decodeFrame(StatusFrameLimits::Kind, StatusFrameLimits::Document, bytes, count,
                       destination);
}
bool encodeStatusFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                       const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                       std::size_t &written) noexcept {
    return encodeFrame(StatusFrameLimits::Kind, StatusFrameLimits::Document, message, total, offset,
                       payload, count, bytes, capacity, written);
}
FrameDecodeResult decodePeerChallengeFrame(const char *bytes, std::size_t count,
                                           CommandFrame &out) noexcept {
    return decodeFixedPeerFrame(PeerProofLimits::ChallengeKind, PeerProofLimits::ChallengeBytes,
                                bytes, count, out);
}
FrameDecodeResult decodePeerProofFrame(const char *bytes, std::size_t count,
                                       CommandFrame &out) noexcept {
    return decodeFixedPeerFrame(PeerProofLimits::ProofKind, PeerProofLimits::ProofBytes, bytes,
                                count, out);
}
bool encodePeerChallengeFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                              const char *payload, std::size_t count, char *bytes,
                              std::size_t capacity, std::size_t &written) noexcept {
    return encodeFrame(PeerProofLimits::ChallengeKind, PeerProofLimits::ChallengeBytes, message,
                       total == PeerProofLimits::ChallengeBytes ? total : 0, offset, payload, count,
                       bytes, capacity, written);
}
bool encodePeerProofFrame(std::uint16_t message, std::uint16_t total, std::uint16_t offset,
                          const char *payload, std::size_t count, char *bytes, std::size_t capacity,
                          std::size_t &written) noexcept {
    return encodeFrame(PeerProofLimits::ProofKind, PeerProofLimits::ProofBytes, message,
                       total == PeerProofLimits::ProofBytes ? total : 0, offset, payload, count,
                       bytes, capacity, written);
}
} // namespace companion
} // namespace larp
