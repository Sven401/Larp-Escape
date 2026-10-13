#pragma once

#include <cstdint>

namespace larp {
namespace ports {
// Diagnostic output never participates in game decisions. Each payload is a value copy.
// PhaseEntered uses GamePhase; RoleRecognized uses RoleId; CrystalObserved/Rejected
// use row and column; CrystalRecorded uses count and audio track; other events use
// the relevant count, track or boolean result in value.
enum class GameDiagnosticEvent : std::uint8_t {
    PhaseEntered,
    RoleRecognized,
    RoleRejected,
    CrystalScan,
    CrystalWithoutRole,
    CrystalObserved,
    CrystalRecorded,
    CrystalRejected,
    CrystalAudioUnavailable,
    CrystalAudioStarted,
    KeystoneReadEmpty,
    KeystoneRepeated,
    KeystoneBoardNotCleared,
    KeystoneMissing,
    ActivationSent
};

class IGameDiagnostics {
  public:
    virtual ~IGameDiagnostics() {}
    virtual void report(GameDiagnosticEvent event, std::uint32_t value = 0U,
                        std::uint32_t auxiliary = 0U) = 0;
};
} // namespace ports
} // namespace larp