#pragma once
#include "GameConfiguration/Profile.h"

namespace larp {
namespace devices {
using configuration::ComponentId;
using configuration::ComponentKind;
using configuration::Limits;
using configuration::Profile;
enum class Owner : std::uint8_t { Nfc, I2c, Audio, Led, Peer, Count };
struct ChannelLimits {
    static const std::size_t Owners = static_cast<std::size_t>(Owner::Count);
    static const std::size_t CommandsPerOwner = 16;
    static const std::size_t Replies = 8;
    static const std::size_t ReplyDrainPerService = 8;
};
struct Route {
    ComponentId id;
    ComponentKind kind = ComponentKind::NfcSpi;
    Owner owner = Owner::Nfc;
    bool enabled = false;
    std::uint8_t profileSlot = 0;
    std::array<std::uint8_t, Limits::MatrixLines> rowExpanders = {};
    std::array<std::uint8_t, Limits::MatrixLines> columnExpanders = {};
    std::array<std::uint8_t, Limits::WakeInputs> wakeExpanders = {};
};
struct I2cBus {
    bool used = false;
    std::uint8_t sda = configuration::UnusedPin, scl = configuration::UnusedPin;
};
struct Plan {
    std::uint32_t profileRevision = 0;
    std::array<Route, Limits::Components> routes = {};
    std::uint8_t routeCount = 0, enabledCount = 0;
    std::array<std::uint8_t, Limits::Components> initializationOrder = {};
    std::array<I2cBus, 2> i2c = {};
    std::array<std::uint8_t, ChannelLimits::Owners> ownerComponents = {};
    std::uint16_t totalLeds = 0;
    std::uint8_t peerChannel = 0;
    std::uint32_t replyTimeoutMilliseconds = 0;
};
enum class PlanError : std::uint8_t { None, InvalidProfile, RadioChannelConflict };
struct PlanResult {
    PlanError error = PlanError::None;
    configuration::ValidationResult validation;
    ComponentId component, conflictingComponent;
    explicit operator bool() const {
        return error == PlanError::None;
    }
};
// Validate first, then compile to caller-owned storage. On failure destination
// remains unchanged. The plan stores indices/IDs, not borrowed profile pointers.
// This declares ownership and dependencies; it does NOT initialize hardware.
PlanResult compilePlan(const Profile &profile, Plan &destination);
Owner componentOwner(ComponentKind kind);
const char *ownerName(Owner owner);
const char *planErrorName(PlanError error);
} // namespace devices
} // namespace larp
