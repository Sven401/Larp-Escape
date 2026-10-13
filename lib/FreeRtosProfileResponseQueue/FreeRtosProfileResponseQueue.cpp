#include "FreeRtosProfileResponseQueue.h"
namespace larp {
namespace companion {
bool FreeRtosProfileResponseQueue::begin() noexcept {
    if (!queue_)
        queue_ = xQueueCreateStatic(ProfileResponseQueueLimits::Depth,
                                    sizeof(RoutedProfileResponse), storage_, &control_);
    return queue_ != nullptr;
}
bool FreeRtosProfileResponseQueue::current(const TrustedAuthority &original) noexcept {
    TrustedAuthority value;
    return original.gameMaster && original.principal && original.epoch &&
           authority_.readAuthority(value) && value == original;
}
ProfileResponseCopy
FreeRtosProfileResponseQueue::copyResponse(const RoutedProfileResponse &source) noexcept {
    if (!queue_)
        return ProfileResponseCopy::Uninitialized;
    // Freeze ALL producer scalars before clock or authority observations.
    const auto value = source;
    if (!value.response.valid())
        return ProfileResponseCopy::Invalid;
    if (clock_.nowMilliseconds() - value.producedAt >= ProfileControlLimits::ResponseMilliseconds)
        return ProfileResponseCopy::Expired;
    if (!current(value.original))
        return ProfileResponseCopy::Unauthorized;
    // A late closure may leave an old copied item. Consumer/bearer reobserve,
    // never relabel to the latest authority, never refresh production time.
    return xQueueSendToBack(queue_, &value, 0) == pdTRUE ? ProfileResponseCopy::Copied
                                                         : ProfileResponseCopy::QueueFull;
}
bool FreeRtosProfileResponseQueue::takeCurrent(RoutedProfileResponse &destination) noexcept {
    if (!queue_)
        return false;
    for (std::size_t i = 0; i < ProfileResponseQueueLimits::Depth; ++i) {
        RoutedProfileResponse value;
        if (xQueueReceive(queue_, &value, 0) != pdTRUE)
            return false;
        if (!value.response.valid() ||
            clock_.nowMilliseconds() - value.producedAt >=
                ProfileControlLimits::ResponseMilliseconds ||
            !current(value.original))
            continue;
        destination = value;
        return true;
    }
    return false;
}
} // namespace companion
} // namespace larp
