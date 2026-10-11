#pragma once

#include <cstdint>

// Owns a single, process-lifetime BLE stack start. This layer has no GATT,
// GAP advertising, connection, or authority interface; the product owner
// installs those separately after this closed startup succeeds.
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
