#pragma once
#include "../Esp32DeviceIdentityStore/Esp32DeviceIdentityStore.h"
#include "../Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.h"
#include "../FreeRtosGameControlChannel/FreeRtosGameControlChannel.h"
#include "Companion/IdentityAccess.h"

// Process-lifetime production startup owner. Call once before RF/ADC/I2S or
// device workers start: the identity entropy adapter temporarily owns that
// resource. This substrate deliberately has no BLE start or mutable access API.
// A later original SDK actor must own the complete policy handoff, never a
// caller-supplied connection, role, or cached App authorization.
class Esp32CompanionIdentityOwner {
  public:
    enum class State : std::uint8_t {
        NotStarted,
        Starting,
        ChannelUnavailable,
        StoreUnavailable,
        IdentityUnavailable,
        PolicyUnavailable,
        ReadyClosed
    };
    Esp32CompanionIdentityOwner() : identity_(store_, entropy_), policy_(channel_) {}
    Esp32CompanionIdentityOwner(const Esp32CompanionIdentityOwner &) = delete;
    Esp32CompanionIdentityOwner &operator=(const Esp32CompanionIdentityOwner &) = delete;
    State begin() noexcept;
    State state() const noexcept {
        return state_;
    }
    // Display metadata only. Failure preserves the destination.
    bool identity(larp::companion::Identity &) const noexcept;
    // Copied observation only. ReadyClosed must leave authority unavailable.
    bool authority(larp::companion::TrustedAuthority &) const noexcept;

  private:
    Esp32DeviceIdentityStore store_;
    Esp32StartupIdentityEntropy entropy_;
    larp::companion::DeviceIdentityOwner identity_;
    larp::companion::FreeRtosGameControlChannel channel_;
    larp::companion::IdentityAccess policy_;
    State state_ = State::NotStarted;
};
