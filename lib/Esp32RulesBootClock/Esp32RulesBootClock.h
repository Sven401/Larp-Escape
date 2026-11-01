#pragma once
#include "DeviceRuntime/RulesProgressOwner.h"

// One actual ESP-IDF startup clock domain per process. NOT an identity key,
// authority, durable checkpoint, admission or automatic Running transition.
// beginEarlyStartup must run exclusively in app_main BEFORE ADC/I2S/RF/device
// initialization. No concurrent/ISR calls: publish only after successful begin,
// then use both clock ports on the same serialized owner worker.
class Esp32RulesBootClock final : public larp::devices::rules_progress::IClock,
                                  public larp::execution::rules::IManualClock {
  public:
    static Esp32RulesBootClock &instance() noexcept;
    Esp32RulesBootClock(const Esp32RulesBootClock &) = delete;
    Esp32RulesBootClock &operator=(const Esp32RulesBootClock &) = delete;
    Esp32RulesBootClock(Esp32RulesBootClock &&) = delete;
    Esp32RulesBootClock &operator=(Esp32RulesBootClock &&) = delete;
    // The first attempt is permanent, including failure. Repeated calls never
    // acquire entropy again. Recreating a RulesOwner cannot recreate this boot.
    bool beginEarlyStartup() noexcept;
    bool boot(larp::execution::rules::Identity &) const noexcept;
    bool read(larp::devices::rules_progress::ClockObservation &) override;
    bool now(std::uint64_t &) override;

  private:
    Esp32RulesBootClock() = default;
    enum class Phase : std::uint8_t { Uninitialized, Initializing, Ready, Failed };
    Phase phase_ = Phase::Uninitialized;
    bool operating_ = false;
    larp::execution::rules::Identity boot_;
    std::int64_t lastMicros_ = 0;
    bool sample(std::uint64_t &) noexcept;
};
