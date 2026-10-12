#pragma once

#include "ClosedBleStackOwner.h"
#include "ClosedBleAdvertiseGate.h"
#include "ClosedBleSecurityGate.h"
#include "ProductGattsOwnerTelemetry.h"
#include "Esp32CompanionIdentityOwner.h"
#include "Esp32GattsService.h"
#include "DiagnosticGattsService.h"
#include "Esp32PeerProofCrypto.h"
#include "Esp32PeerRegistryStore.h"
#include "FreeRtosCommandFrameQueue.h"
#include "FreeRtosGattsResponseQueue.h"
#include "FreeRtosPeerRegistryRuntime.h"
#include "FreeRtosPeerProofGrantRuntime.h"
#include "FreeRtosPeerChallengeReadRuntime.h"
#include "FreeRtosPeerChallengeIssueRuntime.h"
#include "FreeRtosPeerAuthRuntime.h"
#include "Esp32PeerProofNonce.h"
#include "Companion/PeerProofReceiver.h"
#include "FreeRtosProfileFrameQueue.h"
#include <atomic>
#include <new>
#include <type_traits>
#include <esp_gap_ble_api.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Closed product GATT registration and discovery-only GAP owner. Attribute
// creation/advertising does not grant a peer role or diagnostic access.
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
    const ClosedBleStartupAttempt::Report &stackReport() const noexcept {
        return stack_.report();
    }
    bool serviceReady() const noexcept {
        return service_ && service_->ready() && diagnosticService_.ready();
    }
    bool diagnosticServiceReady() const noexcept { return diagnosticService_.ready(); }
    enum class PeerAuthState : std::uint8_t {
        NotStarted, WaitingSource, ReadyClosed, Unavailable
    };
    PeerAuthState peerAuthState() const noexcept {
        return peerAuthState_.load(std::memory_order_acquire);
    }
    // A stored public registry was validated and installed on the original
    // source actor. This does not attest a connected peer, F1 or GM authority.
    enum class RegistryState : std::uint8_t {
        NotStarted, WaitingSource, SourceUnavailable, StoreUnavailable,
        RuntimeUnavailable, RegistryRejected, StoredRegistryInstalledClosed
    };
    RegistryState registryState() const noexcept;
    ClosedBleAdvertiseGate::Phase advertisingState() const noexcept {
        return advertisingState_.load(std::memory_order_acquire);
    }
    // Current original BTU birth only. False also covers stale/disconnected.
    bool originalLink(larp::companion::PeerTransportBinding &) const noexcept;
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
    bool configureDevelopmentControl(larp::diagnostic::DevelopmentControl &) noexcept;
    bool takeDevelopmentRawWork(
        larp::companion::Esp32GattsSource::DevelopmentRawWork &) noexcept;
    bool postDevelopmentRawResult(
        const larp::companion::Esp32GattsSource::DevelopmentRawResult &) noexcept;
#endif

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
    larp::diagnostic::DiagnosticGattsService diagnosticService_;
    Esp32PeerRegistryStore peerStore_;
    larp::companion::Esp32PeerProofCrypto peerCrypto_;
    larp::companion::FreeRtosPeerRegistryRuntime registryRuntime_;
    larp::companion::PeerRegistryOwner registryOwner_;
    larp::companion::Identity device_;
    larp::companion::BootIdentity boot_;
    // Construct only after ReadyClosed yields the immutable boot identity.
    // Both objects and all referenced ports then live for the process lifetime.
    struct PeerAuthGraph {
        larp::companion::Esp32PeerProofNonce nonce;
        larp::companion::FreeRtosPeerProofGrantRuntime grant;
        larp::companion::PeerProofOwner proof;
        larp::companion::FreeRtosPeerChallengeReadRuntime reader;
        larp::companion::FreeRtosPeerChallengeIssueRuntime issuer;
        larp::companion::PeerProofReceiver receiver;
        larp::companion::FreeRtosPeerAuthRuntime auth;
        PeerAuthGraph(larp::companion::Esp32GattsSource &source,
                      const larp::companion::FreeRtosPeerRegistryRuntime &registry,
                      const larp::companion::IProfileImportClock &clock,
                      const larp::companion::BootIdentity &boot,
                      larp::companion::Esp32PeerProofCrypto &crypto) noexcept
            : grant(source, registry, clock, boot), proof(grant, crypto, nonce, clock),
              reader(source, proof, grant, clock),
              issuer(source, proof, grant, registry, clock), receiver(proof),
              auth(source, proof, receiver, grant, clock) {}
        larp::companion::PeerChallengeReadSetup setup(
            const larp::companion::BootIdentity &boot) noexcept {
            larp::companion::PeerChallengeReadSetup result;
            result.boot = boot;
            result.context = &grant;
            result.ingress = &reader;
            result.issue = &issuer;
            result.auth = &auth;
            return result;
        }
    };
    std::aligned_storage<sizeof(PeerAuthGraph), alignof(PeerAuthGraph)>::type
        peerAuthStorage_;
    std::aligned_storage<sizeof(larp::companion::Esp32GattsService),
                         alignof(larp::companion::Esp32GattsService)>::type serviceStorage_;
    PeerAuthGraph *peerAuth_ = nullptr;
    larp::companion::Esp32GattsService *service_ = nullptr;
    std::array<std::uint8_t, 16> advertisedService_ =
        larp::companion::GattsServiceDefinition::service();
    esp_ble_adv_data_t advertiseData_ = {};
    esp_ble_adv_params_t advertiseParams_ = {};
    ClosedBleStackOwner stack_;
    TaskHandle_t worker_ = nullptr, registryWorker_ = nullptr, peerAuthWorker_ = nullptr;
    std::atomic<bool> workerStarted_{false};
    std::atomic<bool> workerReady_{false};
    std::atomic<RegistryState> registryState_{RegistryState::NotStarted};
    std::atomic<PeerAuthState> peerAuthState_{PeerAuthState::NotStarted};
    std::atomic<ClosedBleAdvertiseGate::Phase> advertisingState_{
        ClosedBleAdvertiseGate::Phase::Closed};
    std::atomic<std::uint8_t> dataAck_{0}, startAck_{0}, stopAck_{0};
    // GAP callback and discovery worker enqueue copied, key-free observations.
    // The fixed queue never blocks the BTU task; overflow is counted.
    enum class SecurityEventKind : std::uint8_t {
        Request = 1, Authentication = 2, LinkPublished = 3,
        LinkGone = 4, LinkMissed = 5
    };
    struct SecurityEvent {
        std::uint32_t atMs = 0, birth = 0, token = 0;
        std::int32_t sdk = 0;
        std::uint16_t connection = 0;
        SecurityEventKind kind = SecurityEventKind::Request;
        std::uint8_t reason = 0, authMode = 0;
        bool peerMatch = false, originalPublished = false, accepted = false;
    };
    QueueHandle_t securityEvents_ = nullptr;
    std::atomic<std::uint32_t> securityEventCount_{0}, securityEventDropped_{0};
    std::atomic<std::uint32_t> securityRequestCount_{0}, authenticationCount_{0};
    void recordSecurityEvent(const SecurityEvent &) noexcept;
    State state_ = State::NotStarted;
    static ProductGattsOwner *selected_;
    static void worker(void *) noexcept;
    static void runPeerAuth(void *) noexcept;
    static void loadRegistry(void *) noexcept;
    static void gapCallback(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *) noexcept;
    void runDiscovery() noexcept;
    void quiesceAfterStart(bool startTerminal, bool startSucceeded) noexcept;
    static void callback(esp_gatts_cb_event_t, esp_gatt_if_t,
                         esp_ble_gatts_cb_param_t *) noexcept;
};
