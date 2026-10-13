#include "SerialGameDiagnostics.h"
#include <Arduino.h>

void SerialGameDiagnostics::report(larp::ports::GameDiagnosticEvent event, std::uint32_t value,
                                   std::uint32_t auxiliary) {
    using larp::ports::GameDiagnosticEvent;
    const char *name = nullptr;
    switch (event) {
    case GameDiagnosticEvent::PhaseEntered:
        name = "phase";
        break;
    case GameDiagnosticEvent::RoleRecognized:
        name = "role";
        break;
    case GameDiagnosticEvent::RoleRejected:
        name = "role-rejected";
        break;
    case GameDiagnosticEvent::CrystalRecorded:
        name = "crystal-recorded";
        break;
    case GameDiagnosticEvent::CrystalRejected:
        name = "crystal-rejected";
        break;
    case GameDiagnosticEvent::CrystalWithoutRole:
        name = "crystal-without-role";
        break;
    case GameDiagnosticEvent::CrystalAudioUnavailable:
        name = "audio-unavailable";
        break;
    case GameDiagnosticEvent::CrystalAudioStarted:
        name = "audio-started";
        break;
    case GameDiagnosticEvent::KeystoneRepeated:
        name = "role-repeated";
        break;
    case GameDiagnosticEvent::KeystoneBoardNotCleared:
        name = "board-not-cleared";
        break;
    case GameDiagnosticEvent::KeystoneMissing:
        name = "role-missing";
        break;
    case GameDiagnosticEvent::ActivationSent:
        name = "activation-sent";
        break;
    // High-frequency observations remain available to other diagnostic sinks.
    case GameDiagnosticEvent::CrystalScan:
    case GameDiagnosticEvent::CrystalObserved:
    case GameDiagnosticEvent::KeystoneReadEmpty:
        return;
    }
    if (name == nullptr)
        return;
    Serial.print("Game ");
    Serial.print(name);
    Serial.print(": ");
    Serial.print(value);
    Serial.print(", ");
    Serial.println(auxiliary);
}
