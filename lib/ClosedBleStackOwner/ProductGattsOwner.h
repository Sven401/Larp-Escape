#pragma once

#include "ClosedBleStackOwner.h"
#include "Esp32CompanionIdentityOwner.h"
#include "Esp32GattsService.h"
#include "Esp32PeerProofCrypto.h"
#include "Esp32PeerRegistryStore.h"
#include "FreeRtosCommandFrameQueue.h"
#include "FreeRtosGattsResponseQueue.h"
#include "FreeRtosPeerRegistryRuntime.h"
#include "FreeRtosProfileFrameQueue.h"
#include <atomic>
#include <freertos/task.h>

// Closed product GATT registration. Attribute creation does not grant access:
// the identity policy remains closed, and no GAP advertising is started.
class ProductGattsOwner {
  public:
    enum class State : std::uint8_t {
        NotStarted, QueueFailed, WorkerFailed, HookFailed, StackFailed,
        CallbackFailed, RegistrationFailed, RegistrationPending
    };
    explicit ProductGattsOwner(Esp32CompanionIdentityOwner &identity) noexcept;
    ProductGattsOwner(const ProductGattsOwner &) = delete;
    ProductGattsOwner &operator=(const ProductGattsOwner &) = delete;
    State begin() noexcept;
    State state() const noexcept { return state_; }
    bool serviceReady() const noexcept { return service_.ready(); }
    // A stored public registry was validated and installed on the original
    // source actor. This does not attest a connected peer, F1 or GM authority.
    enum class RegistryState : std::uint8_t {
        NotStarted, WaitingSource, SourceUnavailable, StoreUnavailable,
        RuntimeUnavailable, RegistryRejected, StoredRegistryInstalledClosed
    };
    RegistryState registryState() const noexcept;

  private:
    class Clock : public larp::companion::IProfileImportClock {
      public:
        std::uint32_t nowMilliseconds() const noexcept override;
    } clock_;
    Esp32CompanionIdentityOwner &identity_;
    larp::companion::FreeRtosCommandFrameQueue commands_;
    larp::companion::FreeRtosProfileFrameQueue profiles_;
    larp::companion::Esp32GattsSource source_;
    larp::companion::FreeRtosGattsResponseQueue responses_;
    larp::companion::GattsResponseDispatcher responseWorker_;
    larp::companion::SdkGattsServiceDriver driver_;
    larp::companion::Esp32GattsService service_;
    Esp32PeerRegistryStore peerStore_;
    larp::companion::Esp32PeerProofCrypto peerCrypto_;
    larp::companion::FreeRtosPeerRegistryRuntime registryRuntime_;
    larp::companion::PeerRegistryOwner registryOwner_;
    larp::companion::Identity device_;
    larp::companion::BootIdentity boot_;
    ClosedBleStackOwner stack_;
    TaskHandle_t worker_ = nullptr, registryWorker_ = nullptr;
    std::atomic<bool> workerStarted_{false};
    std::atomic<bool> workerReady_{false};
    std::atomic<RegistryState> registryState_{RegistryState::NotStarted};
    State state_ = State::NotStarted;
    static ProductGattsOwner *selected_;
    static void worker(void *) noexcept;
    static void loadRegistry(void *) noexcept;
    static void callback(esp_gatts_cb_event_t, esp_gatt_if_t,
                         esp_ble_gatts_cb_param_t *) noexcept;
};
