#include "Companion/ProfileSourceEnvelope.h"
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
bool ProfileSourceEnvelope::valid() const noexcept {
    return transfer && bytes && bytes <= configuration::ImportLimits::DocumentBytes &&
           expectedDraftRevision != UINT32_MAX && expectedConfigurationRevision;
}
bool decodeProfileSourceEnvelope(const char *bytes, std::size_t count,
                                 ProfileSourceEnvelope &destination) noexcept {
    if (!bytes || count != ProfileSourceLimits::EnvelopeBytes ||
        static_cast<unsigned char>(bytes[0]) != ProfileSourceLimits::Version ||
        static_cast<unsigned char>(bytes[1]) != ProfileSourceLimits::Kind)
        return false;
    ProfileSourceEnvelope value;
    value.transfer = static_cast<std::uint16_t>(get(bytes + 2, 2));
    value.bytes = get(bytes + 4, 4);
    value.crc32 = get(bytes + 8, 4);
    value.expectedDraftRevision = get(bytes + 12, 4);
    value.expectedConfigurationRevision = get(bytes + 16, 4);
    if (!value.valid())
        return false;
    destination = value;
    return true;
}
bool encodeProfileSourceEnvelope(const ProfileSourceEnvelope &source, char *bytes,
                                 std::size_t capacity, std::size_t &written) noexcept {
    written = 0;
    if (!bytes || capacity < ProfileSourceLimits::EnvelopeBytes || !source.valid())
        return false;
    bytes[0] = ProfileSourceLimits::Version;
    bytes[1] = ProfileSourceLimits::Kind;
    put(bytes + 2, 2, source.transfer);
    put(bytes + 4, 4, source.bytes);
    put(bytes + 8, 4, source.crc32);
    put(bytes + 12, 4, source.expectedDraftRevision);
    put(bytes + 16, 4, source.expectedConfigurationRevision);
    written = ProfileSourceLimits::EnvelopeBytes;
    return true;
}
std::uint32_t profileSourceCrcByte(std::uint32_t crc, std::uint8_t byte) noexcept {
    crc ^= byte;
    for (unsigned i = 0; i < 8; ++i)
        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    return crc;
}
} // namespace companion
} // namespace larp
