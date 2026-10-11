#include "Esp32LedParkDeadline.h"

bool Esp32LedParkDeadline::begin() {
    if (handle_ != nullptr || failed_.load(std::memory_order_acquire))
        return false;
    esp_timer_create_args_t args = {};
    args.callback = callback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "led-off-deadline";
    if (esp_timer_create(&args, &handle_) != ESP_OK) {
        failed_.store(true, std::memory_order_release);
        return false;
    }
    if (xTaskCreatePinnedToCore(offTask, "LED Off", 4096U, this, 2U, &offTask_, 1) != pdPASS ||
        offTask_ == nullptr) {
        esp_timer_delete(handle_);
        handle_ = nullptr;
        failed_.store(true, std::memory_order_release);
        return false;
    }
    return true;
}

bool Esp32LedParkDeadline::arm(std::uint32_t ticket, larp::application::ILedParkPort &port) {
    if (handle_ == nullptr || ticket == 0U ||
        armedTicket_.load(std::memory_order_acquire) != 0U ||
        failed_.load(std::memory_order_acquire)) {
        failed_.store(true, std::memory_order_release);
        return false;
    }
    port_.store(&port, std::memory_order_release);
    armedTicket_.store(ticket, std::memory_order_release);
    armStartedUs_ = esp_timer_get_time();
    if (esp_timer_start_once(handle_, 400000U) != ESP_OK) {
        failed_.store(true, std::memory_order_release);
        // Caller still requests Park immediately and keeps its gate closed.
        return false;
    }
    return true;
}

void Esp32LedParkDeadline::callback(void *context) {
    Esp32LedParkDeadline &self = *static_cast<Esp32LedParkDeadline *>(context);
    const std::uint32_t ticket = self.armedTicket_.load(std::memory_order_acquire);
    auto *port = self.port_.load(std::memory_order_acquire);
    if (ticket == 0U || port == nullptr || self.offTask_ == nullptr) {
        self.failed_.store(true, std::memory_order_release);
        self.missedTicket_.store(ticket, std::memory_order_release);
        return;
    }
    self.firedTicket_.store(ticket, std::memory_order_release);
    xTaskNotifyGive(self.offTask_);
}

void Esp32LedParkDeadline::offTask(void *context) {
    Esp32LedParkDeadline &self = *static_cast<Esp32LedParkDeadline *>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const std::uint32_t ticket = self.armedTicket_.load(std::memory_order_acquire);
        auto *port = self.port_.load(std::memory_order_acquire);
        if (ticket == 0U || port == nullptr || !port->forceBlackFromDeadline(ticket)) {
            self.failed_.store(true, std::memory_order_release);
            self.missedTicket_.store(ticket, std::memory_order_release);
            continue;
        }
        self.submittedTicket_.store(ticket, std::memory_order_release);
        // Timestamp is taken after FastLED.show returned. This only bounds a
        // software submission observed in this run, not physical LED off.
        if (!larp::application::softwareOffWithinDeadline(self.armStartedUs_,
                                                          esp_timer_get_time())) {
            self.failed_.store(true, std::memory_order_release);
            self.missedTicket_.store(ticket, std::memory_order_release);
        }
    }
}

bool Esp32LedParkDeadline::firedFor(std::uint32_t ticket) const {
    return ticket != 0U && firedTicket_.load(std::memory_order_acquire) == ticket;
}

bool Esp32LedParkDeadline::missedFor(std::uint32_t ticket) const {
    return ticket != 0U && missedTicket_.load(std::memory_order_acquire) == ticket;
}

bool Esp32LedParkDeadline::independentBlackSubmittedFor(std::uint32_t ticket) const {
    return ticket != 0U && submittedTicket_.load(std::memory_order_acquire) == ticket;
}
