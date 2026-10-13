#include "EspNowRuntime.h"
#include <WiFi.h>
#include <cstring>

EspNowRuntime *EspNowRuntime::callbackOwner_ = nullptr;

bool EspNowRuntime::begin() {
    if (beginAttempted_)
        return initialized_;
    beginAttempted_ = true;
    if (callbackOwner_ != nullptr)
        return false;
    queue_ = xQueueCreate(queueCapacity, sizeof(SendStatusEvent));
    if (queue_ == nullptr || !WiFi.mode(WIFI_STA))
        return false;
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK)
        return false;
    // Published before registration and unchanged thereafter. If initialization
    // fails, setup restarts the device; no callback-owned queue is deleted.
    callbackOwner_ = this;
    if (esp_now_register_send_cb(onSent) != ESP_OK)
        return false;
    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, receiver_.data(), receiver_.size());
    peer.channel = 0U;
    peer.encrypt = false;
    initialized_ = esp_now_add_peer(&peer) == ESP_OK;
    return initialized_;
}
void EspNowRuntime::observeDepth(std::uint32_t depth) {
    std::uint32_t previous = maximumDepth_.load(std::memory_order_relaxed);
    while (depth > previous &&
           !maximumDepth_.compare_exchange_weak(previous, depth, std::memory_order_relaxed)) {
    }
}
void EspNowRuntime::onSent(const std::uint8_t *mac, esp_now_send_status_t status) {
    (void)mac;
    EspNowRuntime *owner = callbackOwner_;
    if (owner == nullptr)
        return;
    const SendStatusEvent event{status};
    if (xQueueSend(owner->queue_, &event, 0U) != pdTRUE) {
        owner->dropped_.fetch_add(1U, std::memory_order_relaxed);
        owner->observeDepth(queueCapacity);
    } else
        owner->observeDepth(uxQueueMessagesWaiting(owner->queue_));
}
void EspNowRuntime::service() {
    if (!initialized_)
        return;
    observeDepth(uxQueueMessagesWaiting(queue_));
    SendStatusEvent event;
    for (UBaseType_t drained = 0U; drained < queueCapacity; ++drained) {
        if (xQueueReceive(queue_, &event, 0U) != pdTRUE)
            break;
        if (event.status == ESP_NOW_SEND_SUCCESS)
            ++successes_;
        else
            ++failures_;
    }
}
