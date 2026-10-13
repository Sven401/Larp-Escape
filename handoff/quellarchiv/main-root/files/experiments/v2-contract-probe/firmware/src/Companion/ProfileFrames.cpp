#include "Companion/ProfileFrames.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
std::uint32_t get(const char *bytes, unsigned count) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << (i * 8);
    return value;
}
void put(char *bytes, unsigned count, std::uint32_t value) {
    for (unsigned i = 0; i < count; ++i)
        bytes[i] = static_cast<char>(value >> (i * 8));
}
} // namespace
bool ProfileDataFrame::valid() const noexcept {
    return transfer && payload && count && count <= ProfileFrameLimits::PayloadBytes &&
           offset < configuration::ImportLimits::DocumentBytes &&
           count <= configuration::ImportLimits::DocumentBytes - offset;
}
bool decodeProfileDataFrame(const char *bytes, std::size_t count,
                            ProfileDataFrame &destination) noexcept {
    if (!bytes || count <= ProfileFrameLimits::HeaderBytes ||
        count > ProfileFrameLimits::ValueBytes ||
        static_cast<unsigned char>(bytes[0]) != ProfileFrameLimits::Version ||
        static_cast<unsigned char>(bytes[1]) != ProfileFrameLimits::Kind)
        return false;
    ProfileDataFrame value;
    value.transfer = static_cast<std::uint16_t>(get(bytes + 2, 2));
    value.offset = get(bytes + 4, 4);
    value.payload = bytes + ProfileFrameLimits::HeaderBytes;
    value.count = count - ProfileFrameLimits::HeaderBytes;
    if (!value.valid())
        return false;
    destination = value;
    return true;
}
bool encodeProfileDataFrame(const ProfileDataFrame &frame, char *bytes, std::size_t capacity,
                            std::size_t &written) noexcept {
    written = 0;
    if (!bytes || !frame.valid() || capacity < ProfileFrameLimits::HeaderBytes + frame.count)
        return false;
    // Move payload first so in-place framing also preserves overlapping input.
    std::memmove(bytes + ProfileFrameLimits::HeaderBytes, frame.payload, frame.count);
    bytes[0] = ProfileFrameLimits::Version;
    bytes[1] = ProfileFrameLimits::Kind;
    put(bytes + 2, 2, frame.transfer);
    put(bytes + 4, 4, frame.offset);
    written = ProfileFrameLimits::HeaderBytes + frame.count;
    return true;
}
FramedProfileReader::FramedProfileReader(ISealedProfileFrames &source,
                                         const ProfileSourceEnvelope &envelope) noexcept
    : source_(source), envelope_(envelope) {
    if (!envelope_.valid())
        terminal_ = configuration::ReadStatus::Failure;
}
configuration::ReadStatus FramedProfileReader::readByte(std::uint8_t &byte) {
    using configuration::ReadStatus;
    if (terminal_ != ReadStatus::Byte)
        return terminal_;
    if (position_ == payloadBytes_) {
        std::size_t count = 0;
        const auto status = source_.readFrame(value_, sizeof(value_), count);
        if (status == ProfileFrameRead::End && consumed_ == envelope_.bytes && count == 0) {
            terminal_ = ReadStatus::End;
            return terminal_;
        }
        ProfileDataFrame frame;
        if (status != ProfileFrameRead::Value || consumed_ == envelope_.bytes ||
            !decodeProfileDataFrame(value_, count, frame) || frame.transfer != envelope_.transfer ||
            frame.offset != consumed_ || frame.count > envelope_.bytes - consumed_) {
            terminal_ = ReadStatus::Failure;
            return terminal_;
        }
        position_ = 0;
        payloadBytes_ = frame.count;
    }
    byte = static_cast<std::uint8_t>(value_[ProfileFrameLimits::HeaderBytes + position_++]);
    ++consumed_;
    return ReadStatus::Byte;
}
} // namespace companion
} // namespace larp
