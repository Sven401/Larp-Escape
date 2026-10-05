#pragma once

#include "GamePorts/IRuntimeService.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// One process-lifetime instance. begin runs before any messages are sent.
// The WiFi callback copies status; only service() owns the delivery counters.
class EspNowRuntime : public larp::ports::IRuntimeService {
  public:
    explicit EspNowRuntime(const std::array<std::uint8_t, 6> &receiver) : receiver_(receiver) {}
    EspNowRuntime(const EspNowRuntime &) = delete;
    EspNowRuntime &operator=(const EspNowRuntime &) = delete;
    bool begin();
    void service() override;
    std::uint32_t droppedStatusCount() const {
        return dropped_.load(std::memory_order_relaxed);
    }
    std::uint32_t maximumObservedQueueDepth() const {
        return maximumDepth_.load(std::memory_order_relaxed);
    }
    std::uint32_t successfulDeliveryCount() const {
        return successes_;
    }
    std::uint32_t failedDeliveryCount() const {
        return failures_;
    }

  private:
    static constexpr UBaseType_t queueCapacity = 8U;
    struct SendStatusEvent {
        esp_now_send_status_t status;
    };
    static EspNowRuntime *callbackOwner_;
    static void onSent(const std::uint8_t *mac, esp_now_send_status_t status);
    void observeDepth(std::uint32_t depth);
    std::array<std::uint8_t, 6> receiver_;
    QueueHandle_t queue_ = nullptr;
    std::atomic<std::uint32_t> dropped_{0U};
    std::atomic<std::uint32_t> maximumDepth_{0U};
    std::uint32_t successes_ = 0U;
    std::uint32_t failures_ = 0U;
    bool initialized_ = false;
    bool beginAttempted_ = false;
};
