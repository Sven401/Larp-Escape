#pragma once
#include "Companion/PeerAuthResult.h"
#include "Companion/PeerChallengeIssue.h"
#include "Companion/PeerLinkSecurity.h"
#include "Companion/PeerProof.h"
#include "Companion/PeerProofGrant.h"
#include "Companion/PeerRegistryActor.h"
#include "FreeRtosGattsConnection.h"
#include "GattsServiceDefinition.h"
#include "SdkGattSourceAbi.h"
#include "SdkPeerSecurityAbi.h"
#include "../ClosedBleStackOwner/DiagnosticGattsDefinition.h"
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
#include "CompanionIntegration/DiagnosticDevelopmentControl.h"
#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"
#endif
#include <freertos/task.h>
namespace larp {
namespace companion {
// Process lifetime; one BTU source actor owns all lifecycle/IdentityAccess work,
// one response worker posts. Register once per boot; no stack stop/restart,
// deregistration or deletion while callbacks/work may remain in the SDK.
// Only the selected source registration is intercepted. Its write callbacks
// replace the BTA write path; other callbacks and other applications pass through.
class Esp32GattsSource : public IGattsResponseContext,
                         public IGattsResponseSubmitter,
                         private IIdentityAccessObserver {
  public:
    Esp32GattsSource(IdentityAccess &access, const IProfileImportClock &clock,
                     const IGameControlChannel &channel, FreeRtosCommandFrameQueue &commands,
                     FreeRtosProfileFrameQueue &profiles)
        : access_(access), clock_(clock), channel_(channel), commands_(commands),
          profiles_(profiles) {}
    Esp32GattsSource(const Esp32GattsSource &) = delete;
    Esp32GattsSource &operator=(const Esp32GattsSource &) = delete;
    // Registration wrapper on the actual BTU source task, before any link.
    bool registeredSourceNow(std::uint8_t interface) noexcept;
    bool readRegistration(GattsSourceRegistration &) const noexcept;
    GattsEndpointSetupPost postEndpointSetup(const GattsEndpointSetup &, IWriteResponseBearer &,
                                             const PeerChallengeReadSetup & = {}) noexcept;
    GattsEndpointSetupState endpointSetupState(const GattsEndpointSetup &) const noexcept;
    // Trusted sole registry worker, copied original before deferring to BTU.
    bool readPeerRoleOrigin(const Identity &, PeerRegistryActorOrigin &) const noexcept;
    PeerRegistryRolePost postPeerRoleJob(const PeerRegistryRoleJob &) noexcept;
    PeerRegistryRoleState peerRoleState(const PeerRegistryRoleJob &,
                                        TrustedAuthority &) const noexcept;
    void fencePeerRoles(const PeerRegistryRoleJob &) noexcept;
    // Copied keys are tied to this installed job, but remain usable on fresh links.
    bool installedPeerRoles(const PeerRegistryRoleJob &) const noexcept;
    // Once before any link, on the original actor; context must be a protected
    // immutable-boot/public-registry adapter, not the mutable crypto owner.
    bool configurePeerProofGrantNow(const BootIdentity &, const IPeerProofContext &) noexcept;
    bool readPeerProofGrantOrigin(const PeerProofBinding &,
                                  PeerRegistryActorOrigin &) const noexcept;
    PeerProofGrantPost postPeerProofGrant(const PeerProofGrantJob &) noexcept;
    PeerProofGrantState peerProofGrantState(const PeerProofGrantJob &, TrustedAuthority &) noexcept;
    // Sole crypto worker. Fence first, atomically close only its copied owned
    // publication, retain a close ACK for later original-actor reconciliation.
    void cancelPeerProofGrant(const PeerProofGrantJob &) noexcept;
    // Once, on that same task, using actual created attribute handles, before
    // ANY connection callback. Repeated/late configuration never rebinds.
    bool configureEndpointsNow(std::uint16_t command, std::uint16_t profile,
                               IWriteResponseBearer &responses) noexcept;
    // Once before advertising: install the exact diagnostic handle set on the
    // original BTU actor. This installs only a rejection fence, never an owner.
    bool postDiagnosticClosedSetup(const diagnostic::DiagnosticClosedHandles &) noexcept;
    bool diagnosticClosedSetupInstalled(const diagnostic::DiagnosticClosedHandles &) const noexcept;
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
    struct DevelopmentRawWork {
        diagnostic::DevelopmentRawStepRequest request;
        PeerTransportBinding original;
        std::uint32_t token = 0, receivedAtMs = 0;
    };
    struct DevelopmentRawResult {
        DevelopmentRawWork work;
        diagnostic::RawExpanderObservation observation;
        std::uint32_t startedAtMs = 0, endedAtMs = 0;
    };
    struct DevelopmentLedWork {
        diagnostic::DevelopmentLedStepRequest request;
        PeerTransportBinding original;
        std::uint32_t token = 0, receivedAtMs = 0;
    };
    struct DevelopmentLedResult {
        DevelopmentLedWork work;
        std::uint32_t startedAtMs = 0, endedAtMs = 0;
        std::uint8_t renderCalls = 0;
        bool blackSubmitted = false, timerFired = false, softwareFault = true;
    };
    // Immutable public identity for the development-only encrypted read.
    bool configureDevelopmentInfo(const Identity &, const BootIdentity &) noexcept;
    bool configureDevelopmentControl(diagnostic::DevelopmentControl &) noexcept;
    bool takeDevelopmentRawWork(DevelopmentRawWork &) noexcept;
    bool postDevelopmentRawResult(const DevelopmentRawResult &) noexcept;
    bool takeDevelopmentLedWork(DevelopmentLedWork &) noexcept;
    bool developmentLedWorkCurrent(const DevelopmentLedWork &) noexcept;
    bool postDevelopmentLedResult(const DevelopmentLedResult &) noexcept;
#endif
    GattsLifecycleOutcome connectionNow(std::uint8_t interface, const std::uint8_t *address,
                                        std::uint16_t packedConnection, bool connected) noexcept;
    // Sole source actor only; original closed token for subsequent peer verification.
    bool originalConnectionNow(BoundConnection &) const noexcept;
    // Read-only copy of the current original BTU connection birth. This is
    // connection evidence only: no bond, MITM, role or diagnosis assertion.
    bool readOriginalTransport(PeerTransportBinding &) const noexcept;
    // The original BTU MTU request belongs to this complete connection birth.
    // No value is available before that request; an MTU is transport capacity,
    // never peer authority or permission to accept a diagnostic write.
    bool readDiagnosticOriginalMtu(const PeerTransportBinding &,
                                   std::uint16_t &) const noexcept;
    // Monotone original callback lifecycle, including disconnect; no link claim.
    bool readOriginalLifecycle(std::uint32_t &) const noexcept;
    // Exact original observer/channel agreement with no principal or GM.
    // Anonymous connection and closed post-disconnect epochs are permitted.
    bool closedChannelState(const Identity &) const noexcept;
    // Crypto-worker read: copied original connection and current owned policy.
    bool closedPeerConnectionCurrent(const PeerProofBinding &) const noexcept;
    // Earliest original SMP actor callback, before the existing BTM callback.
    // Forward original event/pointer. No SDKsecurity mutation or role grant;
    // failed original pairing closes only this owned local identity policy.
    bool smpEventNow(std::uint8_t, const std::uint8_t *, const sdk446::SmpData *) noexcept;
    void smpForwardedNow() noexcept;
    // Trusted sole actor at grant: current real SDK metadata on frozen original.
    // Fresh pairing scope; persisted bond-mode restore is still separate.
    bool readOriginalPeerSecurityNow(const PeerProofBinding &,
                                     OriginalPeerSecurity &) const noexcept;
    // Returns true only for our consumed WRITE/EXEC callbacks, even on rejection.
    // No raw SDK pointer or event without its original binding is queued.
    bool requestNow(std::uint16_t packedConnection, std::uint32_t transaction, std::uint8_t type,
                    const sdk446::Request *) noexcept;
    bool configureChallengeReadNow(std::uint16_t, IPeerChallengeReadIngress &) noexcept;
    bool configureChallengeIssueNow(std::uint16_t, IPeerChallengeIssueIngress &) noexcept;
    bool challengeIssueCurrent(const PeerChallengeIssueRequest &) const noexcept;
    bool challengeIssueProducerNow(const PeerChallengeIssueRequest &) const noexcept;
    bool challengeReadCurrent(const PeerChallengeReadRequest &) const noexcept;
    bool challengeReadProducerNow(const PeerChallengeReadRequest &) const noexcept;
    bool postChallengeReadReply(const PeerChallengeReadReply &) noexcept;
    bool configurePeerAuthNow(std::uint16_t, std::uint16_t, IPeerAuthIngress &) noexcept;
    bool peerProofWriteCurrent(const PeerProofWriteRequest &) const noexcept;
    bool peerProofWriteProducerNow(const PeerProofWriteRequest &) const noexcept;
    bool peerAuthReadCurrent(const PeerChallengeReadRequest &) const noexcept;
    bool peerAuthReadProducerNow(const PeerChallengeReadRequest &) const noexcept;
    bool postPeerAuthReply(const PeerAuthReadReply &) noexcept;
    bool originalLive(const GattsWriteBinding &caller) const noexcept override;
    std::uint32_t nowMilliseconds() const noexcept override {
        return clock_.nowMilliseconds();
    }
    bool submitOriginal(const RoutedWriteResponse &) noexcept override;

  private:
    struct Pending {
        Esp32GattsSource *owner = nullptr;
        RoutedWriteResponse response;
        std::uint8_t state = 0; // 0 free, 1 posted, 2 consuming
    };
    struct SetupPending {
        GattsEndpointSetup value;
        IWriteResponseBearer *responses = nullptr;
        PeerChallengeReadSetup challenge;
        std::uint8_t state = 0; // 0 unused, 1 queued, 2 consuming, 3 installed, 4 rejected
    } setup_;
    struct DiagnosticPending {
        diagnostic::DiagnosticClosedHandles value;
        std::uint8_t state = 0; // 0 unused, 1 posted, 2 consuming, 3 installed, 4 rejected
    } diagnostic_;
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
    Identity developmentDevice_;
    BootIdentity developmentBoot_;
    bool developmentInfoConfigured_ = false;
    diagnostic::DevelopmentControl *developmentControl_ = nullptr;
    bool developmentSubscribed_ = false;
    std::uint16_t developmentRepliesUsed_ = 0;
    struct DevelopmentRawPending {
        DevelopmentRawWork work;
        diagnostic::RawExpanderObservation observation;
        std::uint32_t startedAtMs = 0, endedAtMs = 0;
        std::uint8_t state = 0; // 0 free, 1 ready, 2 worker, 3 posted, 4 ACK reservation
    } developmentRaw_;
    std::uint32_t developmentRawToken_ = 0;
    struct DevelopmentLedPending {
        DevelopmentLedWork work;
        std::uint32_t startedAtMs = 0, endedAtMs = 0;
        std::uint8_t renderCalls = 0;
        bool blackSubmitted = false, timerFired = false, softwareFault = true;
        std::uint8_t state = 0; // 0 free, 1 ready, 2 worker, 3 posted, 4 ACK reservation
    } developmentLed_;
    std::uint32_t developmentLedToken_ = 0;
    std::uint8_t developmentRawRepliesUsed_ = 0;
    std::uint8_t developmentLedRepliesUsed_ = 0;
    bool readDevelopmentLinkNow(const PeerTransportBinding &,
                                diagnostic::DevelopmentLink &) const noexcept;
    static void sendDevelopmentRawResult(void *) noexcept;
    static void sendDevelopmentLedResult(void *) noexcept;
#endif
    struct RolePending {
        PeerRegistryRoleJob value;
        TrustedAuthority ack;
        std::uint32_t postToken = 0;
        std::uint8_t state = 0;
    } role_;
    struct ProofGrantPending {
        PeerProofGrantJob value;
        TrustedAuthority ack, closeExpected, closeAck;
        std::uint32_t postToken = 0;
        std::uint8_t state = 0;
    } grant_;
    struct ChallengePending {
        PeerChallengeReadReply value;
        std::uint32_t token = 0;
        std::uint8_t state = 0;
    } challengePending_;
    std::uint16_t challengeAttribute_ = 0;
    IPeerChallengeReadIngress *challengeIngress_ = nullptr;
    bool challengePosting_ = false;
    std::uint16_t issueAttribute_ = 0;
    IPeerChallengeIssueIngress *issueIngress_ = nullptr;
    bool validChallengeReply(const PeerChallengeReadReply &) const noexcept;
    static void sendChallengeReply(void *) noexcept;
    struct AuthPending {
        PeerAuthReadReply value;
        std::uint32_t token = 0;
        std::uint8_t state = 0;
    } authPending_;
    std::uint16_t proofAttribute_ = 0, resultAttribute_ = 0;
    IPeerAuthIngress *authIngress_ = nullptr;
    bool authPosting_ = false;
    bool validAuthReply(const PeerAuthReadReply &) const noexcept;
    bool authAckCurrent(const PeerAuthReadReply &) const noexcept;
    bool authScopeCurrent(const PeerChallengeReadRequest &, bool) const noexcept;
    bool authProducerNow(const PeerChallengeReadRequest &, bool) const noexcept;
    static void sendAuthReply(void *) noexcept;
    BootIdentity proofBoot_;
    const IPeerProofContext *proofContext_ = nullptr;
    TaskHandle_t proofWorker_ = nullptr;
    bool grantConfigured_ = false, grantPosting_ = false;
    TaskHandle_t roleWorker_ = nullptr;
    std::uint32_t lifecycle_ = 0;
    bool rolePosting_ = false, registryFenced_ = false;
    IdentityAccess &access_;
    const IProfileImportClock &clock_;
    const IGameControlChannel &channel_;
    FreeRtosCommandFrameQueue &commands_;
    FreeRtosProfileFrameQueue &profiles_;
    IWriteResponseBearer *responses_ = nullptr;
    alignas(FreeRtosGattsConnection) std::uint8_t
        ownerStorage_[sizeof(FreeRtosGattsConnection)] = {};
    FreeRtosGattsConnection *owner_ = nullptr;
    GattsConnectionEvent live_;
    BoundConnection connection_;
    PeerTransportBinding publishedTransport_;
    PeerTransportBinding diagnosticMtuBirth_;
    std::uint16_t diagnosticMtu_ = 0;
    // BTU-only transport subscription state. An ATT ACK alone is never a
    // delivery claim; each flag belongs to one exact original link birth.
    PeerTransportBinding diagnosticProbeBirth_;
    bool controlProbeSubscribed_ = false, stepProbeSubscribed_ = false;
    IdentityAccessSnapshot identitySnapshot_;
    PeerLinkSecurity linkSecurity_;
    bool smpForwarding_ = false;
    bool peerProofOpen_ = false;
    void *sdkThread_ = nullptr;
    TaskHandle_t sourceTask_ = nullptr;
    std::uint16_t command_ = 0, profile_ = 0;
    std::uint8_t interface_ = 0xff;
    std::uint32_t registrationToken_ = 0;
    bool setupPosting_ = false;
    bool registered_ = false, sawConnection_ = false, active_ = false, posting_ = false;
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    Pending pending_[GattsResponseLimits::Depth];
    bool transportNow(PeerTransportBinding &) const noexcept;
    bool proofGrantScopeCurrent(const PeerProofGrantJob &, bool beforeGrant) const noexcept;
    void reconcileProofGrantCloseNow() noexcept;
    static void applyPeerProofGrant(void *) noexcept;
    void copyIdentitySnapshot(const IdentityAccessSnapshot &) noexcept override;
    bool sourceTask() const noexcept;
    bool validResponse(const RoutedWriteResponse &) const noexcept;
    static void consume(void *) noexcept;
    static void installEndpoints(void *) noexcept;
    static void installDiagnosticClosed(void *) noexcept;
    static void applyPeerRoles(void *) noexcept;
    bool roleOriginalCurrent(const PeerRegistryActorOrigin &) const noexcept;
};
// Before SDK initialization; single process-lifetime instance/application.
// Successful arming is not registration, service creation, BLE start or identity.
bool armGattsSourceBeforeSdkStart(std::uint16_t application, Esp32GattsSource &) noexcept;
} // namespace companion
} // namespace larp
