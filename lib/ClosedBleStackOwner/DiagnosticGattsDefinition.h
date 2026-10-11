#pragma once
#include "../GattsWriteAdmission/GattsServiceDefinition.h"
#include <array>
#include <cstdint>

namespace larp {
namespace diagnostic {
// BLE/ESP little-endian UUID bytes, distinct from the existing game service.
struct DiagnosticGattsDefinition {
    static const std::uint16_t Handles = 11; // service, four declaration/value pairs, two CCCDs
    static const std::uint32_t SetupDeadlineMilliseconds = 30000;
    static std::array<std::uint8_t, 16> uuid(std::uint8_t suffix) noexcept {
        return {{suffix, 0x00, 0xa2, 0x85, 0xff, 0x4a, 0x6b, 0x85,
                 0xfd, 0x43, 0xc2, 0x1c, 0x1d, 0x6a, 0x7e, 0x40}};
    }
    static std::array<std::uint8_t, 16> service() noexcept { return uuid(0x10); }
    static std::array<std::uint8_t, 16> controlWrite() noexcept { return uuid(0x11); }
    static std::array<std::uint8_t, 16> controlReply() noexcept { return uuid(0x12); }
    static std::array<std::uint8_t, 16> stepWrite() noexcept { return uuid(0x13); }
    static std::array<std::uint8_t, 16> stepReply() noexcept { return uuid(0x14); }
};

struct DiagnosticClosedHandles {
    companion::GattsSourceRegistration original;
    std::uint16_t service = 0, controlWrite = 0, controlReply = 0,
                  controlCccd = 0, stepWrite = 0, stepReply = 0, stepCccd = 0;
    bool complete() const noexcept {
        return original.token && original.interface != 0xff && service &&
               service < controlWrite && controlWrite < controlReply &&
               controlReply < controlCccd && controlCccd < stepWrite &&
               stepWrite < stepReply && stepReply < stepCccd &&
               static_cast<std::uint32_t>(stepCccd) <
                   static_cast<std::uint32_t>(service) + DiagnosticGattsDefinition::Handles;
    }
    bool protectedWrite(std::uint16_t handle) const noexcept {
        return handle == controlWrite || handle == controlCccd ||
               handle == stepWrite || handle == stepCccd;
    }
};
} // namespace diagnostic
} // namespace larp
