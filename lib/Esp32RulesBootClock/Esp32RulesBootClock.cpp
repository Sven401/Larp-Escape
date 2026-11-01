#include "Esp32RulesBootClock.h"
#include "Esp32StartupIdentityEntropy.h"
#include <esp_timer.h>

Esp32RulesBootClock &Esp32RulesBootClock::instance() noexcept {
    static Esp32RulesBootClock domain;
    return domain;
}
bool Esp32RulesBootClock::beginEarlyStartup() noexcept {
    if (operating_ || phase_ == Phase::Initializing)
        return false;
    if (phase_ != Phase::Uninitialized)
        return phase_ == Phase::Ready;
    phase_ = Phase::Initializing;
    operating_ = true;
    larp::companion::Identity candidate;
    Esp32StartupIdentityEntropy entropy;
    // Existing adapter always releases startup entropy BEFORE returning.
    const bool filled = entropy.make(candidate);
    const auto micros = filled ? esp_timer_get_time() : -1;
    if (!filled || micros < 0) {
        phase_ = Phase::Failed;
        operating_ = false;
        return false;
    }
    for (std::size_t i = 0; i < candidate.bytes.size(); ++i)
        boot_.bytes[i] = candidate.bytes[i];
    lastMicros_ = micros;
    phase_ = Phase::Ready;
    operating_ = false;
    return true;
}
bool Esp32RulesBootClock::boot(larp::execution::rules::Identity &out) const noexcept {
    out = {};
    if (operating_ || phase_ != Phase::Ready)
        return false;
    out = boot_;
    return true;
}
bool Esp32RulesBootClock::sample(std::uint64_t &out) noexcept {
    out = 0;
    if (operating_ || phase_ != Phase::Ready)
        return false;
    operating_ = true;
    const auto micros = esp_timer_get_time();
    if (micros < 0 || micros < lastMicros_) {
        phase_ = Phase::Failed;
        operating_ = false;
        return false;
    }
    lastMicros_ = micros;
    out = static_cast<std::uint64_t>(micros / 1000);
    operating_ = false;
    return true;
}
bool Esp32RulesBootClock::read(larp::devices::rules_progress::ClockObservation &out) {
    out = {};
    std::uint64_t millis = 0;
    if (!sample(millis))
        return false;
    out.boot = boot_;
    out.monotonic = millis;
    return true;
}
bool Esp32RulesBootClock::now(std::uint64_t &out) {
    return sample(out);
}
