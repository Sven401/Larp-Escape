#pragma once
#include "GattsServiceDefinition.h"
namespace larp {
namespace companion {
enum class GattsServiceEventKind : std::uint8_t { Other, Registered, Created, Attribute, Started };
struct GattsServiceEvent {
    GattsServiceEventKind kind = GattsServiceEventKind::Other;
    std::array<std::uint8_t, 16> uuid = {};
    std::uint16_t application = 0, service = 0, attribute = 0;
    std::uint8_t interface = 0xff, instance = 0;
    bool success = false, uuid128 = false, primary = false;
    bool operator==(const GattsServiceEvent &other) const noexcept {
        return kind == other.kind && uuid == other.uuid && application == other.application &&
               service == other.service && attribute == other.attribute &&
               interface == other.interface && instance == other.instance &&
               success == other.success && uuid128 == other.uuid128 && primary == other.primary;
    }
};
} // namespace companion
} // namespace larp
