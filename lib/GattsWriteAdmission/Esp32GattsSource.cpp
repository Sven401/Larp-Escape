#include "Esp32GattsSource.h"
#include <cstring>
#include <new>
namespace larp {
namespace companion {
namespace {
Esp32GattsSource *selected = nullptr;
std::uint16_t application = 0;
sdk446::Callbacks originalCallbacks = {};
bool registrationAttempted = false;
void connectionCallback(std::uint8_t interface, std::uint8_t *address, std::uint16_t packed,
                        bool connected, std::uint16_t reason, std::uint8_t transport) {
    if (selected && transport == 2)
        selected->connectionNow(interface, address, packed, connected);
    // Forward AFTER original lifetime invalidation; no local lock surrounds SDK.
    if (originalCallbacks.connection)
        originalCallbacks.connection(interface, address, packed, connected, reason, transport);
}
void requestCallback(std::uint16_t packed, std::uint32_t transaction, std::uint8_t type,
                     sdk446::Request *request) {
    if (selected && selected->requestNow(packed, transaction, type, request))
        return; // Selected writes never re-enter the delayed BTA/BTC write path.
    if (originalCallbacks.request)
        originalCallbacks.request(packed, transaction, type, request);
}
} // namespace
bool armGattsSourceBeforeSdkStart(std::uint16_t app, Esp32GattsSource &source) noexcept {
    if (!app || selected || btu_thread)
        return false;
    application = app;
    selected = &source;
    return true;
}
bool Esp32GattsSource::sourceTask() const noexcept {
    return sourceTask_ && sourceTask_ == xTaskGetCurrentTaskHandle() && sdkThread_ &&
           sdkThread_ == btu_thread;
}
bool Esp32GattsSource::registeredSourceNow(std::uint8_t interface) noexcept {
    if (registered_ || !interface || interface == 0xff || !btu_thread ||
        !xTaskGetCurrentTaskHandle())
        return false;
    // The wrapper invokes this synchronously from GATT_Register, whose source
    // caller is BTU. The source task/SDK instance remain immutable afterwards.
    sdkThread_ = btu_thread;
    sourceTask_ = xTaskGetCurrentTaskHandle();
    interface_ = interface;
    registered_ = true;
    return true;
}
bool Esp32GattsSource::configureEndpointsNow(std::uint16_t command, std::uint16_t profile,
                                             IWriteResponseBearer &responses) noexcept {
    if (!sourceTask() || !registered_ || owner_ || sawConnection_ || !command || !profile ||
        command == profile)
        return false;
    GattsEndpoints endpoints;
    endpoints.interface = interface_;
    endpoints.command = command;
    endpoints.profile = profile;
    auto *owner = new (ownerStorage_) FreeRtosGattsConnection(access_, clock_, endpoints);
    command_ = command;
    profile_ = profile;
    responses_ = &responses;
    portENTER_CRITICAL(&mutex_);
    owner_ = owner; // Publish once; object/endpoints never replaced or destroyed.
    portEXIT_CRITICAL(&mutex_);
    return true;
}
GattsLifecycleOutcome Esp32GattsSource::connectionNow(std::uint8_t interface,
                                                      const std::uint8_t *address,
                                                      std::uint16_t packed,
                                                      bool connected) noexcept {
    GattsConnectionEvent event;
    event.interface = interface;
    event.connection = packed; // Full private packed conn_id, not BTC's upper byte.
    if (address)
        std::memcpy(event.peerAddress.data(), address, event.peerAddress.size());
    if (!sourceTask() || interface != interface_ || (packed & 0xff) != interface)
        return GattsLifecycleOutcome::Invalid;
    sawConnection_ = true;
    if (!owner_)
        return GattsLifecycleOutcome::Unavailable;
    if (connected) {
        BoundConnection opened;
        const auto outcome = owner_->connectNow(event, opened);
        if (outcome == GattsLifecycleOutcome::Opened) {
            live_ = event;
            connection_ = opened;
            active_ = true;
        } else if (outcome == GattsLifecycleOutcome::Conflict) {
            active_ = false;
            connection_ = {};
            live_ = {};
        }
        return outcome;
    }
    const auto outcome = owner_->disconnectNow(event);
    if (outcome == GattsLifecycleOutcome::Closed || outcome == GattsLifecycleOutcome::Unavailable) {
        active_ = false;
        connection_ = {};
        live_ = {};
    }
    return outcome;
}
bool Esp32GattsSource::originalConnectionNow(BoundConnection &destination) const noexcept {
    if (!sourceTask() || !active_)
        return false;
    destination = connection_;
    return true;
}
bool Esp32GattsSource::requestNow(std::uint16_t packed, std::uint32_t transaction,
                                  std::uint8_t type, const sdk446::Request *borrowed) noexcept {
    if (type != 2 && type != 3)
        return false;
    // All write/execute callbacks of the selected app are consumed, including
    // preconfiguration/stale/unknown-attribute requests. Never delayed/rebound.
    GattsWriteEvent event;
    event.connection = packed;
    event.interface = static_cast<std::uint8_t>(packed);
    event.transaction = transaction;
    char bytes[CommandFrameLimits::Frame];
    if (type == 2 && borrowed) {
        event.attribute = borrowed->write.handle;
        event.offset = borrowed->write.offset;
        event.count = borrowed->write.length;
        event.needResponse = borrowed->write.needResponse;
        event.prepared = borrowed->write.prepared;
        if (event.count && event.count <= sizeof(bytes)) {
            std::memcpy(bytes, borrowed->write.value, event.count);
            event.bytes = bytes;
        }
    } else if (type == 3 && borrowed) {
        event.prepared = true;
        event.needResponse = true;
        // EXEC has no attribute or retained prepare buffer. Select our fixed
        // command endpoint only for the explicit unsupported error response.
        event.attribute = 0;
    }
    if (!sourceTask() || !owner_ || !responses_ || !active_ || packed != live_.connection ||
        !borrowed)
        return true;
    if (type == 3)
        event.attribute = command_;
    event.peerAddress = live_.peerAddress;
    const auto now = clock_.nowMilliseconds(); // After owned scalars and bounded bytes.
    if (event.attribute == profile_)
        admitOriginalGattsProfileWrite(event, *owner_, channel_, profiles_, *responses_, now);
    else if (event.attribute == command_)
        admitOriginalGattsWrite(event, *owner_, channel_, commands_, *responses_, now);
    return true;
}
bool Esp32GattsSource::originalLive(const GattsWriteBinding &caller) const noexcept {
    const auto original = caller;
    portENTER_CRITICAL(&mutex_);
    auto *owner = owner_;
    portEXIT_CRITICAL(&mutex_);
    return owner && owner->originalLive(original);
}
bool Esp32GattsSource::validResponse(const RoutedWriteResponse &value) const noexcept {
    if (value.attribute != value.original.attribute ||
        (value.original.connection.handle & 0xff) != value.original.interface)
        return false;
    switch (value.status) {
    case AttWriteStatus::Ok:
    case AttWriteStatus::InvalidHandle:
    case AttWriteStatus::InvalidPdu:
    case AttWriteStatus::RequestUnsupported:
    case AttWriteStatus::InvalidOffset:
    case AttWriteStatus::InsufficientAuthorization:
    case AttWriteStatus::InvalidValueLength:
    case AttWriteStatus::UnlikelyError:
    case AttWriteStatus::InsufficientResources:
        break;
    default:
        return false;
    }
    return clock_.nowMilliseconds() - value.receivedAt < GattsResponseLimits::MaxAgeMilliseconds &&
           originalLive(value.original);
}
bool Esp32GattsSource::submitOriginal(const RoutedWriteResponse &caller) noexcept {
    const auto value = caller; // Freeze before external clock/lifetime observation.
    if (posting_)
        return false;
    posting_ = true;
    if (!validResponse(value)) {
        posting_ = false;
        return false;
    }
    Pending *item = nullptr;
    portENTER_CRITICAL(&mutex_);
    for (auto &candidate : pending_) {
        if (!candidate.state) {
            candidate.response = value;
            candidate.owner = this;
            candidate.state = 1;
            item = &candidate;
            break;
        }
    }
    portEXIT_CRITICAL(&mutex_);
    // Worker may race with consumption. The slot stays occupied from reservation
    // through the real SDK call. SDK false post guarantees no callback is queued.
    const bool posted =
        item && sdkThread_ && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &consume, item, 0, 0);
    if (item && !posted) {
        portENTER_CRITICAL(&mutex_);
        item->state = 0;
        portEXIT_CRITICAL(&mutex_);
    }
    posting_ = false;
    return posted; // Local SDK posting only; no peer-delivery or execution claim.
}
void Esp32GattsSource::consume(void *context) noexcept {
    auto *item = static_cast<Pending *>(context);
    if (!item || !item->owner)
        return;
    auto &source = *item->owner;
    portENTER_CRITICAL(&source.mutex_);
    const bool occupied = item->state == 1;
    if (occupied)
        item->state = 2;
    const auto value = item->response;
    portEXIT_CRITICAL(&source.mutex_);
    if (!occupied)
        return;
    // Same BTU actor as earliest conn/write callbacks. No BTC or BTA re-post:
    // final original/age check followed directly by actual GATT processing.
    if (source.sourceTask() && source.validResponse(value))
        GATTS_SendRsp(value.original.connection.handle, value.transaction,
                      static_cast<std::uint8_t>(value.status), nullptr);
    portENTER_CRITICAL(&source.mutex_);
    item->state = 0;
    portEXIT_CRITICAL(&source.mutex_);
    // SDK status is a local result only. Failure never retries or frees input.
}
} // namespace companion
} // namespace larp
extern "C" std::uint8_t __wrap_GATT_Register(larp::companion::sdk446::Uuid *uuid,
                                             const larp::companion::sdk446::Callbacks *callbacks) {
    using namespace larp::companion;
    if (!selected || !uuid || uuid->len != 2 || uuid->uu.uuid16 != application)
        return __real_GATT_Register(uuid, callbacks);
    if (registrationAttempted || !callbacks || !callbacks->connection || !callbacks->request)
        return 0; // One selected registration per boot; fail closed on retry/reuse.
    registrationAttempted = true;
    originalCallbacks = *callbacks;
    auto installed = *callbacks;
    installed.connection = &connectionCallback;
    installed.request = &requestCallback;
    const auto interface = __real_GATT_Register(uuid, &installed);
    if (!selected->registeredSourceNow(interface))
        return 0;
    return interface;
}
