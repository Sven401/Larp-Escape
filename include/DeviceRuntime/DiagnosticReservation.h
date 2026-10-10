#pragma once
#include "DeviceRuntime/Lifecycle.h"
namespace larp {
namespace devices {
// Reservation only: no phase grants diagnostic execution or physical ownership.
enum class DiagnosticReservationPhase : std::uint8_t { Idle, Parking, ParkedClosed, Fault };
enum class DiagnosticReservationOutcome : std::uint8_t {
    Reserved,
    Busy,
    GameRunning,
    NotReady,
    StaleRevision,
    CounterExhausted
};
enum class DiagnosticReservationError : std::uint8_t {
    None,
    OwnerRejected,
    OwnerFailed,
    Timeout,
    BarrierFailure
};
struct DiagnosticReservationSnapshot {
    DiagnosticReservationPhase phase = DiagnosticReservationPhase::Idle;
    DiagnosticReservationError error = DiagnosticReservationError::None;
    std::uint32_t generation = 0, configurationRevision = 0, startedAt = 0;
    std::uint32_t staleReplies = 0;
    std::uint8_t parkedOwners = 0;
    ControlTicket operation;
};
} // namespace devices
} // namespace larp
