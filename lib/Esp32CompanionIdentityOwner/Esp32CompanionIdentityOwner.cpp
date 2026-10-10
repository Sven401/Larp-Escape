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
    return state_ = State::ReadyClosed;
}
bool Esp32CompanionIdentityOwner::identity(larp::companion::Identity &destination) const noexcept {
    return state_ == State::ReadyClosed && policy_.deviceIdentity(destination);
}
bool Esp32CompanionIdentityOwner::authority(
    larp::companion::TrustedAuthority &destination) const noexcept {
    return channel_.readAuthority(destination);
}
