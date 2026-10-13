#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace larp {
namespace runtime {
enum class PassiveTargetOutcome : std::uint8_t { InvalidFrame, NoTarget, UnsupportedUid, Card };
struct PassiveTarget {
    std::array<std::uint8_t, 7> uid = {};
    std::uint8_t length = 0;
};
// Restricted PN532 InListPassiveTarget/106-kbps Type A response. Caller supplies
// bytes actually read, excluding the SPI command/I2C readiness byte. No borrowed
// payload pointers escape. Normal frames only, at most 64 bytes, one target.
inline PassiveTargetOutcome parsePassiveTarget(const std::uint8_t *frame, std::size_t received,
                                               PassiveTarget &target) {
    target = PassiveTarget();
    if (!frame || received < 10 || frame[0] != 0 || frame[1] != 0 || frame[2] != 0xff)
        return PassiveTargetOutcome::InvalidFrame;
    const std::size_t length = frame[3];
    if (length < 3 || length > 57 || length + 7 > received ||
        static_cast<std::uint8_t>(frame[3] + frame[4]) != 0 || frame[5] != 0xd5 ||
        frame[6] != 0x4b || frame[length + 6] != 0)
        return PassiveTargetOutcome::InvalidFrame;
    std::uint8_t checksum = 0;
    for (std::size_t index = 5; index <= length + 5; ++index)
        checksum = static_cast<std::uint8_t>(checksum + frame[index]);
    if (checksum != 0)
        return PassiveTargetOutcome::InvalidFrame;
    if (frame[7] == 0)
        return length == 3 ? PassiveTargetOutcome::NoTarget : PassiveTargetOutcome::InvalidFrame;
    if (frame[7] != 1 || length < 8 || frame[8] != 1)
        return PassiveTargetOutcome::InvalidFrame;
    const std::size_t uidLength = frame[12];
    if (uidLength != 4 && uidLength != target.uid.size())
        return PassiveTargetOutcome::UnsupportedUid;
    if (8 + uidLength > length)
        return PassiveTargetOutcome::InvalidFrame;
    const std::size_t extra = length - 8 - uidLength;
    // Optional ATS includes its own length byte; reject hidden/truncated tails.
    if (extra && frame[13 + uidLength] != extra)
        return PassiveTargetOutcome::InvalidFrame;
    for (std::size_t index = 0; index < uidLength; ++index)
        target.uid[index] = frame[13 + index];
    target.length = static_cast<std::uint8_t>(uidLength);
    return PassiveTargetOutcome::Card;
}
} // namespace runtime
} // namespace larp
