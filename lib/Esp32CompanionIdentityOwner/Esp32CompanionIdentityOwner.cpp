#include "Esp32CompanionIdentityOwner.h"

Esp32CompanionIdentityOwner::State Esp32CompanionIdentityOwner::begin() noexcept {
    if (state_ != State::NotStarted)
        return state_; // Includes reentry and every failed attempt; no automatic retry.
    state_ = State::Starting;
    if (!channel_.begin())
        return state_ = State::ChannelUnavailable;
    channel_.suspendAuthority();
    if (!store_.begin())
        return state_ = State::StoreUnavailable;
    const auto result = identity_.begin();
    larp::companion::Identity device;
    if ((result != larp::companion::DeviceIdentityOutcome::Loaded &&
         result != larp::companion::DeviceIdentityOutcome::Created) ||
        !identity_.identity(device))
        return state_ = State::IdentityUnavailable;
    if (policy_.begin(device) != larp::companion::IdentityAccessOutcome::Initialized)
        return state_ = State::PolicyUnavailable;
    larp::companion::Identity fresh;
    if (!entropy_.make(fresh))
        return state_ = State::BootUnavailable;
    larp::companion::BootIdentity boot;
    for (std::size_t index = 0; index < 4; ++index) {
        boot.high = (boot.high << 8) | fresh.bytes[index];
        boot.low = (boot.low << 8) | fresh.bytes[index + 4];
    }
    if (!boot.valid())
        return state_ = State::BootUnavailable;
    boot_ = boot;
    return state_ = State::ReadyClosed;
}
bool Esp32CompanionIdentityOwner::identity(larp::companion::Identity &destination) const noexcept {
    return state_ == State::ReadyClosed && policy_.deviceIdentity(destination);
}
bool Esp32CompanionIdentityOwner::bootIdentity(
    larp::companion::BootIdentity &destination) const noexcept {
    if (state_ != State::ReadyClosed || !boot_.valid())
        return false;
    destination = boot_;
    return true;
}
bool Esp32CompanionIdentityOwner::authority(
    larp::companion::TrustedAuthority &destination) const noexcept {
    return channel_.readAuthority(destination);
}
