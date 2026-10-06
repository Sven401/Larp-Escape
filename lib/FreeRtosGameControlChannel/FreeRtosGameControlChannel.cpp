#include "FreeRtosGameControlChannel.h"
#include <type_traits>
namespace larp {
namespace companion {
static_assert(std::is_trivially_copyable<TrustedGameRequest>::value,
              "Queue requires copied command values");
static_assert(std::is_trivially_copyable<RoutedGameReply>::value,
              "Queue requires copied reply values");
static_assert(std::is_trivially_copyable<ConfiguredStatus>::value,
              "Queue requires copied status values");
bool FreeRtosGameControlChannel::begin() {
    if (started_)
        return false;
    commands_ = xQueueCreateStatic(ControlLimits::Queue, sizeof(TrustedGameRequest), commandBytes_,
                                   &commandControl_);
    replies_ = xQueueCreateStatic(ControlLimits::Queue, sizeof(RoutedGameReply), replyBytes_,
                                  &replyControl_);
    status_ = xQueueCreateStatic(1, sizeof(ConfiguredStatus), statusBytes_, &statusControl_);
    started_ = commands_ && replies_ && status_;
    return started_;
}
bool FreeRtosGameControlChannel::receive(TrustedGameRequest &request) noexcept {
    return started_ && xQueueReceive(commands_, &request, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::send(const RoutedGameReply &reply) noexcept {
    return started_ && xQueueSendToBack(replies_, &reply, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::publish(const ConfiguredStatus &status) noexcept {
    return started_ && xQueueOverwrite(status_, &status) == pdTRUE;
}
bool FreeRtosGameControlChannel::submit(const TrustedGameRequest &request) {
    return started_ && request.principal != 0 && xQueueSendToBack(commands_, &request, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::takeReply(RoutedGameReply &reply) {
    return started_ && xQueueReceive(replies_, &reply, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::readStatus(ConfiguredStatus &status) const {
    return started_ && xQueuePeek(status_, &status, 0) == pdTRUE;
}
} // namespace companion
} // namespace larp
