#include "Companion/Identity.h"
#include <algorithm>
#include <cstdio>
namespace larp {
namespace companion {
bool Identity::valid() const noexcept {
    for (const auto byte : bytes)
        if (byte)
            return true;
    return false;
}
bool decodeIdentityHex(const char *bytes, std::size_t count, Identity &destination) noexcept {
    if (!bytes || count != 32)
        return false;
    Identity candidate;
    for (std::size_t i = 0; i < count; ++i) {
        const char ch = bytes[i];
        const int nibble = ch >= '0' && ch <= '9'   ? ch - '0'
                           : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                                    : -1;
        if (nibble < 0)
            return false;
        candidate.bytes[i / 2] |= static_cast<std::uint8_t>(nibble << (i % 2 ? 0 : 4));
    }
    if (!candidate.valid())
        return false;
    destination = candidate;
    return true;
}
ConfiguredEncodeResult encodeDeviceIdentity(const Identity &device, char *buffer,
                                            std::size_t capacity, std::size_t &written) noexcept {
    written = 0;
    if (buffer && capacity)
        buffer[0] = 0;
    if (!device.valid())
        return ConfiguredEncodeResult::InvalidValue;
    if (!buffer || !capacity)
        return ConfiguredEncodeResult::BufferTooSmall;
    char hex[33];
    const char alphabet[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < device.bytes.size(); ++i) {
        hex[2 * i] = alphabet[device.bytes[i] >> 4];
        hex[2 * i + 1] = alphabet[device.bytes[i] & 15];
    }
    hex[32] = 0;
    const int result =
        std::snprintf(buffer, std::min(capacity, maxDeviceIdentityJsonBytes),
                      "{\"schemaVersion\":1,\"kind\":\"device_identity\",\"deviceId\":\"%s\","
                      "\"scope\":\"display_identity\",\"requiresVerifiedConnection\":true,"
                      "\"requiresGameMasterAssignment\":true}",
                      hex);
    if (result < 0 ||
        static_cast<std::size_t>(result) >= std::min(capacity, maxDeviceIdentityJsonBytes)) {
        buffer[0] = 0;
        return ConfiguredEncodeResult::BufferTooSmall;
    }
    written = static_cast<std::size_t>(result);
    return ConfiguredEncodeResult::Success;
}
} // namespace companion
} // namespace larp
