#pragma once

#include <cstdint>

namespace hardware_acceptance {
constexpr std::uint32_t RadioMagic = 0x4C415250;
constexpr std::uint8_t RadioVersion = 1;
constexpr std::uint8_t RadioData = 1;
constexpr std::uint8_t RadioAck = 2;
constexpr std::uint8_t RadioChannel = 1;

struct __attribute__((packed)) RadioFrame {
    std::uint32_t magic;
    std::uint8_t version;
    std::uint8_t type;
    std::uint16_t reserved;
    std::uint64_t run;
    std::uint32_t sequence;
    std::uint32_t check;
};

inline std::uint32_t radioCheck(const RadioFrame &frame) {
    return frame.magic ^ (static_cast<std::uint32_t>(frame.version) << 24) ^
           (static_cast<std::uint32_t>(frame.type) << 16) ^
           static_cast<std::uint32_t>(frame.run) ^ static_cast<std::uint32_t>(frame.run >> 32) ^
           frame.sequence ^ 0xA5C35A3CU;
}
} // namespace hardware_acceptance
