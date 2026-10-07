#include "FreeRtosGattsConnection.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
bool addressValid(const std::array<std::uint8_t, 6> &address) {
    for (const auto byte : address)
        if (byte)
            return true;
    return false;
}
} // namespace
bool FreeRtosGattsConnection::validEndpoints() const noexcept {
    return endpoints_.interface != 0xff && endpoints_.command && endpoints_.profile &&
           endpoints_.command != endpoints_.profile;
}
bool FreeRtosGattsConnection::matches(const GattsWriteBinding &value) const noexcept {
    return active_ && value.connection.token && value.connection == live_.connection &&
           value.interface == live_.interface && value.peerAddress == live_.peerAddress;
}
bool FreeRtosGattsConnection::originalLive(const GattsWriteBinding &caller) const noexcept {
    const auto value = caller;
    portENTER_CRITICAL(&mutex_);
    const bool result = matches(value) && (value.attribute == endpoints_.command ||
                                           value.attribute == endpoints_.profile);
    portEXIT_CRITICAL(&mutex_);
    return result;
}
std::uint32_t FreeRtosGattsConnection::nowMilliseconds() const noexcept {
    return clock_.nowMilliseconds();
}
GattsLifecycleOutcome FreeRtosGattsConnection::connectNow(const GattsConnectionEvent &caller,
                                                          BoundConnection &destination) noexcept {
    const auto event = caller;
    if (busy_)
        return GattsLifecycleOutcome::Busy;
    if (!validEndpoints() || event.interface != endpoints_.interface ||
        !addressValid(event.peerAddress))
        return GattsLifecycleOutcome::Invalid;
    busy_ = true;
    GattsWriteBinding old;
    portENTER_CRITICAL(&mutex_);
    const bool already = active_;
    if (already) {
        old = live_;
        active_ = false;
        live_ = {};
    }
    portEXIT_CRITICAL(&mutex_);
    // Unexpected second connect may mean a lost disconnect. Invalidate the
    // old local context immediately; never keep old rights on the new SDK link.
    if (already) {
        access_.disconnect(old.connection);
        busy_ = false;
        return GattsLifecycleOutcome::Conflict;
    }
    BoundConnection opened;
    if (access_.connect(event.connection, opened) != IdentityAccessOutcome::Connected) {
        busy_ = false;
        return GattsLifecycleOutcome::Unavailable;
    }
    GattsWriteBinding next;
    next.interface = event.interface;
    next.connection = opened;
    next.peerAddress = event.peerAddress;
    // Connect mints a closed epoch only. No GM grant from any SDK field.
    next.authority.epoch = opened.token;
    portENTER_CRITICAL(&mutex_);
    live_ = next;
    active_ = true;
    portEXIT_CRITICAL(&mutex_);
    destination = opened;
    busy_ = false;
    return GattsLifecycleOutcome::Opened;
}
GattsLifecycleOutcome FreeRtosGattsConnection::closeOwned(const GattsWriteBinding &value) noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool current = matches(value);
    if (current) {
        active_ = false;
        live_ = {};
    }
    portEXIT_CRITICAL(&mutex_);
    if (!current)
        return GattsLifecycleOutcome::Ignored;
    // Transport closes before observing external policy, even on failure.
    return access_.disconnect(value.connection) == IdentityAccessOutcome::Disconnected
               ? GattsLifecycleOutcome::Closed
               : GattsLifecycleOutcome::Unavailable;
}
GattsLifecycleOutcome
FreeRtosGattsConnection::retireOriginal(const GattsWriteBinding &caller) noexcept {
    const auto original = caller;
    if (busy_)
        return GattsLifecycleOutcome::Busy;
    busy_ = true;
    const auto result = closeOwned(original);
    busy_ = false;
    return result;
}
GattsLifecycleOutcome
FreeRtosGattsConnection::disconnectNow(const GattsConnectionEvent &caller) noexcept {
    const auto event = caller;
    if (busy_)
        return GattsLifecycleOutcome::Busy;
    busy_ = true;
    GattsWriteBinding original;
    portENTER_CRITICAL(&mutex_);
    const bool current = active_ && event.interface == live_.interface &&
                         event.connection == live_.connection.handle &&
                         event.peerAddress == live_.peerAddress;
    if (current)
        original = live_;
    portEXIT_CRITICAL(&mutex_);
    const auto result = current ? closeOwned(original) : GattsLifecycleOutcome::Ignored;
    busy_ = false;
    return result;
}
bool FreeRtosGattsConnection::captureWriteNow(const GattsWriteEvent &caller, GattsEndpoint endpoint,
                                              GattsWriteBinding &destination) noexcept {
    const auto event = caller;
    if (busy_ || !validEndpoints() ||
        (endpoint != GattsEndpoint::Command && endpoint != GattsEndpoint::Profile))
        return false;
    const auto selected =
        endpoint == GattsEndpoint::Command ? endpoints_.command : endpoints_.profile;
    if (event.attribute != selected)
        return false;
    busy_ = true;
    GattsWriteBinding original;
    portENTER_CRITICAL(&mutex_);
    const bool current = active_ && event.interface == live_.interface &&
                         event.connection == live_.connection.handle &&
                         event.peerAddress == live_.peerAddress;
    if (current)
        original = live_;
    portEXIT_CRITICAL(&mutex_);
    if (!current) {
        busy_ = false;
        return false;
    }
    original.attribute = selected;
    TrustedAuthority route;
    if (access_.route(original.connection, route))
        original.authority = route;
    else
        original.authority = {}; // Closed/no grant; still an original ATT-error context.
    const bool stillLive = originalLive(original);
    if (stillLive)
        destination = original;
    busy_ = false;
    return stillLive;
}
namespace {
GattsWriteResult capturedAdmission(const GattsWriteEvent &event, const GattsWriteBinding &binding,
                                   const IGameControlChannel &authority,
                                   FreeRtosCommandFrameQueue &queue,
                                   IWriteResponseBearer &responses,
                                   std::uint32_t receivedAt) noexcept {
    return admitGattsWrite(event, binding, authority, queue, responses, receivedAt);
}
// Exact overload for the independent profile queue.
GattsWriteResult capturedAdmission(const GattsWriteEvent &event, const GattsWriteBinding &binding,
                                   const IGameControlChannel &authority,
                                   FreeRtosProfileFrameQueue &queue,
                                   IWriteResponseBearer &responses,
                                   std::uint32_t receivedAt) noexcept {
    return admitGattsProfileWrite(event, binding, authority, queue, responses, receivedAt);
}
template <typename Queue>
GattsWriteResult admission(const GattsWriteEvent &caller, FreeRtosGattsConnection &lifecycle,
                           GattsEndpoint endpoint, const IGameControlChannel &authority,
                           Queue &queue, IWriteResponseBearer &responses,
                           std::uint32_t receivedAt) noexcept {
    auto event = caller;
    char owned[CommandFrameLimits::Frame];
    if (event.bytes && event.count && event.count <= sizeof(owned)) {
        std::memcpy(owned, event.bytes, event.count);
        event.bytes = owned;
    } else {
        event.bytes = nullptr; // No borrowed pointer crosses the first observation.
    }
    GattsWriteBinding original;
    if (!lifecycle.captureWriteNow(event, endpoint, original))
        return {};
    return capturedAdmission(event, original, authority, queue, responses, receivedAt);
}
} // namespace
GattsWriteResult
admitOriginalGattsWrite(const GattsWriteEvent &event, FreeRtosGattsConnection &lifecycle,
                        const IGameControlChannel &authority, FreeRtosCommandFrameQueue &queue,
                        IWriteResponseBearer &responses, std::uint32_t receivedAt) noexcept {
    return admission(event, lifecycle, GattsEndpoint::Command, authority, queue, responses,
                     receivedAt);
}
GattsWriteResult admitOriginalGattsProfileWrite(const GattsWriteEvent &event,
                                                FreeRtosGattsConnection &lifecycle,
                                                const IGameControlChannel &authority,
                                                FreeRtosProfileFrameQueue &queue,
                                                IWriteResponseBearer &responses,
                                                std::uint32_t receivedAt) noexcept {
    return admission(event, lifecycle, GattsEndpoint::Profile, authority, queue, responses,
                     receivedAt);
}
} // namespace companion
} // namespace larp
