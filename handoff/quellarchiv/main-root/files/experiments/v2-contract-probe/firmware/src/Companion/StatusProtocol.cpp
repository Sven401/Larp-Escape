#include "Companion/StatusProtocol.h"

#include <cstdio>
#include <cstring>

namespace larp {
namespace companion {
namespace {
constexpr char statusFormat[] = "{\"schemaVersion\":1,\"capabilities\":[\"status.read\"],\"game\":{"
                                "\"phase\":\"%s\",\"role\":%s%s%s,\"crystalCount\":%u,"
                                "\"roundCount\":%lu,\"seenRoleCount\":%u,\"incorrectCrystal\":%s,"
                                "\"keystoneError\":%s,\"audioBusy\":%s}}";

constexpr std::size_t maximumPhaseNameBytes = 32U;
constexpr std::size_t maximumRoleNameBytes = 24U;
// Keep even an overestimate (format placeholders included) within the v1 cap.
static_assert(sizeof(statusFormat) + maximumPhaseNameBytes + maximumRoleNameBytes + 2U + 10U + 2U +
                      3U * 5U <=
                  maxStatusJsonBytes,
              "The version-1 status must fit its fixed capacity");

const char *booleanWireValue(bool value) {
    return value ? "true" : "false";
}
} // namespace

const char *phaseWireName(application::GamePhase phase) {
    switch (phase) {
    case application::GamePhase::Standby:
        return "standby";
    case application::GamePhase::Idle:
        return "idle";
    case application::GamePhase::WaitingForCrystals:
        return "waiting_for_crystals";
    case application::GamePhase::FirstCrystalPlaced:
        return "first_crystal_placed";
    case application::GamePhase::SecondCrystalPlaced:
        return "second_crystal_placed";
    case application::GamePhase::ThirdCrystalPlaced:
        return "third_crystal_placed";
    case application::GamePhase::GameCompleted:
        return "game_completed";
    case application::GamePhase::Bonus:
        return "bonus";
    case application::GamePhase::Error:
        return "error";
    }
    return nullptr;
}

const char *roleWireName(domain::RoleId role) {
    switch (role) {
    case domain::RoleId::Puppenspielerin:
        return "puppenspielerin";
    case domain::RoleId::Krieger:
        return "krieger";
    case domain::RoleId::Narr:
        return "narr";
    case domain::RoleId::Abenteurerin:
        return "abenteurerin";
    case domain::RoleId::Ueberlebende:
        return "ueberlebende";
    case domain::RoleId::Wissende:
        return "wissende";
    }
    return nullptr;
}

StatusEncodeResult encodeStatus(const application::GameSnapshot &snapshot, char *buffer,
                                std::size_t capacity, std::size_t &written) {
    written = 0U;
    if (buffer != nullptr && capacity > 0U) {
        buffer[0] = '\0';
    }

    const char *phase = phaseWireName(snapshot.phase);
    const char *role = snapshot.rolePresent ? roleWireName(snapshot.roleId) : "null";
    if (phase == nullptr || role == nullptr || snapshot.crystalCount > 3U ||
        snapshot.seenRoleCount > 6U || std::strlen(phase) >= maximumPhaseNameBytes ||
        std::strlen(role) >= maximumRoleNameBytes) {
        return StatusEncodeResult::InvalidSnapshot;
    }
    if (buffer == nullptr || capacity == 0U) {
        return StatusEncodeResult::BufferTooSmall;
    }

    const std::size_t outputCapacity =
        capacity < maxStatusJsonBytes ? capacity : maxStatusJsonBytes;
    const char *roleQuote = snapshot.rolePresent ? "\"" : "";
    const int encoded = std::snprintf(buffer, outputCapacity, statusFormat, phase, roleQuote, role,
                                      roleQuote, static_cast<unsigned int>(snapshot.crystalCount),
                                      static_cast<unsigned long>(snapshot.roundCount),
                                      static_cast<unsigned int>(snapshot.seenRoleCount),
                                      booleanWireValue(snapshot.incorrectCrystal),
                                      booleanWireValue(snapshot.keystoneError),
                                      booleanWireValue(snapshot.audioBusy));
    if (encoded < 0) {
        buffer[0] = '\0';
        return StatusEncodeResult::InvalidSnapshot;
    }
    if (static_cast<std::size_t>(encoded) >= outputCapacity) {
        buffer[0] = '\0';
        return StatusEncodeResult::BufferTooSmall;
    }

    written = static_cast<std::size_t>(encoded);
    return StatusEncodeResult::Success;
}
} // namespace companion
} // namespace larp
