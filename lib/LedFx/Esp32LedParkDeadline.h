#pragma once

#include "ClosedDiagnosticArbiter.h"
#include <atomic>
#include <cstdint>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Process-lifetime timer and separate off task. Both run independently of the
// app/BLE and light-render loops; neither proves electrical WS2811 all-off.
class Esp32LedParkDeadline : public larp::application::ILedParkDeadline {
  public:
    bool begin();
    bool arm(std::uint32_t ticket, larp::application::ILedParkPort &port) override;
    bool firedFor(std::uint32_t ticket) const override;
    bool missedFor(std::uint32_t ticket) const override;
    bool independentBlackSubmittedFor(std::uint32_t ticket) const override;
    bool failed() const { return failed_.load(std::memory_order_acquire); }

  private:
    static void callback(void *context);
    static void offTask(void *context);
    esp_timer_handle_t handle_ = nullptr;
    TaskHandle_t offTask_ = nullptr;
    std::int64_t armStartedUs_ = 0;
    std::atomic<larp::application::ILedParkPort *> port_{nullptr};
    std::atomic<std::uint32_t> armedTicket_{0U};
    std::atomic<std::uint32_t> firedTicket_{0U};
    std::atomic<std::uint32_t> missedTicket_{0U};
    std::atomic<std::uint32_t> submittedTicket_{0U};
    std::atomic<bool> failed_{false};
};
