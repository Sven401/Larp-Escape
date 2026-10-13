#pragma once
#include "Companion/CommandFrameQueue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
// Process lifetime. begin before tasks/callbacks. Task-context SDK callback only,
// never ISR. SDK adapter validates service/connection/handle before this method.
// Trusted lifecycle attaches principal/epoch/time; NONE originate in JSON.
// Copied acknowledges only this fixed frame queue, not a command or execution.
class FreeRtosCommandFrameQueue : public ICommandFrameQueue {
  public:
    explicit FreeRtosCommandFrameQueue(const IGameControlChannel &authority)
        : authority_(authority) {}
    FreeRtosCommandFrameQueue(const FreeRtosCommandFrameQueue &) = delete;
    FreeRtosCommandFrameQueue &operator=(const FreeRtosCommandFrameQueue &) = delete;
    bool begin() noexcept;
    FrameCopyOutcome copyWrite(const char *bytes, std::size_t count, std::uint32_t principal,
                               std::uint32_t epoch, std::uint32_t receivedAt, bool prepared = false,
                               std::uint16_t writeOffset = 0) noexcept;
    // Sole transport owner, one fixed copy, zero wait; false preserves frame.
    bool take(CopiedCommandFrame &frame) noexcept override;

  private:
    const IGameControlChannel &authority_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(CopiedCommandFrame)
        std::uint8_t storage_[CommandFrameQueueLimits::Depth * sizeof(CopiedCommandFrame)] = {};
};
} // namespace companion
} // namespace larp
