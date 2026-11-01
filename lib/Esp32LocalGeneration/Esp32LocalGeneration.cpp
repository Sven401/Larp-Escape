#include "Esp32LocalGeneration.h"
#include "Esp32RulesBootClock.h"
#include <cstring>
using larp::devices::LocalGenerationStatus;
Esp32LocalGeneration &Esp32LocalGeneration::instance() noexcept {
    static Esp32LocalGeneration owner;
    return owner;
}
LocalGenerationStatus
Esp32LocalGeneration::reserveEarlyStartup(Esp32RulesBootClock &clock) noexcept {
    if (operating_)
        return LocalGenerationStatus::Unavailable;
    if (attempted_)
        return status_;
    attempted_ = operating_ = true;
    larp::execution::rules::Identity boot;
    if (clock.boot(boot)) {
        larp::devices::LocalGenerationSnapshot selected;
        status_ = store_.reserve(boot, selected);
        larp::execution::rules::Identity after;
        if (status_ == LocalGenerationStatus::Reserved && clock.boot(after) &&
            !std::memcmp(boot.bytes, after.bytes, 16))
            reserved_ = selected;
        else if (status_ == LocalGenerationStatus::Reserved)
            status_ = LocalGenerationStatus::Unavailable;
    }
    operating_ = false;
    return status_;
}
bool Esp32LocalGeneration::reservation(larp::devices::LocalGenerationSnapshot &out) const noexcept {
    out = {};
    if (operating_ || status_ != LocalGenerationStatus::Reserved)
        return false;
    out = reserved_;
    return true;
}
LocalGenerationStatus Esp32LocalGeneration::status() const noexcept {
    return operating_ ? LocalGenerationStatus::Unavailable : status_;
}
