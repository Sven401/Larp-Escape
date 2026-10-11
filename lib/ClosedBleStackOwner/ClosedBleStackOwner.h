#pragma once

#include <cstdint>
#include "ClosedBleStartupAttempt.h"

// Owns a single, process-lifetime BLE stack start. This layer has no GATT,
// GAP advertising, connection, or authority interface; the product owner
// installs those separately after this closed startup succeeds.
class ClosedBleStackOwner {
  public:
    using State = ClosedBleStartupAttempt::State;

    ClosedBleStackOwner(const ClosedBleStackOwner &) = delete;
    ClosedBleStackOwner &operator=(const ClosedBleStackOwner &) = delete;
    ClosedBleStackOwner() = default;

    State begin() noexcept;
    State state() const noexcept { return attempt_.report().state; }
    const ClosedBleStartupAttempt::Report &report() const noexcept {
        return attempt_.report();
    }

  private:
    ClosedBleStartupAttempt attempt_;
};
