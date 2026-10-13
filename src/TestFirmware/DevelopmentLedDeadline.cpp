#include "DevelopmentLedDeadline.h"

bool DevelopmentLedDeadline::begin() noexcept {
    if (timer_ || offTask_) return false;
    esp_timer_create_args_t args = {};
    args.callback = timerCallback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "dev-led-off";
    if (esp_timer_create(&args, &timer_) != ESP_OK) return false;
    if (xTaskCreatePinnedToCore(offTask, "Dev LED Off", 4096U, this, 2U,
                                &offTask_, 1) != pdPASS || !offTask_) {
        esp_timer_delete(timer_);
        timer_ = nullptr;
        return false;
    }
    return true;
}

bool DevelopmentLedDeadline::arm(std::uint32_t ticket,
                                 DevelopmentLedBlackPort &port) noexcept {
    if (!timer_ || !offTask_) return false;
    portENTER_CRITICAL(&mutex_);
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    const bool accepted = core_.arm(ticket, now);
    if (accepted) {
        port_ = &port;
        armedAtUs_ = now;
    }
    portEXIT_CRITICAL(&mutex_);
    if (!accepted) return false;
    if (esp_timer_start_once(timer_, larp::diagnostic::DevelopmentLedDeadlineCore::TimerUs) != ESP_OK) {
        portENTER_CRITICAL(&mutex_);
        core_.fault(ticket);
        portEXIT_CRITICAL(&mutex_);
        return false; // No LED output may start after a failed arm.
    }
    return true;
}

bool DevelopmentLedDeadline::stop(std::uint32_t ticket) noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool accepted = core_.requestStop(ticket);
    portEXIT_CRITICAL(&mutex_);
    if (accepted) xTaskNotifyGive(offTask_);
    return accepted;
}

bool DevelopmentLedDeadline::ready() noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool value = timer_ && offTask_ && core_.ready();
    portEXIT_CRITICAL(&mutex_);
    return value;
}

bool DevelopmentLedDeadline::failed() noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool value = core_.faulted();
    portEXIT_CRITICAL(&mutex_);
    return value;
}

bool DevelopmentLedDeadline::colorAllowed(std::uint32_t ticket) noexcept {
    portENTER_CRITICAL(&mutex_);
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    const bool value = ticket && core_.activeTicket() == ticket &&
        !core_.faulted() && !core_.needsBlack() &&
        !core_.blackSubmittedFor(ticket) && now >= armedAtUs_ &&
        now - armedAtUs_ < 300000U;
    portEXIT_CRITICAL(&mutex_);
    return value;
}

bool DevelopmentLedDeadline::blackSubmittedFor(std::uint32_t ticket) noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool value = core_.blackSubmittedFor(ticket);
    portEXIT_CRITICAL(&mutex_);
    return value;
}

bool DevelopmentLedDeadline::missedFor(std::uint32_t ticket) noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool value = core_.missedFor(ticket);
    portEXIT_CRITICAL(&mutex_);
    return value;
}

void DevelopmentLedDeadline::timerCallback(void *context) noexcept {
    auto &self = *static_cast<DevelopmentLedDeadline *>(context);
    portENTER_CRITICAL(&self.mutex_);
    const auto ticket = self.core_.activeTicket();
    const bool accepted = self.core_.timerFired(ticket);
    const bool needsBlack = self.core_.needsBlack();
    portEXIT_CRITICAL(&self.mutex_);
    if (accepted && needsBlack) xTaskNotifyGive(self.offTask_);
}

void DevelopmentLedDeadline::offTask(void *context) noexcept {
    auto &self = *static_cast<DevelopmentLedDeadline *>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        portENTER_CRITICAL(&self.mutex_);
        const auto ticket = self.core_.activeTicket();
        auto *port = self.core_.needsBlack() ? self.port_ : nullptr;
        portEXIT_CRITICAL(&self.mutex_);
        if (!ticket || !port) continue;
        const bool submitted = port->submitBlack(ticket);
        portENTER_CRITICAL(&self.mutex_);
        if (!submitted) self.core_.fault(ticket);
        else self.core_.blackSubmitted(ticket,
            static_cast<std::uint64_t>(esp_timer_get_time()));
        portEXIT_CRITICAL(&self.mutex_);
    }
}
