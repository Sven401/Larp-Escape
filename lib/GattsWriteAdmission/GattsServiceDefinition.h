#pragma once
#include <array>
#include <cstdint>
namespace larp {
namespace companion {
struct GattsServiceDefinition {
    static const std::uint16_t Application = 0x4c45, Handles = 5;
    static const std::uint32_t SetupDeadlineMilliseconds = 30000;
    static const std::uint32_t EndpointDeadlineMilliseconds = 5000;
    // Canonical UUIDs are stored in BLE/ESP's little-endian 128-bit order.
    static std::array<std::uint8_t, 16> service() noexcept {
        return {{0x01, 0x00, 0xa2, 0x85, 0xff, 0x4a, 0x6b, 0x85, 0xfd, 0x43, 0xc2, 0x1c, 0x1d, 0x6a,
                 0x7e, 0x40}};
    }
    static std::array<std::uint8_t, 16> command() noexcept {
        return {{0x02, 0x00, 0xa2, 0x85, 0xff, 0x4a, 0x6b, 0x85, 0xfd, 0x43, 0xc2, 0x1c, 0x1d, 0x6a,
                 0x7e, 0x40}};
    }
    static std::array<std::uint8_t, 16> profile() noexcept {
        return {{0x03, 0x00, 0xa2, 0x85, 0xff, 0x4a, 0x6b, 0x85, 0xfd, 0x43, 0xc2, 0x1c, 0x1d, 0x6a,
                 0x7e, 0x40}};
    }
};
struct GattsSourceRegistration {
    std::uint32_t token = 0;
    std::uint8_t interface = 0xff;
    bool operator==(const GattsSourceRegistration &other) const noexcept {
        return token == other.token && interface == other.interface;
    }
};
struct GattsEndpointSetup {
    GattsSourceRegistration original;
    std::uint32_t receivedAt = 0;
    std::uint16_t service = 0, command = 0, profile = 0;
    bool operator==(const GattsEndpointSetup &other) const noexcept {
        return original == other.original && receivedAt == other.receivedAt &&
               service == other.service && command == other.command && profile == other.profile;
    }
};
enum class GattsEndpointSetupState : std::uint8_t { Unavailable, Pending, Installed, Rejected };
enum class GattsEndpointSetupPost : std::uint8_t { Posted, Duplicate, Busy, Rejected };
} // namespace companion
} // namespace larp
