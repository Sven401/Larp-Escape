#pragma once

#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"

#include <Wire.h>

// Blocking Wire calls belong on the test-firmware worker, never in a BLE
// callback. The caller initializes the bus and owns serialization.
class RawExpanderWireReader final : public larp::diagnostic::IRawRegisterReader {
  public:
    explicit RawExpanderWireReader(TwoWire &wire) noexcept : wire_(wire) {}
    larp::diagnostic::RawRegisterRead read(std::uint8_t address,
                                           std::uint8_t reg) noexcept override;

  private:
    TwoWire &wire_;
};
