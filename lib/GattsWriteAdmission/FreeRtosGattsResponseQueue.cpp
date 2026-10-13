#include "FreeRtosGattsResponseQueue.h"
namespace larp {
namespace companion {
namespace {
bool valid(const RoutedWriteResponse &value) noexcept {
    bool address = false;
    for (const auto byte : value.original.peerAddress)
        address = address || byte != 0;
    if (!address || !value.original.connection.token || value.original.interface == 0xff ||
        !value.original.attribute || !value.attribute)
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
        return true;
    }
    return false;
}
} // namespace
bool FreeRtosGattsResponseQueue::begin() noexcept {
    if (!queue_)
        queue_ = xQueueCreateStatic(GattsResponseLimits::Depth, sizeof(RoutedWriteResponse),
                                    storage_, &control_);
    return queue_ != nullptr;
}
bool FreeRtosGattsResponseQueue::current(const RoutedWriteResponse &value) const noexcept {
    return valid(value) &&
           context_.nowMilliseconds() - value.receivedAt <
               GattsResponseLimits::MaxAgeMilliseconds &&
           context_.originalLive(value.original);
}
bool FreeRtosGattsResponseQueue::copyWriteResponse(const RoutedWriteResponse &source) noexcept {
    if (!queue_)
        return false;
    // Freeze every scalar before any external observation; retain no pointers.
    const auto value = source;
    return current(value) && xQueueSendToBack(queue_, &value, 0) == pdTRUE;
}
bool FreeRtosGattsResponseQueue::takeCurrent(RoutedWriteResponse &destination) noexcept {
    if (!queue_)
        return false;
    for (std::size_t i = 0; i < GattsResponseLimits::Depth; ++i) {
        RoutedWriteResponse value;
        if (xQueueReceive(queue_, &value, 0) != pdTRUE)
            return false;
        if (!current(value))
            continue;
        destination = value;
        return true;
    }
    return false;
}
GattsResponseDispatch GattsResponseDispatcher::service() noexcept {
    if (busy_)
        return GattsResponseDispatch::Busy;
    busy_ = true;
    RoutedWriteResponse response;
    GattsResponseDispatch result = GattsResponseDispatch::Idle;
    if (queue_.takeCurrent(response)) {
        result = GattsResponseDispatch::Rejected;
        if (queue_.current(response) && submitter_.submitOriginal(response))
            result = GattsResponseDispatch::Submitted;
    }
    busy_ = false;
    return result;
}
} // namespace companion
} // namespace larp
