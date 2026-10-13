#pragma once
#include "Companion/ConfiguredProtocol.h"
namespace larp {
namespace companion {
// Opaque stable registry labels. Not a MAC, key, claimed role or cryptographic proof.
struct Identity {
    std::array<std::uint8_t, 16> bytes = {};
    bool valid() const noexcept;
    bool operator==(const Identity &other) const noexcept {
        return bytes == other.bytes;
    }
};
constexpr std::size_t maxDeviceIdentityJsonBytes = 256;
// Public display metadata ONLY. Client must separately verify/pin the connection.
ConfiguredEncodeResult encodeDeviceIdentity(const Identity &device, char *buffer,
                                            std::size_t capacity, std::size_t &written) noexcept;
bool decodeIdentityHex(const char *bytes, std::size_t count, Identity &destination) noexcept;
} // namespace companion
} // namespace larp
