#pragma once
#include "DeviceRuntime/LocalGeneration.h"
class Esp32LocalGeneration;
// Concrete SDK backend; only the process owner may construct or invoke it.
// One shared exclusive software CAS, NOT a native NVS compare_exchange.
// No namespace/key creation on Missing, no initializer, erase or repair wrapper.
// NVS init itself may perform internal recovery/GC on the SHARED partition.
class Esp32LocalGenerationStore final {
  private:
    friend class Esp32LocalGeneration;
    Esp32LocalGenerationStore() = default;
    larp::devices::LocalGenerationStatus reserve(const larp::execution::rules::Identity &,
                                                 larp::devices::LocalGenerationSnapshot &) noexcept;
};
