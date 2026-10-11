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
      service_(source_, responses_, clock_, driver_),
      registryRuntime_(source_, identity.channel_, clock_),
      registryOwner_(peerStore_, peerCrypto_, registryRuntime_) {}

ProductGattsOwner::RegistryState ProductGattsOwner::registryState() const noexcept {
    const auto published = registryState_.load(std::memory_order_acquire);
    if (published != RegistryState::StoredRegistryInstalledClosed)
        return published;
    PeerRegistrySnapshot installed;
    TrustedAuthority authority;
    return registryRuntime_.snapshot(installed) && installed.device == device_ &&
                   !identity_.authority(authority)
               ? published
               : RegistryState::RegistryRejected;
}

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

void ProductGattsOwner::loadRegistry(void *context) noexcept {
    auto &owner = *static_cast<ProductGattsOwner *>(context);
    const auto started = owner.clock_.nowMilliseconds();
    while (!owner.service_.ready()) {
        if (owner.service_.snapshot().phase == GattsServicePhase::Failed ||
            owner.clock_.nowMilliseconds() - started >= 35000U) {
            owner.registryRuntime_.fence();
            owner.peerStore_.fence();
            owner.registryState_.store(RegistryState::SourceUnavailable, std::memory_order_release);
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelay(1);
    }
    if (!owner.peerStore_.begin()) {
        owner.peerStore_.fence();
        owner.registryState_.store(RegistryState::StoreUnavailable, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }
    if (!owner.registryRuntime_.begin(owner.device_)) {
        owner.registryRuntime_.fence();
        owner.peerStore_.fence();
        owner.registryState_.store(RegistryState::RuntimeUnavailable, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }
    auto outcome = owner.registryOwner_.begin(owner.device_);
    const auto registryStarted = owner.clock_.nowMilliseconds();
    while (outcome == PeerRegistryOutcome::Pending &&
           owner.clock_.nowMilliseconds() - registryStarted < 35000U) {
        vTaskDelay(1);
        outcome = owner.registryOwner_.service();
    }
    PeerRegistrySnapshot installed, loaded;
    TrustedAuthority authority;
    if ((outcome == PeerRegistryOutcome::Loaded || outcome == PeerRegistryOutcome::Empty) &&
        owner.registryRuntime_.snapshot(installed) && owner.registryOwner_.snapshot(loaded) &&
        installed == loaded && installed.device == owner.device_ &&
        !owner.identity_.authority(authority)) {
        owner.registryState_.store(RegistryState::StoredRegistryInstalledClosed,
                                   std::memory_order_release);
    } else {
        owner.registryRuntime_.fence();
        owner.peerStore_.fence();
        owner.registryState_.store(RegistryState::RegistryRejected, std::memory_order_release);
    }
    vTaskDelete(nullptr);
}

ProductGattsOwner::State ProductGattsOwner::begin() noexcept {
    if (state_ != State::NotStarted)
        return state_;
    if (identity_.state() != Esp32CompanionIdentityOwner::State::ReadyClosed ||
        !identity_.identity(device_) || !identity_.bootIdentity(boot_) ||
        !boot_.valid() ||
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
    registryState_.store(RegistryState::WaitingSource, std::memory_order_release);
    if (xTaskCreatePinnedToCore(loadRegistry, "registryClosed", 8192, this, 1,
                                &registryWorker_, 0) != pdPASS) {
        registryState_.store(RegistryState::RuntimeUnavailable, std::memory_order_release);
        return state_ = State::WorkerFailed;
    }
    return state_ = State::RegistrationPending;
}
