#pragma once
#include "DeviceRuntime/LocalGeneration.h"
#include "Esp32LocalGenerationStore.h"
class Esp32RulesBootClock;
// Exactly one process-lifetime reservation owner. Startup exclusively after
// startup entropy is disabled, before workers. No concurrent/ISR calls.
// Public copied reservation is numeric storage evidence ONLY, never an input,
// authorization, station binding, Running or Control acceptance.
class Esp32LocalGeneration final {
  public:
    static Esp32LocalGeneration &instance() noexcept;
    Esp32LocalGeneration(const Esp32LocalGeneration &) = delete;
    Esp32LocalGeneration &operator=(const Esp32LocalGeneration &) = delete;
    Esp32LocalGeneration(Esp32LocalGeneration &&) = delete;
    Esp32LocalGeneration &operator=(Esp32LocalGeneration &&) = delete;
    // First attempt is permanent, success OR failure. New process ALWAYS
    // reserves old+1 even when its new random Boot128 equals the previous one.
    larp::devices::LocalGenerationStatus reserveEarlyStartup(Esp32RulesBootClock &) noexcept;
    bool reservation(larp::devices::LocalGenerationSnapshot &) const noexcept;
    larp::devices::LocalGenerationStatus status() const noexcept;

  private:
    Esp32LocalGeneration() = default;
    Esp32LocalGenerationStore store_;
    larp::devices::LocalGenerationSnapshot reserved_;
    larp::devices::LocalGenerationStatus status_ =
        larp::devices::LocalGenerationStatus::Unavailable;
    bool attempted_ = false, operating_ = false;
};
