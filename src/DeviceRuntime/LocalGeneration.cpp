#include "DeviceRuntime/LocalGeneration.h"
#include <cstring>
namespace larp {
namespace devices {
namespace {
const std::uint8_t Magic[8] = {'L', 'A', 'R', 'P', 'L', 'G', '0', '1'};
std::uint32_t crc(const std::uint8_t *data) noexcept {
    std::uint32_t value = 0xffffffffu;
    for (std::size_t i = 0; i < 60; ++i) {
        value ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
    }
    return ~value;
}
void put(std::uint8_t *out, std::uint64_t value, std::size_t size) noexcept {
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = static_cast<std::uint8_t>(value);
        value >>= 8;
    }
}
std::uint64_t get(const std::uint8_t *in, std::size_t size) noexcept {
    std::uint64_t out = 0;
    for (std::size_t i = 0; i < size; ++i)
        out |= std::uint64_t(in[i]) << (i * 8);
    return out;
}
bool valid(const LocalGenerationSnapshot &value) noexcept {
    if (!value.generation)
        return false;
    for (auto byte : value.boot.bytes)
        if (byte)
            return true;
    return false;
}
} // namespace
bool encodeLocalGeneration(const LocalGenerationSnapshot &source,
                           LocalGenerationRecord &out) noexcept {
    const auto value = source;
    out = {};
    if (!valid(value))
        return false;
    std::memcpy(out.bytes.data(), Magic, 8);
    put(out.bytes.data() + 8, 1, 2);
    put(out.bytes.data() + 10, 64, 2);
    put(out.bytes.data() + 16, value.generation, 8);
    std::memcpy(out.bytes.data() + 24, value.boot.bytes, 16);
    put(out.bytes.data() + 60, crc(out.bytes.data()), 4);
    return true;
}
bool decodeLocalGeneration(const LocalGenerationRecord &source,
                           LocalGenerationSnapshot &out) noexcept {
    const auto record = source;
    out = {};
    const auto *p = record.bytes.data();
    if (std::memcmp(p, Magic, 8) || get(p + 8, 2) != 1 || get(p + 10, 2) != 64 ||
        get(p + 60, 4) != crc(p))
        return false;
    for (std::size_t i = 12; i < 16; ++i)
        if (p[i])
            return false;
    for (std::size_t i = 40; i < 60; ++i)
        if (p[i])
            return false;
    LocalGenerationSnapshot value;
    value.generation = get(p + 16, 8);
    std::memcpy(value.boot.bytes, p + 24, 16);
    if (!valid(value))
        return false;
    out = value;
    return true;
}
} // namespace devices
} // namespace larp
