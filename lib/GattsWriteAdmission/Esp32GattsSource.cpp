#include "Esp32GattsSource.h"
#include <cstring>
#include <new>
namespace larp {
namespace companion {
namespace {
Esp32GattsSource *selected = nullptr;
std::uint16_t application = 0;
sdk446::Callbacks originalCallbacks = {};
bool registrationAttempted = false;
bool smpRegistrationAttempted = false, smpRegistered = false;
sdk446::SmpCallback originalSmpCallback = nullptr;
std::uint8_t smpCallback(std::uint8_t event, std::uint8_t *address, sdk446::SmpData *data) {
    auto *source = selected;
    const bool captured = source && source->smpEventNow(event, address, data);
    const auto result = originalSmpCallback ? originalSmpCallback(event, address, data) : 0xff;
    if (captured)
        source->smpForwardedNow();
    return result;
}
std::uint32_t registrationCounter = 0;
std::uint32_t peerRolePostCounter = 0;
std::uint32_t proofGrantPostCounter = 0;
std::uint32_t challengeReadPostCounter = 0;
std::uint32_t peerAuthReadPostCounter = 0;
portMUX_TYPE registrationMutex = portMUX_INITIALIZER_UNLOCKED;
void connectionCallback(std::uint8_t interface, std::uint8_t *address, std::uint16_t packed,
                        bool connected, std::uint16_t reason, std::uint8_t transport) {
    if (selected && transport == 2)
        selected->connectionNow(interface, address, packed, connected);
    // Forward AFTER original lifetime invalidation; no local lock surrounds SDK.
    if (originalCallbacks.connection)
        originalCallbacks.connection(interface, address, packed, connected, reason, transport);
}
void requestCallback(std::uint16_t packed, std::uint32_t transaction, std::uint8_t type,
                     sdk446::Request *request) {
    if (selected && selected->requestNow(packed, transaction, type, request))
        return; // Selected writes never re-enter the delayed BTA/BTC write path.
    if (originalCallbacks.request)
        originalCallbacks.request(packed, transaction, type, request);
}
} // namespace
bool armGattsSourceBeforeSdkStart(std::uint16_t app, Esp32GattsSource &source) noexcept {
    if (!app || selected || btu_thread)
        return false;
    application = app;
    selected = &source;
    return true;
}
bool Esp32GattsSource::sourceTask() const noexcept {
    return sourceTask_ && sourceTask_ == xTaskGetCurrentTaskHandle() && sdkThread_ &&
           sdkThread_ == btu_thread;
}
bool Esp32GattsSource::registeredSourceNow(std::uint8_t interface) noexcept {
    if (registered_ || !interface || interface == 0xff || !btu_thread ||
        !xTaskGetCurrentTaskHandle())
        return false;
    // The wrapper invokes this synchronously from GATT_Register, whose source
    // caller is BTU. The source task/SDK instance remain immutable afterwards.
    portENTER_CRITICAL(&registrationMutex);
    const auto token = registrationCounter == UINT32_MAX ? 0 : ++registrationCounter;
    portEXIT_CRITICAL(&registrationMutex);
    if (!token)
        return false; // No registration token wrap/reuse.
    const auto task = xTaskGetCurrentTaskHandle();
    const auto sdk = btu_thread;
    portENTER_CRITICAL(&mutex_);
    sdkThread_ = sdk;
    sourceTask_ = task;
    interface_ = interface;
    registrationToken_ = token;
    lifecycle_ = 1;
    registered_ = true;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::readRegistration(GattsSourceRegistration &destination) const noexcept {
    GattsSourceRegistration value;
    portENTER_CRITICAL(&mutex_);
    value.token = registrationToken_;
    value.interface = interface_;
    const auto sdk = sdkThread_;
    portEXIT_CRITICAL(&mutex_);
    if (!value.token || !sdk || sdk != btu_thread)
        return false;
    destination = value;
    return true;
}
void Esp32GattsSource::copyIdentitySnapshot(const IdentityAccessSnapshot &caller) noexcept {
    const auto value = caller;
    portENTER_CRITICAL(&mutex_);
    identitySnapshot_ = value;
    portEXIT_CRITICAL(&mutex_);
}
bool Esp32GattsSource::closedPeerConnectionCurrent(const PeerProofBinding &caller) const noexcept {
    const auto original = caller;
    if (!original.valid())
        return false;
    GattsSourceRegistration reg;
    if (!readRegistration(reg) || reg.token != original.registration ||
        reg.interface != original.interface)
        return false;
    portENTER_CRITICAL(&mutex_);
    const auto owned = identitySnapshot_;
    const bool open = peerProofOpen_ && !registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    if (!open || !owned.proofAllowed || !(owned.connection == original.connection) ||
        !(owned.device == original.device))
        return false;
    TrustedAuthority observed;
    if (!channel_.readAuthority(observed) || !(observed == owned.authority))
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool current = peerProofOpen_ && !registryFenced_ && identitySnapshot_ == owned;
    portEXIT_CRITICAL(&mutex_);
    return current;
}
bool Esp32GattsSource::roleOriginalCurrent(const PeerRegistryActorOrigin &original) const noexcept {
    GattsSourceRegistration reg;
    if (!readRegistration(reg) || reg.token != original.registration ||
        reg.interface != original.interface)
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool current = owner_ && lifecycle_ == original.lifecycle && !registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    return current;
}
bool Esp32GattsSource::readPeerRoleOrigin(const Identity &caller,
                                          PeerRegistryActorOrigin &destination) const noexcept {
    PeerRegistryActorOrigin value;
    value.device = caller; // Freeze before any external observation.
    if (!value.device.valid() || selected != this)
        return false;
    GattsSourceRegistration reg;
    if (!readRegistration(reg))
        return false;
    value.registration = reg.token;
    value.interface = reg.interface;
    portENTER_CRITICAL(&mutex_);
    value.lifecycle = lifecycle_;
    const auto owned = identitySnapshot_;
    portEXIT_CRITICAL(&mutex_);
    if (!owned.current || !(owned.device == value.device))
        return false;
    value.authority = owned.authority; // Never adopt another publisher's current authority.
    TrustedAuthority observed;
    const bool present = channel_.readAuthority(observed);
    if ((value.authority.epoch ? !present || !(observed == value.authority) : present) ||
        !value.valid() || !roleOriginalCurrent(value))
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool unchanged =
        identitySnapshot_ == owned && lifecycle_ == value.lifecycle && !registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    if (!unchanged)
        return false;
    destination = value;
    return true;
}
PeerRegistryRolePost Esp32GattsSource::postPeerRoleJob(const PeerRegistryRoleJob &caller) noexcept {
    const auto value = caller;
    if (rolePosting_)
        return PeerRegistryRolePost::Busy;
    rolePosting_ = true;
    const auto worker = xTaskGetCurrentTaskHandle();
    if (selected != this || !worker || worker == sourceTask_ || !validPeerRegistryRoleJob(value) ||
        !roleOriginalCurrent(value.original) ||
        clock_.nowMilliseconds() - value.receivedAt >=
            PeerRegistryActorLimits::DeadlineMilliseconds) {
        rolePosting_ = false;
        return PeerRegistryRolePost::Rejected;
    }
    portENTER_CRITICAL(&mutex_);
    const bool wrongWorker = roleWorker_ && roleWorker_ != worker;
    const auto state = role_.state;
    const bool duplicate = state && role_.value == value;
    const bool stale = state && value.sequence <= role_.value.sequence;
    portEXIT_CRITICAL(&mutex_);
    if (wrongWorker || state == 5 || (stale && !duplicate)) {
        rolePosting_ = false;
        return PeerRegistryRolePost::Rejected;
    }
    if (duplicate) {
        rolePosting_ = false;
        return state == 4 ? PeerRegistryRolePost::Rejected : PeerRegistryRolePost::Duplicate;
    }
    if (state == 1 || state == 2) {
        rolePosting_ = false;
        return PeerRegistryRolePost::Busy;
    }
    portENTER_CRITICAL(&registrationMutex);
    const auto token = peerRolePostCounter == UINT32_MAX ? 0 : ++peerRolePostCounter;
    portEXIT_CRITICAL(&registrationMutex);
    if (!token) {
        rolePosting_ = false;
        return PeerRegistryRolePost::Rejected;
    }
    portENTER_CRITICAL(&mutex_);
    roleWorker_ = worker;
    role_.value = value;
    role_.ack = {};
    role_.postToken = token;
    role_.state = 1;
    portEXIT_CRITICAL(&mutex_);
    // Opaque integer token never points into a reusable mutable slot. An old
    // callback cannot consume a new request after the single slot is reused.
    const bool posted =
        roleOriginalCurrent(value.original) && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &applyPeerRoles,
                        reinterpret_cast<void *>(static_cast<std::uintptr_t>(token)), 0, 0);
    if (!posted) {
        portENTER_CRITICAL(&mutex_);
        role_.state = 4;
        portEXIT_CRITICAL(&mutex_);
    }
    rolePosting_ = false;
    return posted ? PeerRegistryRolePost::Posted : PeerRegistryRolePost::Rejected;
}
PeerRegistryRoleState
Esp32GattsSource::peerRoleState(const PeerRegistryRoleJob &caller,
                                TrustedAuthority &destination) const noexcept {
    const auto value = caller;
    if (!roleOriginalCurrent(value.original))
        return PeerRegistryRoleState::Unavailable;
    portENTER_CRITICAL(&mutex_);
    const auto state = role_.state;
    const bool match = state && role_.value == value;
    const auto ack = role_.ack;
    const bool worker = roleWorker_ == xTaskGetCurrentTaskHandle();
    portEXIT_CRITICAL(&mutex_);
    if (!match || !worker)
        return PeerRegistryRoleState::Unavailable;
    if (clock_.nowMilliseconds() - value.receivedAt >=
        PeerRegistryActorLimits::DeadlineMilliseconds)
        return PeerRegistryRoleState::Rejected;
    if (state == 4 || state == 5)
        return PeerRegistryRoleState::Rejected;
    if (state != 3)
        return PeerRegistryRoleState::Pending;
    TrustedAuthority observed;
    const bool present = channel_.readAuthority(observed);
    if ((ack.epoch ? !present || !(observed == ack) : present) ||
        !roleOriginalCurrent(value.original))
        return PeerRegistryRoleState::Unavailable;
    destination = ack;
    return PeerRegistryRoleState::Confirmed;
}
bool Esp32GattsSource::installedPeerRoles(const PeerRegistryRoleJob &caller) const noexcept {
    const auto value = caller;
    GattsSourceRegistration reg;
    if (!readRegistration(reg) || reg.token != value.original.registration ||
        reg.interface != value.original.interface)
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool installed = role_.state == 3 && !registryFenced_ && role_.value == value &&
                           value.kind == PeerRegistryRoleKind::Install;
    portEXIT_CRITICAL(&mutex_);
    return installed;
}
void Esp32GattsSource::fencePeerRoles(const PeerRegistryRoleJob &caller) noexcept {
    const auto value = caller;
    const auto worker = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&mutex_);
    if (roleWorker_ == worker && role_.value == value) {
        registryFenced_ = true;
        peerProofOpen_ = false;
        if (role_.state == 1 || role_.state == 2)
            role_.state = 5;
    }
    portEXIT_CRITICAL(&mutex_);
}
void Esp32GattsSource::applyPeerRoles(void *context) noexcept {
    auto *source = selected;
    if (!source)
        return;
    const auto token = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
    portENTER_CRITICAL(&source->mutex_);
    const bool pending = token && token == source->role_.postToken && source->role_.state == 1 &&
                         !source->registryFenced_;
    if (pending)
        source->role_.state = 2;
    const auto value = source->role_.value;
    portEXIT_CRITICAL(&source->mutex_);
    if (!pending)
        return;
    source->reconcileProofGrantCloseNow();
    // A crypto-worker cancellation may have closed this same original while
    // its role-close job was queued. Accept only that stored exact own closure
    // ACK after the sole Identity actor acknowledged it; never adopt a publisher.
    TrustedAuthority expected = value.original.authority;
    portENTER_CRITICAL(&source->mutex_);
    const auto &grant = source->grant_;
    if (value.kind == PeerRegistryRoleKind::Close && grant.state == 5 && grant.closeAck.epoch &&
        grant.closeExpected == expected && grant.value.original.device == value.original.device &&
        grant.value.original.registration == value.original.registration &&
        grant.value.original.lifecycle == value.original.lifecycle &&
        grant.value.original.interface == value.original.interface &&
        source->identitySnapshot_.current && source->identitySnapshot_.connected &&
        source->identitySnapshot_.connection == grant.value.proof.original.connection &&
        source->identitySnapshot_.authority == grant.closeAck)
        expected = grant.closeAck;
    portEXIT_CRITICAL(&source->mutex_);
    TrustedAuthority observed, ack;
    Identity device;
    const bool present = source->channel_.readAuthority(observed);
    bool valid = source->sourceTask() && source->roleOriginalCurrent(value.original) &&
                 (expected.epoch ? present && observed == expected : !present) &&
                 source->access_.deviceIdentity(device) && device == value.original.device &&
                 source->clock_.nowMilliseconds() - value.receivedAt <
                     PeerRegistryActorLimits::DeadlineMilliseconds &&
                 source->roleOriginalCurrent(value.original);
    if (valid) {
        portENTER_CRITICAL(&source->mutex_);
        source->peerProofOpen_ = false;
        portEXIT_CRITICAL(&source->mutex_);
        valid = source->access_.replaceGameMastersClosed(value.roles, &ack) ==
                IdentityAccessOutcome::AssignmentsReplaced;
    }
    const bool ackPresent = source->channel_.readAuthority(observed);
    valid = valid && (ack.epoch ? ackPresent && observed == ack : !ackPresent) &&
            source->roleOriginalCurrent(value.original) &&
            source->clock_.nowMilliseconds() - value.receivedAt <
                PeerRegistryActorLimits::DeadlineMilliseconds;
    portENTER_CRITICAL(&source->mutex_);
    if (source->role_.state == 2 && source->role_.postToken == token && !source->registryFenced_) {
        source->role_.ack = ack;
        source->role_.state = valid ? 3 : 4;
    }
    portEXIT_CRITICAL(&source->mutex_);
}
GattsEndpointSetupPost
Esp32GattsSource::postEndpointSetup(const GattsEndpointSetup &caller,
                                    IWriteResponseBearer &responses,
                                    const PeerChallengeReadSetup &callerChallenge) noexcept {
    const auto value = caller;
    const auto challenge = callerChallenge;
    if (setupPosting_)
        return GattsEndpointSetupPost::Busy;
    setupPosting_ = true;
    GattsSourceRegistration current;
    const auto end = static_cast<std::uint32_t>(value.service) +
                     (value.proof       ? GattsServiceDefinition::AuthHandles
                      : value.issue     ? GattsServiceDefinition::IssueHandles
                      : value.challenge ? GattsServiceDefinition::ChallengeHandles
                                        : GattsServiceDefinition::Handles) -
                     1;
    if (!readRegistration(current) || !(current == value.original) || !value.service ||
        value.command <= value.service || value.profile <= value.command || value.profile > end ||
        (value.challenge &&
         (value.challenge <= value.profile || value.challenge > end || !challenge.boot.valid() ||
          !challenge.context || !challenge.ingress)) ||
        (!value.challenge &&
         (challenge.boot.valid() || challenge.context || challenge.ingress || challenge.issue)) ||
        (value.issue && (!value.challenge || value.issue <= value.challenge || value.issue > end ||
                         !challenge.issue)) ||
        (!value.issue && challenge.issue) ||
        ((value.proof || value.result || challenge.auth) &&
         (!value.issue || !value.proof || !value.result || !challenge.auth ||
          value.proof <= value.issue || value.result <= value.proof || value.result > end)) ||
        end > UINT16_MAX ||
        clock_.nowMilliseconds() - value.receivedAt >=
            GattsServiceDefinition::EndpointDeadlineMilliseconds) {
        setupPosting_ = false;
        return GattsEndpointSetupPost::Rejected;
    }
    portENTER_CRITICAL(&mutex_);
    const auto state = setup_.state;
    const bool duplicate = state && setup_.value == value && setup_.challenge == challenge;
    if (!state) {
        setup_.value = value;
        setup_.responses = &responses;
        setup_.challenge = challenge;
        setup_.state = 1;
    }
    portEXIT_CRITICAL(&mutex_);
    if (state) {
        setupPosting_ = false;
        return duplicate && state != 4 ? GattsEndpointSetupPost::Duplicate
                                       : GattsEndpointSetupPost::Rejected;
    }
    const bool posted =
        sdkThread_ && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &installEndpoints, this, 0, 0);
    if (!posted) {
        portENTER_CRITICAL(&mutex_);
        setup_.state = 4;
        portEXIT_CRITICAL(&mutex_);
    }
    setupPosting_ = false;
    return posted ? GattsEndpointSetupPost::Posted : GattsEndpointSetupPost::Rejected;
}
GattsEndpointSetupState
Esp32GattsSource::endpointSetupState(const GattsEndpointSetup &caller) const noexcept {
    const auto value = caller;
    GattsSourceRegistration current;
    if (!readRegistration(current) || !(current == value.original))
        return GattsEndpointSetupState::Unavailable;
    portENTER_CRITICAL(&mutex_);
    const auto state = setup_.state;
    const bool matches = state && setup_.value == value;
    portEXIT_CRITICAL(&mutex_);
    if (!matches)
        return GattsEndpointSetupState::Unavailable;
    if ((state == 1 || state == 2) && clock_.nowMilliseconds() - value.receivedAt >=
                                          GattsServiceDefinition::EndpointDeadlineMilliseconds)
        return GattsEndpointSetupState::Rejected;
    return state == 3   ? GattsEndpointSetupState::Installed
           : state == 4 ? GattsEndpointSetupState::Rejected
                        : GattsEndpointSetupState::Pending;
}
void Esp32GattsSource::installEndpoints(void *context) noexcept {
    auto &source = *static_cast<Esp32GattsSource *>(context);
    portENTER_CRITICAL(&source.mutex_);
    const bool pending = source.setup_.state == 1;
    if (pending)
        source.setup_.state = 2;
    const auto value = source.setup_.value;
    auto *responses = source.setup_.responses;
    const auto challenge = source.setup_.challenge;
    portEXIT_CRITICAL(&source.mutex_);
    if (!pending)
        return;
    GattsSourceRegistration current;
    bool installed = source.sourceTask() && source.readRegistration(current) &&
                     current == value.original && responses &&
                     source.clock_.nowMilliseconds() - value.receivedAt <
                         GattsServiceDefinition::EndpointDeadlineMilliseconds &&
                     source.configureEndpointsNow(value.command, value.profile, *responses);
    if (installed && value.challenge)
        installed = challenge.context && challenge.ingress &&
                    source.configurePeerProofGrantNow(challenge.boot, *challenge.context) &&
                    source.configureChallengeReadNow(value.challenge, *challenge.ingress);
    if (installed && value.issue)
        installed =
            challenge.issue && source.configureChallengeIssueNow(value.issue, *challenge.issue);
    if (installed && value.proof)
        installed = challenge.auth &&
                    source.configurePeerAuthNow(value.proof, value.result, *challenge.auth);
    portENTER_CRITICAL(&source.mutex_);
    source.setup_.state = installed ? 3 : 4;
    portEXIT_CRITICAL(&source.mutex_);
}
bool Esp32GattsSource::configurePeerProofGrantNow(const BootIdentity &caller,
                                                  const IPeerProofContext &context) noexcept {
    const auto boot = caller;
    if (!sourceTask() || !owner_ || sawConnection_ || grantConfigured_ || !boot.valid())
        return false;
    portENTER_CRITICAL(&mutex_);
    proofBoot_ = boot;
    proofContext_ = &context;
    grantConfigured_ = true;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::readPeerProofGrantOrigin(const PeerProofBinding &caller,
                                                PeerRegistryActorOrigin &out) const noexcept {
    const auto original = caller;
    portENTER_CRITICAL(&mutex_);
    const auto boot = proofBoot_;
    const bool configured = grantConfigured_;
    const auto *context = proofContext_;
    portEXIT_CRITICAL(&mutex_);
    PeerRegistryActorOrigin value;
    if (!configured || !context || !(original.boot == boot) || !original.valid() ||
        !closedPeerConnectionCurrent(original) || !context->originalLive(original) ||
        !readPeerRoleOrigin(original.device, value) || value.authority.gameMaster ||
        value.authority.principal || !closedPeerConnectionCurrent(original))
        return false;
    out = value;
    return true;
}
bool Esp32GattsSource::proofGrantScopeCurrent(const PeerProofGrantJob &caller,
                                              bool beforeGrant) const noexcept {
    // Private helper: every caller already holds its own frozen entry copy.
    const auto &value = caller;
    if (!grantConfigured_ || !proofContext_ || !(value.proof.original.boot == proofBoot_) ||
        !roleOriginalCurrent(value.original) ||
        !closedPeerConnectionCurrent(value.proof.original) ||
        !proofContext_->originalLive(value.proof.original))
        return false;
    portENTER_CRITICAL(&mutex_);
    const auto owned = identitySnapshot_;
    portEXIT_CRITICAL(&mutex_);
    return owned.device == value.original.device &&
           owned.connection == value.proof.original.connection &&
           (!beforeGrant || owned.authority == value.original.authority);
}
PeerProofGrantPost Esp32GattsSource::postPeerProofGrant(const PeerProofGrantJob &caller) noexcept {
    const auto value = caller;
    const auto worker = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&mutex_);
    const bool unavailable = grantPosting_ || !worker || worker == sourceTask_ ||
                             (proofWorker_ && proofWorker_ != worker);
    if (!unavailable)
        grantPosting_ = true;
    const auto state = grant_.state;
    const bool same = state && grant_.value == value;
    const bool stale = state && value.sequence <= grant_.value.sequence;
    portEXIT_CRITICAL(&mutex_);
    if (unavailable)
        return PeerProofGrantPost::Rejected;
    PeerProofGrantPost result = PeerProofGrantPost::Rejected;
    if (same)
        result = (state == 1 || state == 2 || state == 3 || state == 6)
                     ? PeerProofGrantPost::Duplicate
                     : PeerProofGrantPost::Rejected;
    else if (state == 1 || state == 2)
        result = PeerProofGrantPost::Busy;
    else if (!stale && selected == this && validPeerProofGrantJob(value) &&
             peerProofGrantFresh(value, clock_.nowMilliseconds()) &&
             proofGrantScopeCurrent(value, true) &&
             proofContext_->credentialCurrent(value.proof.credential) &&
             proofGrantScopeCurrent(value, true)) {
        portENTER_CRITICAL(&registrationMutex);
        const auto token = proofGrantPostCounter == UINT32_MAX ? 0 : ++proofGrantPostCounter;
        portEXIT_CRITICAL(&registrationMutex);
        if (token) {
            portENTER_CRITICAL(&mutex_);
            proofWorker_ = worker;
            grant_.value = value;
            grant_.ack = {};
            grant_.closeExpected = {};
            grant_.closeAck = {};
            grant_.postToken = token;
            grant_.state = 1;
            portEXIT_CRITICAL(&mutex_);
            const bool posted =
                peerProofGrantFresh(value, clock_.nowMilliseconds()) &&
                proofGrantScopeCurrent(value, true) && sdkThread_ == btu_thread &&
                osi_thread_post(static_cast<osi_thread *>(sdkThread_), &applyPeerProofGrant,
                                reinterpret_cast<void *>(static_cast<std::uintptr_t>(token)), 0, 0);
            if (!posted) {
                portENTER_CRITICAL(&mutex_);
                grant_.state = 4;
                portEXIT_CRITICAL(&mutex_);
            }
            result = posted ? PeerProofGrantPost::Posted : PeerProofGrantPost::Rejected;
        }
    }
    portENTER_CRITICAL(&mutex_);
    grantPosting_ = false;
    portEXIT_CRITICAL(&mutex_);
    return result;
}
PeerProofGrantState Esp32GattsSource::peerProofGrantState(const PeerProofGrantJob &caller,
                                                          TrustedAuthority &out) noexcept {
    const auto value = caller;
    portENTER_CRITICAL(&mutex_);
    const bool match = grant_.value == value && proofWorker_ == xTaskGetCurrentTaskHandle();
    const auto state = grant_.state;
    const auto ack = grant_.ack;
    portEXIT_CRITICAL(&mutex_);
    if (!match || !state)
        return PeerProofGrantState::Unavailable;
    if (state == 4 || state == 5)
        return PeerProofGrantState::Rejected;
    if (state == 2) {
        // IdentityAccess briefly clears its published snapshot before CAS. A
        // worker poll must not cancel this in-flight original actor transaction.
        // Pending never confirms rights; final ACK still uses all policy checks.
        const auto consumingCurrent = [&]() {
            portENTER_CRITICAL(&mutex_);
            const bool current = grantConfigured_ && value.proof.original.boot == proofBoot_ &&
                                 (grant_.state == 2 || grant_.state == 3) &&
                                 grant_.value == value && peerProofOpen_ && !registryFenced_ &&
                                 lifecycle_ == value.original.lifecycle &&
                                 registrationToken_ == value.original.registration &&
                                 interface_ == value.original.interface &&
                                 identitySnapshot_.current && identitySnapshot_.connected &&
                                 identitySnapshot_.device == value.proof.original.device &&
                                 identitySnapshot_.connection == value.proof.original.connection;
            portEXIT_CRITICAL(&mutex_);
            return current;
        };
        return peerProofGrantFresh(value, clock_.nowMilliseconds()) &&
                       roleOriginalCurrent(value.original) && consumingCurrent() &&
                       proofContext_->credentialCurrent(value.proof.credential) &&
                       peerProofGrantFresh(value, clock_.nowMilliseconds()) && consumingCurrent()
                   ? PeerProofGrantState::Pending
                   : PeerProofGrantState::Rejected;
    }
    if ((state != 6 && !peerProofGrantFresh(value, clock_.nowMilliseconds())) ||
        !proofGrantScopeCurrent(value, state != 3 && state != 6) ||
        !proofContext_->credentialCurrent(value.proof.credential) ||
        !proofGrantScopeCurrent(value, state != 3 && state != 6))
        return PeerProofGrantState::Rejected;
    if (state != 3 && state != 6)
        return PeerProofGrantState::Pending;
    TrustedAuthority observed;
    if (!channel_.readAuthority(observed) || !(observed == ack))
        return PeerProofGrantState::Unavailable;
    if (state != 6 && !peerProofGrantFresh(value, clock_.nowMilliseconds()))
        return PeerProofGrantState::Rejected;
    portENTER_CRITICAL(&mutex_);
    const bool unchanged = (grant_.state == 3 || grant_.state == 6) && grant_.value == value &&
                           identitySnapshot_.authority == ack && peerProofOpen_ &&
                           !registryFenced_ && lifecycle_ == value.original.lifecycle &&
                           identitySnapshot_.connection == value.proof.original.connection;
    if (unchanged)
        grant_.state = 6; // Observed ACK; deadline cannot revoke an established session.
    portEXIT_CRITICAL(&mutex_);
    if (!unchanged)
        return PeerProofGrantState::Unavailable;
    out = ack;
    return PeerProofGrantState::Confirmed;
}
void Esp32GattsSource::cancelPeerProofGrant(const PeerProofGrantJob &caller) noexcept {
    const auto value = caller;
    TrustedAuthority expected;
    portENTER_CRITICAL(&mutex_);
    const bool match =
        proofWorker_ == xTaskGetCurrentTaskHandle() && grant_.value == value && grant_.state;
    if (match) {
        const bool applied = grant_.state == 3 || grant_.state == 6;
        grant_.state = 5;
        // Only the applied grant's exact ACK may retire authority. A pre-grant
        // cancel masks this proof link without disturbing a queued role closure.
        if (identitySnapshot_.device == value.proof.original.device &&
            identitySnapshot_.connection == value.proof.original.connection &&
            lifecycle_ == value.original.lifecycle &&
            registrationToken_ == value.original.registration) {
            if (applied && identitySnapshot_.authority == grant_.ack)
                expected = grant_.ack;
            peerProofOpen_ = false;
        }
    }
    portEXIT_CRITICAL(&mutex_);
    if (!match || !expected.epoch)
        return;
    TrustedAuthority closed;
    if (access_.closeCopiedAuthority(expected, closed) != AuthorityUpdate::Published)
        return;
    portENTER_CRITICAL(&mutex_);
    if (grant_.value == value && grant_.state == 5) {
        grant_.closeExpected = expected;
        grant_.closeAck = closed;
    }
    portEXIT_CRITICAL(&mutex_);
}
void Esp32GattsSource::reconcileProofGrantCloseNow() noexcept {
    if (!sourceTask())
        return;
    portENTER_CRITICAL(&mutex_);
    const auto original = grant_.value.proof.original.connection;
    const auto expected = grant_.closeExpected;
    const auto closed = grant_.closeAck;
    // Keep the immutable closure evidence until this slot is replaced. A role
    // closure queued for the same original may need this exact transition.
    portEXIT_CRITICAL(&mutex_);
    if (closed.epoch)
        (void)access_.acknowledgeCopiedClose(original, expected, closed);
}
void Esp32GattsSource::applyPeerProofGrant(void *context) noexcept {
    auto *source = selected;
    if (!source)
        return;
    const auto token = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
    portENTER_CRITICAL(&source->mutex_);
    const bool pending = token && source->grantConfigured_ && token == source->grant_.postToken &&
                         source->grant_.state == 1;
    if (pending)
        source->grant_.state = 2;
    const auto value = source->grant_.value;
    portEXIT_CRITICAL(&source->mutex_);
    source->reconcileProofGrantCloseNow();
    if (!pending)
        return;
    OriginalPeerSecurity security;
    VerifiedPeerEvidence evidence;
    bool accepted = source->sourceTask() &&
                    peerProofGrantFresh(value, source->clock_.nowMilliseconds()) &&
                    source->proofGrantScopeCurrent(value, true) &&
                    source->proofContext_->credentialCurrent(value.proof.credential) &&
                    source->readOriginalPeerSecurityNow(value.proof.original, security) &&
                    source->proofContext_->credentialCurrent(value.proof.credential) &&
                    peerProofGrantFresh(value, source->clock_.nowMilliseconds()) &&
                    source->proofGrantScopeCurrent(value, true) &&
                    bindPeerProofEvidence(value.proof, security, evidence);
    portENTER_CRITICAL(&source->mutex_);
    accepted = accepted && source->grant_.state == 2 && source->grant_.value == value;
    portEXIT_CRITICAL(&source->mutex_);
    bool attempted = false;
    if (accepted) {
        attempted = true;
        const auto outcome =
            source->access_.authenticate(value.proof.original.connection, evidence);
        accepted = outcome == IdentityAccessOutcome::Verified ||
                   outcome == IdentityAccessOutcome::Duplicate;
    }
    accepted = accepted && source->proofGrantScopeCurrent(value, false) &&
               source->proofContext_->credentialCurrent(value.proof.credential) &&
               peerProofGrantFresh(value, source->clock_.nowMilliseconds()) &&
               source->proofGrantScopeCurrent(value, false);
    portENTER_CRITICAL(&source->mutex_);
    const bool same = source->grant_.value == value && source->grant_.state == 2;
    accepted = accepted && same && source->peerProofOpen_ && !source->registryFenced_ &&
               source->lifecycle_ == value.original.lifecycle &&
               source->identitySnapshot_.connection == value.proof.original.connection;

    if (same) {
        source->grant_.ack = accepted ? source->identitySnapshot_.authority : TrustedAuthority{};
        source->grant_.state = accepted ? 3 : 4;
    }
    portEXIT_CRITICAL(&source->mutex_);
    if (!accepted && attempted && source->sourceTask()) {
        source->reconcileProofGrantCloseNow();
        // Preserve a newer connection/publisher, even if an external port replaced it.
        BoundConnection original;
        if (source->originalConnectionNow(original) && original == value.proof.original.connection)
            (void)source->access_.authenticate(original, {});
    }
}
bool Esp32GattsSource::configureEndpointsNow(std::uint16_t command, std::uint16_t profile,
                                             IWriteResponseBearer &responses) noexcept {
    if (!sourceTask() || !registered_ || owner_ || sawConnection_ || !command || !profile ||
        command == profile)
        return false;
    Identity device;
    if (!access_.deviceIdentity(device) || !access_.attachObserver(*this))
        return false;
    GattsEndpoints endpoints;
    endpoints.interface = interface_;
    endpoints.command = command;
    endpoints.profile = profile;
    auto *owner = new (ownerStorage_) FreeRtosGattsConnection(access_, clock_, endpoints);
    command_ = command;
    profile_ = profile;
    responses_ = &responses;
    portENTER_CRITICAL(&mutex_);
    owner_ = owner; // Publish once; object/endpoints never replaced or destroyed.
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::postDiagnosticClosedSetup(
    const diagnostic::DiagnosticClosedHandles &caller) noexcept {
    const auto value = caller;
    const std::uint16_t attributes[] = {
        value.service, value.controlWrite, value.controlReply, value.controlCccd,
        value.stepWrite, value.stepReply, value.stepCccd};
    for (const auto attribute : attributes)
        if (attribute == command_ || attribute == profile_)
            return false;
    GattsSourceRegistration original;
    if (!value.complete() || !readRegistration(original) || !(original == value.original) ||
        !owner_ || sawConnection_ || !xTaskGetCurrentTaskHandle() || sourceTask())
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool unused = diagnostic_.state == 0;
    if (unused) {
        diagnostic_.value = value;
        diagnostic_.state = 1;
    }
    portEXIT_CRITICAL(&mutex_);
    if (!unused)
        return false;
    if (!osi_thread_post(static_cast<osi_thread *>(sdkThread_), &installDiagnosticClosed,
                         this, 0, 0)) {
        portENTER_CRITICAL(&mutex_);
        diagnostic_.state = 4;
        portEXIT_CRITICAL(&mutex_);
        return false;
    }
    return true;
}
void Esp32GattsSource::installDiagnosticClosed(void *context) noexcept {
    auto &source = *static_cast<Esp32GattsSource *>(context);
    diagnostic::DiagnosticClosedHandles value;
    portENTER_CRITICAL(&source.mutex_);
    const bool pending = source.diagnostic_.state == 1;
    if (pending) {
        value = source.diagnostic_.value;
        source.diagnostic_.state = 2;
    }
    portEXIT_CRITICAL(&source.mutex_);
    if (!pending)
        return;
    GattsSourceRegistration current;
    const std::uint16_t attributes[] = {
        value.service, value.controlWrite, value.controlReply, value.controlCccd,
        value.stepWrite, value.stepReply, value.stepCccd};
    bool separate = true;
    for (const auto attribute : attributes)
        separate = separate && attribute != source.command_ && attribute != source.profile_;
    const bool installed = source.sourceTask() && source.readRegistration(current) &&
                           current == value.original && value.complete() && source.owner_ &&
                           !source.sawConnection_ && separate;
    portENTER_CRITICAL(&source.mutex_);
    source.diagnostic_.state = installed ? 3 : 4;
    portEXIT_CRITICAL(&source.mutex_);
}
bool Esp32GattsSource::diagnosticClosedSetupInstalled(
    const diagnostic::DiagnosticClosedHandles &caller) const noexcept {
    const auto value = caller;
    GattsSourceRegistration current;
    if (!value.complete() || !readRegistration(current) || !(current == value.original))
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool installed = diagnostic_.state == 3 &&
                           diagnostic_.value.original == value.original &&
                           diagnostic_.value.service == value.service &&
                           diagnostic_.value.controlWrite == value.controlWrite &&
                           diagnostic_.value.controlReply == value.controlReply &&
                           diagnostic_.value.controlCccd == value.controlCccd &&
                           diagnostic_.value.stepWrite == value.stepWrite &&
                           diagnostic_.value.stepReply == value.stepReply &&
                           diagnostic_.value.stepCccd == value.stepCccd;
    portEXIT_CRITICAL(&mutex_);
    return installed;
}
GattsLifecycleOutcome Esp32GattsSource::connectionNow(std::uint8_t interface,
                                                      const std::uint8_t *address,
                                                      std::uint16_t packed,
                                                      bool connected) noexcept {
    GattsConnectionEvent event;
    event.interface = interface;
    event.connection = packed; // Full private packed conn_id, not BTC's upper byte.
    if (address)
        std::memcpy(event.peerAddress.data(), address, event.peerAddress.size());
    if (!sourceTask() || interface != interface_ || (packed & 0xff) != interface)
        return GattsLifecycleOutcome::Invalid;
    reconcileProofGrantCloseNow();
    portENTER_CRITICAL(&mutex_);
    peerProofOpen_ = false;
    publishedTransport_ = {};
    const bool lifetimeAvailable = lifecycle_ != UINT32_MAX;
    if (lifetimeAvailable)
        ++lifecycle_;
    else
        registryFenced_ = true;
    portEXIT_CRITICAL(&mutex_);
    linkSecurity_.close();
    if (!lifetimeAvailable) {
        const auto original = connection_;
        const bool wasActive = active_;
        active_ = false;
        connection_ = {};
        live_ = {};
        if (wasActive)
            (void)access_.disconnect(original); // Exact owned authority; preserve other producers.
        return GattsLifecycleOutcome::Unavailable;
    }
    sawConnection_ = true;
    if (!owner_)
        return GattsLifecycleOutcome::Unavailable;
    if (connected) {
        BoundConnection opened;
        const auto outcome = owner_->connectNow(event, opened);
        if (outcome == GattsLifecycleOutcome::Opened) {
            live_ = event;
            connection_ = opened;
            active_ = true;
            portENTER_CRITICAL(&mutex_);
            peerProofOpen_ =
                identitySnapshot_.proofAllowed && identitySnapshot_.connection == opened;
            portEXIT_CRITICAL(&mutex_);
            PeerTransportBinding current;
            if (transportNow(current)) {
                portENTER_CRITICAL(&mutex_);
                publishedTransport_ = current;
                portEXIT_CRITICAL(&mutex_);
            }
        } else if (outcome == GattsLifecycleOutcome::Conflict) {
            active_ = false;
            connection_ = {};
            live_ = {};
        }
        return outcome;
    }
    const auto outcome = owner_->disconnectNow(event);
    if (outcome == GattsLifecycleOutcome::Closed || outcome == GattsLifecycleOutcome::Unavailable) {
        active_ = false;
        connection_ = {};
        live_ = {};
    }
    return outcome;
}
bool Esp32GattsSource::originalConnectionNow(BoundConnection &destination) const noexcept {
    if (!sourceTask() || !active_)
        return false;
    destination = connection_;
    return true;
}
bool Esp32GattsSource::readOriginalTransport(PeerTransportBinding &destination) const noexcept {
    PeerTransportBinding value;
    IdentityAccessSnapshot owned;
    bool fenced;
    portENTER_CRITICAL(&mutex_);
    value = publishedTransport_;
    owned = identitySnapshot_;
    fenced = registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    GattsSourceRegistration registration;
    if (fenced || !value.valid() || !owned.current || !owned.connected ||
        !(owned.device == value.device) || !(owned.connection == value.connection) ||
        !readRegistration(registration) || registration.token != value.registration ||
        registration.interface != value.interface)
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool stillCurrent = !registryFenced_ && publishedTransport_ == value &&
                              identitySnapshot_ == owned;
    portEXIT_CRITICAL(&mutex_);
    if (!stillCurrent)
        return false;
    destination = value;
    return true;
}
bool Esp32GattsSource::readOriginalLifecycle(std::uint32_t &destination) const noexcept {
    portENTER_CRITICAL(&mutex_);
    const auto value = lifecycle_;
    const bool current = registered_ && !registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    if (!current)
        return false;
    destination = value;
    return true;
}
bool Esp32GattsSource::closedChannelState(const Identity &device) const noexcept {
    IdentityAccessSnapshot owned;
    bool fenced;
    portENTER_CRITICAL(&mutex_);
    owned = identitySnapshot_;
    fenced = registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    if (fenced || !owned.current || !(owned.device == device) ||
        owned.authority.principal || owned.authority.gameMaster)
        return false;
    if (owned.connected) {
        PeerTransportBinding birth;
        if (!readOriginalTransport(birth) ||
            !(owned.connection == birth.connection) ||
            owned.authority.epoch != birth.connection.token)
            return false;
    }
    TrustedAuthority observed;
    const bool present = channel_.readAuthority(observed);
    if (owned.authority.epoch ? !present || !(observed == owned.authority) : present)
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool current = !registryFenced_ && identitySnapshot_ == owned;
    portEXIT_CRITICAL(&mutex_);
    return current;
}
bool Esp32GattsSource::transportNow(PeerTransportBinding &destination) const noexcept {
    if (!sourceTask() || !active_ || !owner_)
        return false;
    PeerTransportBinding value;
    value.connection = connection_;
    value.address = live_.peerAddress;
    value.registration = registrationToken_;
    value.interface = interface_;
    portENTER_CRITICAL(&mutex_);
    value.device = identitySnapshot_.device;
    value.lifecycle = lifecycle_;
    const bool available = identitySnapshot_.current && identitySnapshot_.connected &&
                           identitySnapshot_.connection == value.connection && !registryFenced_;
    portEXIT_CRITICAL(&mutex_);
    if (!available || !value.valid())
        return false;
    destination = value;
    return true;
}
bool Esp32GattsSource::smpEventNow(std::uint8_t event, const std::uint8_t *borrowedAddress,
                                   const sdk446::SmpData *borrowed) noexcept {
    if (!sourceTask() || smpForwarding_ || !smpRegistered)
        return false;
    smpForwarding_ = true; // No security query during the original callback's forwarding.
    std::array<std::uint8_t, 6> address = {};
    if (borrowedAddress)
        std::memcpy(address.data(), borrowedAddress, address.size());
    PeerPairingResult result;
    if (event == 7 && borrowed) {
        // Pinned tSMP_CMPL is five one-byte fields at union offset0. Byte copies
        // also avoid interpreting an invalid bool representation in negative models.
        std::uint8_t bytes[5];
        std::memcpy(bytes, borrowed, sizeof(bytes));
        result.reason = bytes[0];
        result.securityLevel = bytes[1];
        result.cancelled = bytes[2];
        result.overBr = bytes[3];
        result.mode = bytes[4];
    }
    if (event == 7 && linkSecurity_.discardAbandonedCompletion(address))
        return true;
    PeerTransportBinding original, after;
    if (!transportNow(original) || original.address != address)
        return true;
    const auto now = clock_.nowMilliseconds();
    if (!transportNow(after) || !(after == original))
        return true;
    PeerPairingOutcome outcome = PeerPairingOutcome::Mismatch;
    if (event == 1 || event == 2)
        outcome = linkSecurity_.start(original, now);
    else if (event == 7)
        outcome = linkSecurity_.complete(original, result, now);
    if (outcome == PeerPairingOutcome::Failed || outcome == PeerPairingOutcome::Expired ||
        outcome == PeerPairingOutcome::Consumed) {
        // Exact owned link only. Reject before forwarding so its queued GM work
        // cannot remain authorized. IdentityAccess preserves another publisher.
        (void)access_.authenticate(original.connection, {});
    }
    return true;
}
void Esp32GattsSource::smpForwardedNow() noexcept {
    if (sourceTask())
        smpForwarding_ = false;
}
bool Esp32GattsSource::readOriginalPeerSecurityNow(
    const PeerProofBinding &caller, OriginalPeerSecurity &destination) const noexcept {
    const auto proof = caller;
    PeerTransportBinding original, after;
    if (!sourceTask() || smpForwarding_ || !smpRegistered || !proof.valid() ||
        !closedPeerConnectionCurrent(proof) || !transportNow(original))
        return false;
    auto address = original.address;
    std::array<std::uint8_t, 6> observedAddress = {};
    std::uint8_t interface = 0, transport = 0, state = 0;
    PeerLinkState link;
    // These are immediate same-actor reads, never association of a queued GAP event.
    // SDK signatures use writable address pointers: recheck the frozen input and
    // original lifetime after each port, before querying any subsequent metadata.
    const auto current = [&]() {
        return address == original.address && transportNow(after) && after == original &&
               closedPeerConnectionCurrent(proof);
    };
    if (!GATT_GetConnectionInfor(original.connection.handle, &interface, observedAddress.data(),
                                 &transport) ||
        interface != original.interface || transport != 2 || observedAddress != address ||
        !current() || BTM_BleSecurityProcedureIsRunning(address.data()) || !current() ||
        !BTM_GetSecurityFlagsByTransport(address.data(), &link.flags, 2) || !current() ||
        !BTM_GetLeSecurityState(address.data(), &state, &link.keyBytes) || (state & 5) != 5 ||
        !current() || BTM_BleSecurityProcedureIsRunning(address.data()) || !current())
        return false;
    OriginalPeerSecurity value;
    if (!linkSecurity_.select(original, proof, link, value))
        return false;
    destination = value;
    return true;
}
bool Esp32GattsSource::requestNow(std::uint16_t packed, std::uint32_t transaction,
                                  std::uint8_t type, const sdk446::Request *borrowed) noexcept {
    if (type == 1) {
        // Pinned tGATTS_REQ_TYPE_READ=1. Freeze the union before any port.
        if (!borrowed)
            return challengeAttribute_ != 0;
        const auto read = borrowed->read;
        const bool resultRead = resultAttribute_ && read.handle == resultAttribute_;
        if ((!challengeAttribute_ || read.handle != challengeAttribute_) && !resultRead)
            return false;
        PeerChallengeReadRequest value;
        value.attribute = read.handle;
        value.offset = read.offset;
        value.isLong = read.isLong;
        value.transaction = transaction;
        if (!sourceTask() || !read.needResponse || !challengeIngress_ || !active_ ||
            packed != connection_.handle)
            return true;
        PeerTransportBinding transport;
        if (!transportNow(transport))
            return true;
        value.original.device = transport.device;
        value.original.boot = proofBoot_;
        value.original.connection = transport.connection;
        value.original.registration = transport.registration;
        value.original.interface = transport.interface;
        value.lifecycle = transport.lifecycle;
        value.address = transport.address;
        value.receivedAt = clock_.nowMilliseconds();
        if (resultRead) {
            if (authIngress_ && peerAuthReadProducerNow(value))
                (void)authIngress_->copyResultRead(value);
        } else if (challengeReadProducerNow(value))
            (void)challengeIngress_->copyRead(value);
        return true;
    }
    if (type != 2 && type != 3)
        return false;
    // All write/execute callbacks of the selected app are consumed, including
    // preconfiguration/stale/unknown-attribute requests. Never delayed/rebound.
    GattsWriteEvent event;
    event.connection = packed;
    event.interface = static_cast<std::uint8_t>(packed);
    event.transaction = transaction;
    char bytes[CommandFrameLimits::Frame];
    if (type == 2 && borrowed) {
        event.attribute = borrowed->write.handle;
        event.offset = borrowed->write.offset;
        event.count = borrowed->write.length;
        event.needResponse = borrowed->write.needResponse;
        event.prepared = borrowed->write.prepared;
        diagnostic::DiagnosticClosedHandles diagnostic;
        portENTER_CRITICAL(&mutex_);
        const bool installed = diagnostic_.state == 3;
        if (installed)
            diagnostic = diagnostic_.value;
        portEXIT_CRITICAL(&mutex_);
        if (installed && diagnostic.protectedWrite(event.attribute)) {
            GattsSourceRegistration original;
            if (sourceTask() && readRegistration(original) &&
                original == diagnostic.original && active_ &&
                packed == live_.connection &&
                (packed & 0xff) == diagnostic.original.interface &&
                event.needResponse)
                (void)GATTS_SendRsp(packed, transaction,
                                    static_cast<std::uint8_t>(
                                        AttWriteStatus::InsufficientAuthorization),
                                    nullptr);
            // The SDK actor only refuses the current original connection. A
            // successful local ATT error submission is no grant, execution or
            // peer-delivery proof. Do not even copy diagnostic request bytes
            // into the game/profile write buffer while this route is closed.
            return true;
        }
        if (event.count && event.count <= sizeof(bytes)) {
            std::memcpy(bytes, borrowed->write.value, event.count);
            event.bytes = bytes;
        }
    } else if (type == 3 && borrowed) {
        event.prepared = true;
        event.needResponse = true;
        // EXEC has no attribute or retained prepare buffer. Select our fixed
        // command endpoint only for the explicit unsupported error response.
        event.attribute = 0;
    }
    if (!sourceTask() || !owner_ || !responses_ || !active_ || packed != live_.connection ||
        !borrowed)
        return true;
    if (type == 3)
        event.attribute = command_;
    event.peerAddress = live_.peerAddress;
    if (issueAttribute_ && event.attribute == issueAttribute_) {
        if (type != 2 || event.needResponse || event.prepared || event.offset ||
            event.count != PeerChallengeIssueLimits::Bytes || !event.bytes || !issueIngress_)
            return true;
        PeerTransportBinding transport;
        if (!transportNow(transport))
            return true;
        PeerChallengeIssueRequest value;
        value.scope.original.device = transport.device;
        value.scope.original.boot = proofBoot_;
        value.scope.original.connection = transport.connection;
        value.scope.original.registration = transport.registration;
        value.scope.original.interface = transport.interface;
        value.scope.lifecycle = transport.lifecycle;
        value.scope.address = transport.address;
        value.scope.attribute = event.attribute;
        value.scope.transaction = transaction;
        std::memcpy(value.bytes.data(), bytes, value.bytes.size());
        value.scope.receivedAt = clock_.nowMilliseconds();
        if (challengeIssueProducerNow(value))
            (void)issueIngress_->copyIssue(value);
        return true;
    }
    if (proofAttribute_ && event.attribute == proofAttribute_) {
        if (type != 2 || event.needResponse || event.prepared || event.offset || !event.bytes ||
            event.count <= CommandFrameLimits::Header || event.count > CommandFrameLimits::Frame ||
            !authIngress_)
            return true;
        PeerTransportBinding transport;
        if (!transportNow(transport))
            return true;
        PeerProofWriteRequest v;
        v.scope.original.device = transport.device;
        v.scope.original.boot = proofBoot_;
        v.scope.original.connection = transport.connection;
        v.scope.original.registration = transport.registration;
        v.scope.original.interface = transport.interface;
        v.scope.lifecycle = transport.lifecycle;
        v.scope.address = transport.address;
        v.scope.attribute = event.attribute;
        v.scope.transaction = transaction;
        v.count = event.count;
        std::memcpy(v.bytes.data(), bytes, v.count);
        v.scope.receivedAt = clock_.nowMilliseconds();
        if (peerProofWriteProducerNow(v))
            (void)authIngress_->copyProof(v);
        return true;
    }
    const auto now = clock_.nowMilliseconds(); // After owned scalars and bounded bytes.
    if (event.attribute == profile_)
        admitOriginalGattsProfileWrite(event, *owner_, channel_, profiles_, *responses_, now);
    else if (event.attribute == command_)
        admitOriginalGattsWrite(event, *owner_, channel_, commands_, *responses_, now);
    return true;
}
bool Esp32GattsSource::configureChallengeReadNow(std::uint16_t attribute,
                                                 IPeerChallengeReadIngress &ingress) noexcept {
    if (!sourceTask() || !owner_ || sawConnection_ || !grantConfigured_ || challengeAttribute_ ||
        !attribute || attribute == command_ || attribute == profile_)
        return false;
    portENTER_CRITICAL(&mutex_);
    challengeAttribute_ = attribute;
    challengeIngress_ = &ingress;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::configureChallengeIssueNow(std::uint16_t attribute,
                                                  IPeerChallengeIssueIngress &ingress) noexcept {
    if (!sourceTask() || !owner_ || sawConnection_ || !grantConfigured_ || !challengeAttribute_ ||
        issueAttribute_ || !attribute || attribute == command_ || attribute == profile_ ||
        attribute == challengeAttribute_)
        return false;
    portENTER_CRITICAL(&mutex_);
    issueAttribute_ = attribute;
    issueIngress_ = &ingress;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::challengeIssueCurrent(
    const PeerChallengeIssueRequest &caller) const noexcept {
    const auto value = caller;
    portENTER_CRITICAL(&mutex_);
    const bool scope = issueAttribute_ && value.scope.attribute == issueAttribute_ && issueIngress_;
    const auto challenge = challengeAttribute_;
    portEXIT_CRITICAL(&mutex_);
    auto readScope = value.scope;
    readScope.attribute = challenge;
    return scope && !value.scope.offset && !value.scope.isLong && challengeReadCurrent(readScope);
}
bool Esp32GattsSource::challengeIssueProducerNow(
    const PeerChallengeIssueRequest &caller) const noexcept {
    const auto value = caller;
    auto readScope = value.scope;
    portENTER_CRITICAL(&mutex_);
    readScope.attribute = challengeAttribute_;
    portEXIT_CRITICAL(&mutex_);
    return challengeIssueCurrent(value) && challengeReadProducerNow(readScope) &&
           challengeIssueCurrent(value);
}
bool Esp32GattsSource::challengeReadCurrent(const PeerChallengeReadRequest &caller) const noexcept {
    const auto value = caller;
    portENTER_CRITICAL(&mutex_);
    const bool scope = challengeAttribute_ && value.attribute == challengeAttribute_ &&
                       challengeIngress_ && grantConfigured_ && value.original.boot == proofBoot_ &&
                       lifecycle_ == value.lifecycle && !registryFenced_;
    const auto *context = proofContext_;
    portEXIT_CRITICAL(&mutex_);
    return selected == this && scope && value.lifecycle && value.original.valid() && context &&
           closedPeerConnectionCurrent(value.original) && context->originalLive(value.original) &&
           closedPeerConnectionCurrent(value.original);
}
bool Esp32GattsSource::challengeReadProducerNow(
    const PeerChallengeReadRequest &caller) const noexcept {
    const auto value = caller;
    PeerTransportBinding transport;
    return sourceTask() && challengeReadCurrent(value) && transportNow(transport) &&
           transport.device == value.original.device &&
           transport.connection == value.original.connection &&
           transport.registration == value.original.registration &&
           transport.interface == value.original.interface &&
           transport.lifecycle == value.lifecycle && transport.address == value.address &&
           challengeReadCurrent(value);
}
bool Esp32GattsSource::validChallengeReply(const PeerChallengeReadReply &value) const noexcept {
    const auto &request = value.request; // All callers own a frozen entry copy.
    const auto fresh = [&]() {
        return clock_.nowMilliseconds() - request.receivedAt <
                   PeerChallengeReadLimits::MaxAgeMilliseconds &&
               challengeReadCurrent(request) &&
               clock_.nowMilliseconds() - request.receivedAt <
                   PeerChallengeReadLimits::MaxAgeMilliseconds;
    };
    if (!fresh())
        return false;
    if (value.status == 0) {
        const auto &snapshot = value.snapshot;
        PeerTranscript encoded;
        if ((!request.isLong && request.offset) || request.offset > encoded.size() ||
            !(snapshot.challenge.original == request.original) ||
            !(snapshot.challenge.peer == snapshot.credential.peer) ||
            !encodePeerChallenge(snapshot.challenge, encoded) || !proofContext_ ||
            !proofContext_->credentialCurrent(snapshot.credential) ||
            clock_.nowMilliseconds() - snapshot.issuedAt >= PeerProofLimits::MaxAgeMilliseconds ||
            !fresh() || !proofContext_->credentialCurrent(snapshot.credential) ||
            clock_.nowMilliseconds() - snapshot.issuedAt >= PeerProofLimits::MaxAgeMilliseconds)
            return false;
    } else if (value.status != 7 && value.status != 8)
        return false;
    return fresh();
}
bool Esp32GattsSource::postChallengeReadReply(const PeerChallengeReadReply &caller) noexcept {
    const auto value = caller;
    if (!challengeIngress_ || !challengeIngress_->workerNow() ||
        xTaskGetCurrentTaskHandle() == sourceTask_ || challengePosting_)
        return false;
    challengePosting_ = true;
    if (!validChallengeReply(value)) {
        challengePosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&mutex_);
    const bool busy = challengePending_.state != 0;
    portEXIT_CRITICAL(&mutex_);
    if (busy) {
        challengePosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&registrationMutex);
    const auto token = challengeReadPostCounter == UINT32_MAX ? 0 : ++challengeReadPostCounter;
    portEXIT_CRITICAL(&registrationMutex);
    if (!token) {
        challengePosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&mutex_);
    challengePending_.value = value;
    challengePending_.token = token;
    challengePending_.state = 1;
    portEXIT_CRITICAL(&mutex_);
    const bool posted =
        validChallengeReply(value) && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &sendChallengeReply,
                        reinterpret_cast<void *>(static_cast<std::uintptr_t>(token)), 0, 0);
    if (!posted) {
        portENTER_CRITICAL(&mutex_);
        challengePending_.state = 0;
        portEXIT_CRITICAL(&mutex_);
    }
    challengePosting_ = false;
    return posted;
}
void Esp32GattsSource::sendChallengeReply(void *context) noexcept {
    auto *source = selected;
    if (!source)
        return;
    const auto token = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
    portENTER_CRITICAL(&source->mutex_);
    const bool pending =
        token && token == source->challengePending_.token && source->challengePending_.state == 1;
    if (pending)
        source->challengePending_.state = 2;
    const auto value = source->challengePending_.value;
    portEXIT_CRITICAL(&source->mutex_);
    if (!pending)
        return;
    OriginalPeerSecurity security;
    if (source->sourceTask() && source->challengeReadProducerNow(value.request) &&
        source->validChallengeReply(value) &&
        (value.status != 0 ||
         source->readOriginalPeerSecurityNow(value.request.original, security)) &&
        source->challengeReadProducerNow(value.request) && source->validChallengeReply(value)) {
        sdk446::Response response = {};
        response.value.connection = value.request.original.connection.handle;
        response.value.handle = value.request.attribute;
        response.value.offset = value.request.offset;
        if (!value.status) {
            PeerTranscript transcript;
            if (encodePeerChallenge(value.snapshot.challenge, transcript)) {
                response.value.length = transcript.size() - value.request.offset;
                if (response.value.length)
                    std::memcpy(response.value.value, transcript.data() + value.request.offset,
                                response.value.length);
            }
        }
        // Immediate original BTU call. SDK consumes/copies this response here;
        // no response pointer escapes, posting never asserts peer delivery.
        (void)GATTS_SendRsp(value.request.original.connection.handle, value.request.transaction,
                            value.status, &response);
    }
    portENTER_CRITICAL(&source->mutex_);
    source->challengePending_.state = 0;
    portEXIT_CRITICAL(&source->mutex_);
}
bool Esp32GattsSource::postPeerAuthReply(const PeerAuthReadReply &caller) noexcept {
    const auto v = caller;
    if (!authIngress_ || !authIngress_->workerNow() || xTaskGetCurrentTaskHandle() == sourceTask_ ||
        authPosting_)
        return false;
    authPosting_ = true;
    if (!validAuthReply(v)) {
        authPosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&mutex_);
    const bool busy = authPending_.state != 0;
    portEXIT_CRITICAL(&mutex_);
    if (busy) {
        authPosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&registrationMutex);
    const auto token = peerAuthReadPostCounter == UINT32_MAX ? 0 : ++peerAuthReadPostCounter;
    portEXIT_CRITICAL(&registrationMutex);
    if (!token) {
        authPosting_ = false;
        return false;
    }
    portENTER_CRITICAL(&mutex_);
    authPending_.value = v;
    authPending_.token = token;
    authPending_.state = 1;
    portEXIT_CRITICAL(&mutex_);
    const bool posted =
        validAuthReply(v) && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &sendAuthReply,
                        reinterpret_cast<void *>(static_cast<std::uintptr_t>(token)), 0, 0);
    if (!posted) {
        portENTER_CRITICAL(&mutex_);
        authPending_.state = 0;
        portEXIT_CRITICAL(&mutex_);
    }
    authPosting_ = false;
    return posted;
}
void Esp32GattsSource::sendAuthReply(void *context) noexcept {
    auto *source = selected;
    if (!source)
        return;
    const auto token = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
    portENTER_CRITICAL(&source->mutex_);
    const bool pending =
        token && token == source->authPending_.token && source->authPending_.state == 1;
    if (pending)
        source->authPending_.state = 2;
    const auto v = source->authPending_.value;
    portEXIT_CRITICAL(&source->mutex_);
    if (!pending)
        return;
    OriginalPeerSecurity security;
    if (source->sourceTask() && source->peerAuthReadProducerNow(v.request) &&
        source->validAuthReply(v) &&
        (v.status != 0 || source->readOriginalPeerSecurityNow(v.request.original, security)) &&
        source->peerAuthReadProducerNow(v.request) && source->validAuthReply(v)) {
        sdk446::Response response = {};
        response.value.connection = v.request.original.connection.handle;
        response.value.handle = v.request.attribute;
        response.value.offset = v.request.offset;
        if (!v.status) {
            std::array<std::uint8_t, 32> bytes;
            if (encodePeerAuthResult(v.result, bytes)) {
                response.value.length = bytes.size() - v.request.offset;
                if (response.value.length)
                    std::memcpy(response.value.value, bytes.data() + v.request.offset,
                                response.value.length);
            }
        }
        (void)GATTS_SendRsp(v.request.original.connection.handle, v.request.transaction, v.status,
                            &response);
    }
    portENTER_CRITICAL(&source->mutex_);
    source->authPending_.state = 0;
    portEXIT_CRITICAL(&source->mutex_);
}
bool Esp32GattsSource::originalLive(const GattsWriteBinding &caller) const noexcept {
    const auto original = caller;
    portENTER_CRITICAL(&mutex_);
    auto *owner = owner_;
    portEXIT_CRITICAL(&mutex_);
    return owner && owner->originalLive(original);
}
bool Esp32GattsSource::validResponse(const RoutedWriteResponse &value) const noexcept {
    if (value.attribute != value.original.attribute ||
        (value.original.connection.handle & 0xff) != value.original.interface)
        return false;
    switch (value.status) {
    case AttWriteStatus::Ok:
    case AttWriteStatus::InvalidHandle:
    case AttWriteStatus::InvalidPdu:
    case AttWriteStatus::RequestUnsupported:
    case AttWriteStatus::InvalidOffset:
    case AttWriteStatus::InsufficientAuthorization:
    case AttWriteStatus::InvalidValueLength:
    case AttWriteStatus::UnlikelyError:
    case AttWriteStatus::InsufficientResources:
        break;
    default:
        return false;
    }
    return clock_.nowMilliseconds() - value.receivedAt < GattsResponseLimits::MaxAgeMilliseconds &&
           originalLive(value.original);
}
bool Esp32GattsSource::submitOriginal(const RoutedWriteResponse &caller) noexcept {
    const auto value = caller; // Freeze before external clock/lifetime observation.
    if (posting_)
        return false;
    posting_ = true;
    if (!validResponse(value)) {
        posting_ = false;
        return false;
    }
    Pending *item = nullptr;
    portENTER_CRITICAL(&mutex_);
    for (auto &candidate : pending_) {
        if (!candidate.state) {
            candidate.response = value;
            candidate.owner = this;
            candidate.state = 1;
            item = &candidate;
            break;
        }
    }
    portEXIT_CRITICAL(&mutex_);
    // Worker may race with consumption. The slot stays occupied from reservation
    // through the real SDK call. SDK false post guarantees no callback is queued.
    const bool posted =
        item && sdkThread_ && sdkThread_ == btu_thread &&
        osi_thread_post(static_cast<osi_thread *>(sdkThread_), &consume, item, 0, 0);
    if (item && !posted) {
        portENTER_CRITICAL(&mutex_);
        item->state = 0;
        portEXIT_CRITICAL(&mutex_);
    }
    posting_ = false;
    return posted; // Local SDK posting only; no peer-delivery or execution claim.
}
void Esp32GattsSource::consume(void *context) noexcept {
    auto *item = static_cast<Pending *>(context);
    if (!item || !item->owner)
        return;
    auto &source = *item->owner;
    portENTER_CRITICAL(&source.mutex_);
    const bool occupied = item->state == 1;
    if (occupied)
        item->state = 2;
    const auto value = item->response;
    portEXIT_CRITICAL(&source.mutex_);
    if (!occupied)
        return;
    // Same BTU actor as earliest conn/write callbacks. No BTC or BTA re-post:
    // final original/age check followed directly by actual GATT processing.
    if (source.sourceTask() && source.validResponse(value))
        GATTS_SendRsp(value.original.connection.handle, value.transaction,
                      static_cast<std::uint8_t>(value.status), nullptr);
    portENTER_CRITICAL(&source.mutex_);
    item->state = 0;
    portEXIT_CRITICAL(&source.mutex_);
    // SDK status is a local result only. Failure never retries or frees input.
}
} // namespace companion
} // namespace larp
extern "C" std::uint8_t __wrap_GATT_Register(larp::companion::sdk446::Uuid *uuid,
                                             const larp::companion::sdk446::Callbacks *callbacks) {
    using namespace larp::companion;
    if (!selected || !uuid || uuid->len != 2 || uuid->uu.uuid16 != application)
        return __real_GATT_Register(uuid, callbacks);
    if (registrationAttempted || !callbacks || !callbacks->connection || !callbacks->request)
        return 0; // One selected registration per boot; fail closed on retry/reuse.
    registrationAttempted = true;
    originalCallbacks = *callbacks;
    auto installed = *callbacks;
    installed.connection = &connectionCallback;
    installed.request = &requestCallback;
    const auto interface = __real_GATT_Register(uuid, &installed);
    if (!selected->registeredSourceNow(interface))
        return 0;
    return interface;
}

extern "C" bool __wrap_SMP_Register(larp::companion::sdk446::SmpCallback callback) {
    using namespace larp::companion;
    if (!selected)
        return __real_SMP_Register(callback);
    if (smpRegistrationAttempted || !callback)
        return false;
    smpRegistrationAttempted = true;
    originalSmpCallback = callback;
    smpRegistered = __real_SMP_Register(&smpCallback);
    return smpRegistered;
}
