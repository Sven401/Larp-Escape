#pragma once

#include <cstdint>

// Development capacity cut only. Owns a single, process-lifetime BLE stack
// start. It has no GATT, GAP advertising, connection, or authority interface.
class ClosedBleStackOwner {
  public:
    enum class State : std::uint8_t {
        NotStarted,
        ControllerInitFailed,
        ControllerEnableFailed,
        BluedroidInitFailed,
        BluedroidEnableFailed,
        ReadyClosed
    };

    ClosedBleStackOwner(const ClosedBleStackOwner &) = delete;
    ClosedBleStackOwner &operator=(const ClosedBleStackOwner &) = delete;
    ClosedBleStackOwner() = default;

    State begin() noexcept;
    State state() const noexcept { return state_; }

  private:
    State state_ = State::NotStarted;
};
