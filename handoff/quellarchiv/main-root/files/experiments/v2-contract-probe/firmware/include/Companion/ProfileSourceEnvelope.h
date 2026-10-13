#pragma once
#include "GameConfiguration/ProfileReader.h"
namespace larp {
namespace companion {
struct ProfileSourceLimits {
    static const std::size_t EnvelopeBytes = 20;
    static const std::uint8_t Version = 1, Kind = 5;
    static const std::uint32_t DeadlineMilliseconds = 120000;
};
// Metadata for a complete exclusively owned bounded source. Not a data frame,
// authentication proof or apply command. CRC detects accidental corruption only.
struct ProfileSourceEnvelope {
    std::uint16_t transfer = 0;
    std::uint32_t bytes = 0, crc32 = 0, expectedDraftRevision = 0,
                  expectedConfigurationRevision = 0;
    bool valid() const noexcept;
};
bool decodeProfileSourceEnvelope(const char *bytes, std::size_t count,
                                 ProfileSourceEnvelope &destination) noexcept;
bool encodeProfileSourceEnvelope(const ProfileSourceEnvelope &source, char *bytes,
                                 std::size_t capacity, std::size_t &written) noexcept;
// CRC32/ISO-HDLC: initial0xffffffff, reflected polynomial0xedb88320,
// final xor0xffffffff. Count/digest include all whitespace and UTF8 bytes.
std::uint32_t profileSourceCrcByte(std::uint32_t crc, std::uint8_t byte) noexcept;
class IProfileImportClock {
  public:
    virtual ~IProfileImportClock() = default;
    virtual std::uint32_t nowMilliseconds() const noexcept = 0;
};
} // namespace companion
} // namespace larp
