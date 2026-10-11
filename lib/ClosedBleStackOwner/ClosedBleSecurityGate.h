#pragma once

#include "Companion/PeerLinkSecurity.h"

// A GAP security request is allowed only for the current original GATT birth.
// This grants SMP a chance to bond; it never grants a product role or diagnosis.
class ClosedBleSecurityGate {
  public:
    static bool allow(bool serviceReady, bool registryInstalledClosed,
                      bool channelClosed, const larp::companion::PeerTransportBinding &before,
                      const larp::companion::PeerTransportBinding &after,
                      const std::array<std::uint8_t, 6> &requestAddress) noexcept {
        return serviceReady && registryInstalledClosed && channelClosed &&
               before.valid() && before == after && before.address == requestAddress;
    }
};
