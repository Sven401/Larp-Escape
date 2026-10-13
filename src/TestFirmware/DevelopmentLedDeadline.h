#pragma once

#include "CompanionIntegration/DevelopmentLedDeadlineCore.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DevelopmentLedBlackPort {
  public:
    virtual ~DevelopmentLedBlackPort() = default;
    // Must serialize with every LED render and return only after show() returns.
    virtual bool submitBlack(std::uint32_t ticket) noexcept = 0;
};

// Repeatable test-image timer. The ESP timer task requests a separate off task;
// neither BLE callbacks nor the Arduino loop are needed to submit black.
class DevelopmentLedDeadline {
  public:
    bool begin() noexcept;
    bool arm(std::uint32_t ticket, DevelopmentLedBlackPort &port) noexcept;
    bool stop(std::uint32_t ticket) noexcept;
    bool ready() noexcept;
    bool failed() noexcept;
    bool colorAllowed(std::uint32_t ticket) noexcept;
    bool blackSubmittedFor(std::uint32_t ticket) noexcept;
    bool missedFor(std::uint32_t ticket) noexcept;

  private:
    static void timerCallback(void *context) noexcept;
    static void offTask(void *context) noexcept;
    portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    larp::diagnostic::DevelopmentLedDeadlineCore core_;
    esp_timer_handle_t timer_ = nullptr;
    TaskHandle_t offTask_ = nullptr;
    DevelopmentLedBlackPort *port_ = nullptr;
    std::uint64_t armedAtUs_ = 0;
};
