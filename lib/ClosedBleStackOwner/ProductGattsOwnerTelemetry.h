#pragma once

#include "ClosedBleSecurityGate.h"

// Diagnostic classification only. The existing security gate remains the sole
// decision; no telemetry outcome is a pairing, role or diagnostic grant.
struct ProductGattsOwnerTelemetry {
    enum class Reason : std::uint8_t {
        Allowed = 0,
        ServiceUnavailable = 1,
        RegistryUnavailable = 2,
        ChannelNotClosed = 3,
        NoOriginalLink = 4,
        RecycledLink = 5,
        PeerMismatch = 6,
        GateChanged = 7
    };

    static Reason classify(bool serviceReady, bool registryInstalledClosed,
                           bool channelClosed, bool observed, bool current,
                           const larp::companion::PeerTransportBinding &before,
                           const larp::companion::PeerTransportBinding &after,
                           const std::array<std::uint8_t, 6> &address) noexcept {
        if (!serviceReady)
            return Reason::ServiceUnavailable;
        if (!registryInstalledClosed)
            return Reason::RegistryUnavailable;
        if (!channelClosed)
            return Reason::ChannelNotClosed;
        if (!observed || !current || !before.valid() || !after.valid())
            return Reason::NoOriginalLink;
        if (!(before == after))
            return Reason::RecycledLink;
        if (before.address != address)
            return Reason::PeerMismatch;
        return Reason::Allowed;
    }
};
