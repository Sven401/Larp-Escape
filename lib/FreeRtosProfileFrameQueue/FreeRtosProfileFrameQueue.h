#pragma once
#include "Companion/ProfileFrameQueue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
// Process lifetime, begin before callbacks. Task-context SDK callback only, no
// ISR/filesystem/parser/tick/wait. Trusted lifecycle supplies original route/time.
class FreeRtosProfileFrameQueue : public IProfileFrameQueue {
  public:
    explicit FreeRtosProfileFrameQueue(const IGameControlChannel &authority)
        : authority_(authority) {}
    FreeRtosProfileFrameQueue(const FreeRtosProfileFrameQueue &) = delete;
    FreeRtosProfileFrameQueue &operator=(const FreeRtosProfileFrameQueue &) = delete;
    bool begin() noexcept;
    ProfileCopyOutcome copyWrite(const char *bytes, std::size_t count, std::uint32_t principal,
                                 std::uint32_t epoch, std::uint32_t receivedAt,
                                 bool prepared = false, std::uint16_t writeOffset = 0) noexcept;
    // Sole owner, one zero-wait copy; false preserves destination.
    bool take(CopiedProfileFrame &frame) noexcept override;

  private:
    const IGameControlChannel &authority_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(CopiedProfileFrame)
        std::uint8_t storage_[ProfileFrameQueueLimits::Depth * sizeof(CopiedProfileFrame)] = {};
};
} // namespace companion
} // namespace larp
