#include "ProductGattsOwner.h"
#include "GattsServiceDefinition.h"
#include <Arduino.h>
#include <esp_gatts_api.h>

using namespace larp::companion;
ProductGattsOwner *ProductGattsOwner::selected_ = nullptr;

std::uint32_t ProductGattsOwner::Clock::nowMilliseconds() const noexcept { return millis(); }

ProductGattsOwner::ProductGattsOwner(Esp32CompanionIdentityOwner &identity) noexcept
    : identity_(identity), commands_(identity.channel_), profiles_(identity.channel_),
      source_(identity.policy_, clock_, identity.channel_, commands_, profiles_),
      responses_(source_), responseWorker_(responses_, source_),
      service_(source_, responses_, clock_, driver_) {}

void ProductGattsOwner::worker(void *context) noexcept {
    auto &owner = *static_cast<ProductGattsOwner *>(context);
    owner.workerReady_.store(owner.service_.begin(), std::memory_order_relaxed);
    owner.workerStarted_.store(true, std::memory_order_release);
    for (;;) {
        if (owner.workerReady_.load(std::memory_order_relaxed)) {
            owner.service_.service();
            (void)owner.responseWorker_.service();
        }
        vTaskDelay(1);
    }
}

void ProductGattsOwner::callback(esp_gatts_cb_event_t kind, esp_gatt_if_t interface,
                                 esp_ble_gatts_cb_param_t *parameter) noexcept {
    auto *owner = selected_;
    if (owner && parameter)
        (void)owner->service_.copyEvent(fromSdkGattsService(kind, interface, *parameter));
}

ProductGattsOwner::State ProductGattsOwner::begin() noexcept {
    if (state_ != State::NotStarted)
        return state_;
    if (identity_.state() != Esp32CompanionIdentityOwner::State::ReadyClosed ||
        !commands_.begin() || !profiles_.begin() || !responses_.begin())
        return state_ = State::QueueFailed;
    if (xTaskCreatePinnedToCore(worker, "gattsClosed", 6144, this, 1, &worker_, 0) != pdPASS)
        return state_ = State::WorkerFailed;
    for (unsigned i = 0; i < 100 && !workerStarted_.load(std::memory_order_acquire); ++i)
        vTaskDelay(1);
    if (!workerStarted_.load(std::memory_order_acquire) ||
        !workerReady_.load(std::memory_order_relaxed) || selected_ ||
        !armGattsSourceBeforeSdkStart(GattsServiceDefinition::Application, source_))
        return state_ = State::HookFailed;
    selected_ = this;
    if (stack_.begin() != ClosedBleStackOwner::State::ReadyClosed)
        return state_ = State::StackFailed;
    if (esp_ble_gatts_register_callback(callback) != ESP_OK)
        return state_ = State::CallbackFailed;
    if (esp_ble_gatts_app_register(GattsServiceDefinition::Application) != ESP_OK)
        return state_ = State::RegistrationFailed;
    return state_ = State::RegistrationPending;
}
