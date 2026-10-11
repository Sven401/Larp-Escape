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
      diagnosticService_(source_, clock_),
      registryRuntime_(source_, identity.channel_, clock_),
      registryOwner_(peerStore_, peerCrypto_, registryRuntime_) {}

ProductGattsOwner::RegistryState ProductGattsOwner::registryState() const noexcept {
    const auto published = registryState_.load(std::memory_order_acquire);
    if (published != RegistryState::StoredRegistryInstalledClosed)
        return published;
    PeerRegistrySnapshot installed;
    return registryRuntime_.snapshot(installed) && installed.device == device_ &&
                   source_.closedChannelState(device_)
               ? published
               : RegistryState::RegistryRejected;
}
bool ProductGattsOwner::originalLink(PeerTransportBinding &destination) const noexcept {
    return serviceReady() &&
           registryState() == RegistryState::StoredRegistryInstalledClosed &&
           source_.readOriginalTransport(destination);
}
void ProductGattsOwner::gapCallback(esp_gap_ble_cb_event_t event,
                                    esp_ble_gap_cb_param_t *parameter) noexcept {
    auto *owner = selected_;
    if (!owner || !parameter)
        return;
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT)
        owner->dataAck_.store(parameter->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS ? 1 : 2,
                              std::memory_order_release);
    else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT)
        owner->startAck_.store(parameter->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS ? 1 : 2,
                               std::memory_order_release);
    else if (event == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT)
        owner->stopAck_.store(parameter->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS ? 1 : 2,
                              std::memory_order_release);
    else if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        std::array<std::uint8_t, 6> address;
        for (unsigned i = 0; i < address.size(); ++i)
            address[i] = parameter->ble_security.ble_req.bd_addr[i];
        PeerTransportBinding before, after;
        const bool observed = owner->source_.readOriginalTransport(before);
        const bool closedBefore = owner->serviceReady() &&
                                  owner->registryState() == RegistryState::StoredRegistryInstalledClosed &&
                                  owner->source_.closedChannelState(owner->device_);
        const bool current = owner->source_.readOriginalTransport(after);
        const bool accept = observed && closedBefore && current &&
                            ClosedBleSecurityGate::allow(owner->serviceReady(),
                                owner->registryState() == RegistryState::StoredRegistryInstalledClosed,
                                owner->source_.closedChannelState(owner->device_),
                                before, after, address);
        const auto posted = esp_ble_gap_security_rsp(parameter->ble_security.ble_req.bd_addr,
                                                      accept) == ESP_OK;
        owner->securityRequestObservation_.store(
            posted ? (accept ? 1 : 2) : (accept ? 3 : 4), std::memory_order_release);
    } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
        const auto &result = parameter->ble_security.auth_cmpl;
        owner->authenticationObservation_.store(
            result.success ? 1U : static_cast<std::uint16_t>(0x100U | result.fail_reason),
            std::memory_order_release);
    }
}
void ProductGattsOwner::quiesceAfterStart(bool startTerminal,
                                          bool startSucceeded) noexcept {
    ClosedBleRadioFence fence(startTerminal, startSucceeded);
    advertisingState_.store(ClosedBleAdvertiseGate::Phase::Quiescing,
                            std::memory_order_release);
    // Attempt a stop even if the start post is still pending. That attempt is
    // not enough if a late positive start ACK subsequently arrives.
    stopAck_.store(0, std::memory_order_release);
    const bool initialStop = esp_ble_gap_stop_advertising() == ESP_OK;
    if (startTerminal && startSucceeded)
        fence.stopPosted(initialStop);
    else if (!startTerminal)
        fence.earlyStopPosted(initialStop);
    const auto startedAt = clock_.nowMilliseconds();
    std::uint32_t lastAttemptAt = startedAt;
    for (;;) {
        const auto stop = stopAck_.exchange(0, std::memory_order_acquire);
        if (stop)
            fence.stopComplete(stop == 1);
        const auto start = startAck_.exchange(0, std::memory_order_acquire);
        if (start)
            fence.startComplete(start == 1);
        if (fence.closed()) {
            advertisingState_.store(ClosedBleAdvertiseGate::Phase::FencedClosed,
                                    std::memory_order_release);
            return;
        }
        const auto now = clock_.nowMilliseconds();
        if (fence.needsStopAfterStart() && now - lastAttemptAt >= 100U) {
            fence.stopPosted(esp_ble_gap_stop_advertising() == ESP_OK);
            lastAttemptAt = now;
        }
        if (now - startedAt >= 10000U)
            advertisingState_.store(ClosedBleAdvertiseGate::Phase::RadioUnconfirmed,
                                    std::memory_order_release);
        vTaskDelay(1);
    }
}
void ProductGattsOwner::runDiscovery() noexcept {
    ClosedBleAdvertiseGate gate;
    const auto publish = [&]() {
        advertisingState_.store(gate.phase(), std::memory_order_release);
    };
    if (!gate.configure(serviceReady(),
                        registryState() == RegistryState::StoredRegistryInstalledClosed,
                        source_.closedChannelState(device_))) {
        publish();
        return;
    }
    publish();
    // No IO path proves MITM on this hardware. Bonding may be requested by the
    // phone, but no bond/security result is treated as product authority.
    std::uint8_t auth = ESP_LE_AUTH_BOND;
    std::uint8_t io = ESP_IO_CAP_NONE;
    if (esp_ble_gap_register_callback(gapCallback) != ESP_OK ||
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, sizeof(auth)) != ESP_OK ||
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &io, sizeof(io)) != ESP_OK) {
        gate.fail();
        publish();
        return;
    }
    advertiseData_.set_scan_rsp = false;
    advertiseData_.include_name = false;
    advertiseData_.service_uuid_len = advertisedService_.size();
    advertiseData_.p_service_uuid = advertisedService_.data();
    advertiseData_.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    advertiseParams_.adv_int_min = 0x100;
    advertiseParams_.adv_int_max = 0x180;
    advertiseParams_.adv_type = ADV_TYPE_IND;
    advertiseParams_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    advertiseParams_.channel_map = ADV_CHNL_ALL;
    advertiseParams_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    if (esp_ble_gap_config_adv_data(&advertiseData_) != ESP_OK) {
        gate.fail();
        publish();
        return;
    }
    std::uint32_t connectedBirth = 0, advertisedBirth = 0;
    std::uint32_t pendingAt = clock_.nowMilliseconds();
    bool startPosted = false, startTerminal = false, startSucceeded = false;
    std::uint8_t securityLogs = 0, authenticationLogs = 0;
    const auto abort = [&]() {
        gate.fail();
        if (startPosted)
            quiesceAfterStart(startTerminal, startSucceeded);
        else
            publish();
    };
    for (;;) {
        if (!serviceReady() ||
            registryState() != RegistryState::StoredRegistryInstalledClosed) {
            abort();
            return;
        }
        if ((gate.phase() == ClosedBleAdvertiseGate::Phase::DataPending ||
             gate.phase() == ClosedBleAdvertiseGate::Phase::StartPending) &&
            clock_.nowMilliseconds() - pendingAt >= 10000U) {
            abort();
            return;
        }
        const auto data = dataAck_.exchange(0, std::memory_order_acquire);
        if (data) {
            if (!gate.dataComplete(data == 1) ||
                !source_.readOriginalLifecycle(advertisedBirth)) {
                abort();
                return;
            }
            startPosted = true;
            if (esp_ble_gap_start_advertising(&advertiseParams_) != ESP_OK) {
                abort();
                return;
            }
            startTerminal = false;
            startSucceeded = false;
            pendingAt = clock_.nowMilliseconds();
        }
        const auto started = startAck_.exchange(0, std::memory_order_acquire);
        if (started) {
            startTerminal = true;
            startSucceeded = started == 1;
            if (!gate.startComplete(startSucceeded)) {
                abort();
                return;
            }
        }
        PeerTransportBinding link;
        if (gate.phase() == ClosedBleAdvertiseGate::Phase::Visible &&
            source_.readOriginalTransport(link) && gate.connected())
            connectedBirth = link.lifecycle;
        if (gate.phase() == ClosedBleAdvertiseGate::Phase::Visible &&
            !source_.readOriginalTransport(link)) {
            std::uint32_t lifecycle = 0;
            if (source_.readOriginalLifecycle(lifecycle) && lifecycle > advertisedBirth) {
                if (!gate.missedOriginalBirth(serviceReady(),
                                              registryState() == RegistryState::StoredRegistryInstalledClosed,
                                              source_.closedChannelState(device_))) {
                    abort();
                    return;
                }
                advertisedBirth = lifecycle;
                startTerminal = false;
                startSucceeded = false;
                startPosted = true;
                if (esp_ble_gap_start_advertising(&advertiseParams_) != ESP_OK) {
                    abort();
                    return;
                }
                pendingAt = clock_.nowMilliseconds();
            }
        }
        if (gate.phase() == ClosedBleAdvertiseGate::Phase::Connected &&
            !source_.readOriginalTransport(link)) {
            std::uint32_t lifecycle = 0;
            if (!source_.readOriginalLifecycle(lifecycle) || lifecycle <= connectedBirth ||
                !gate.disconnected(serviceReady(),
                                   registryState() == RegistryState::StoredRegistryInstalledClosed,
                                   source_.closedChannelState(device_))) {
                abort();
                return;
            }
            advertisedBirth = lifecycle;
            startTerminal = false;
            startSucceeded = false;
            startPosted = true;
            if (esp_ble_gap_start_advertising(&advertiseParams_) != ESP_OK) {
                abort();
                return;
            }
            pendingAt = clock_.nowMilliseconds();
        }
        publish();
        const auto request = securityRequestObservation_.exchange(0, std::memory_order_acq_rel);
        if (request && securityLogs < 8U) {
            ++securityLogs;
            Serial.print("BLE security request response (1=accepted, 2=rejected, 3/4=SDK error): ");
            Serial.println(static_cast<unsigned>(request));
        }
        const auto authentication = authenticationObservation_.exchange(0,
                                                                         std::memory_order_acq_rel);
        if (authentication && authenticationLogs < 8U) {
            ++authenticationLogs;
            Serial.print("BLE authentication result (1=success, 0x100+reason=failure): ");
            Serial.println(static_cast<unsigned>(authentication));
        }
        vTaskDelay(1);
    }
}

void ProductGattsOwner::worker(void *context) noexcept {
    auto &owner = *static_cast<ProductGattsOwner *>(context);
    owner.workerReady_.store(owner.service_.begin() && owner.diagnosticService_.begin(),
                             std::memory_order_relaxed);
    owner.workerStarted_.store(true, std::memory_order_release);
    for (;;) {
        if (owner.workerReady_.load(std::memory_order_relaxed)) {
            owner.service_.service();
            owner.diagnosticService_.service(owner.service_.ready());
            (void)owner.responseWorker_.service();
        }
        vTaskDelay(1);
    }
}

void ProductGattsOwner::callback(esp_gatts_cb_event_t kind, esp_gatt_if_t interface,
                                 esp_ble_gatts_cb_param_t *parameter) noexcept {
    auto *owner = selected_;
    if (owner && parameter) {
        if (owner->diagnosticService_.owns(kind, *parameter))
            (void)owner->diagnosticService_.copyEvent(kind, interface, *parameter);
        else
            (void)owner->service_.copyEvent(fromSdkGattsService(kind, interface, *parameter));
    }
}

void ProductGattsOwner::loadRegistry(void *context) noexcept {
    auto &owner = *static_cast<ProductGattsOwner *>(context);
    const auto started = owner.clock_.nowMilliseconds();
    while (!owner.serviceReady()) {
        if (owner.service_.snapshot().phase == GattsServicePhase::Failed ||
            owner.diagnosticService_.phase() ==
                larp::diagnostic::DiagnosticGattsPhase::Failed ||
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
        owner.runDiscovery();
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
