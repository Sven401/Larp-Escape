#pragma once
#include "Companion/ProfileResponseQueue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
// Process-lifetime static output. begin before workers, sole producer/consumer;
// no ISR/profile/parser/filesystem/wait/tick. Current authority is synchronized.
class FreeRtosProfileResponseQueue : public IProfileResponseQueue {
  public:
    FreeRtosProfileResponseQueue(const IGameControlChannel &authority, IProfileImportClock &clock)
        : authority_(authority), clock_(clock) {}
    FreeRtosProfileResponseQueue(const FreeRtosProfileResponseQueue &) = delete;
    FreeRtosProfileResponseQueue &operator=(const FreeRtosProfileResponseQueue &) = delete;
    bool begin() noexcept;
    ProfileResponseCopy copyResponse(const RoutedProfileResponse &) noexcept override;
    bool takeCurrent(RoutedProfileResponse &) noexcept override;

  private:
    const IGameControlChannel &authority_;
    IProfileImportClock &clock_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(RoutedProfileResponse) std::uint8_t
        storage_[ProfileResponseQueueLimits::Depth * sizeof(RoutedProfileResponse)] = {};
    bool current(const TrustedAuthority &) noexcept;
};
} // namespace companion
} // namespace larp
