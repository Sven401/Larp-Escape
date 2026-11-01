#pragma once
#include "DeviceRuntime/RulesBundleJournal.h"
#include "DeviceRuntime/RulesCommitOrdering.h"
#include "DeviceRuntime/RulesProgressControl.h"
#include "DeviceRuntime/RulesProgressEffect.h"
#include "DeviceRuntime/RulesProgressTransaction.h"
#include "GameExecution/ManualOverrideOwner.h"

namespace larp {
namespace devices {
namespace rules_progress {
struct ClockObservation {
    rules::Identity boot;
    std::uint64_t monotonic = 0;
};
class IClock {
  public:
    virtual ~IClock() = default;
    virtual bool read(ClockObservation &) = 0;
};
// Called ONLY after the original claim is durably committed. All argument
// views expire on return and must not be retained. Return describes the local
// handler, never physical observation or D0 observed. Missing/failed ownership
// or uncertain handoff is Unknown; recovery never calls the handler again.
class IIntentHandler {
  public:
    virtual ~IIntentHandler() = default;
    virtual HandlerResult deliver(const rules::Context &, const rules::Identity &application,
                                  std::uint16_t index, const rules::Intent &) = 0;
};
enum class ControlEligibility : std::uint8_t { Current, Denied, Unavailable };
class IControlFinalFence {
  public:
    virtual ~IControlFinalFence() = default;
    // Original accepted control/install request, connection/time/revisions;
    // checkpoint/lifecycle-local origin is a distinct original source. No
    // current-target lookup or generic GM flag. Preflight proves availability
    // and current original eligibility BEFORE any inert catalog write. Check
    // runs freshly only in the actual frozen Control commit callback under the
    // serialized metadata hold.
    virtual ControlEligibility preflight(const rules::Program &, const rules::State &,
                                         const Control &) {
        return ControlEligibility::Unavailable;
    }
    virtual ControlEligibility check(const rules::Program &, const rules::State &before,
                                     const Control &original, const ProgressRecord &) = 0;
};
enum class IntentEligibility : std::uint8_t { Current, Denied, Unavailable };
class IIntentEligibility {
  public:
    virtual ~IIntentEligibility() = default;
    // Full original accepted plan/authorization/causes and immutable reservation,
    // both original and installed program scopes, and exact frozen claim bytes
    // represented by Effect plus its actual physical metadata. Never substitute
    // a current different principal/connection or a new resource assignment.
    // Callback views expire on return. Missing actual owner proof is Unavailable.
    virtual IntentEligibility
    check(const rules::Program &originalProgram, const rules::Program &installedProgram,
          const rules::State &current, const Transaction &originalEnvelope,
          const codec::Application &originalApplication, const IntentReservation &,
          const rules::Intent &, const Effect &frozenClaim, const ProgressRecord &) = 0;
};
enum class IntentStage : std::uint8_t { Pending, Attempted, Returned, Failed, Unknown };
struct CatalogEntry {
    rules::Digest digest, deployment;
    std::uint32_t first = 0, count = 0;
};
struct ApplicationEntry {
    rules::Identity operation;
    rules::Digest request, catalog, blob;
    std::uint32_t first = 0, count = 0;
    ProgressRecord final;
    rules::Fence before;
    rules::ReleaseDisposition release = rules::ReleaseDisposition::Unresolved;
};
struct ControlEntry {
    rules::Identity operation;
    ControlKind kind = ControlKind::Checkpoint;
    rules::Digest request, raw, catalog;
    rules::Fence before;
    std::uint32_t first = 0, count = 0;
    ProgressRecord record;
};
struct IntentEntry {
    rules::Identity application;
    IntentReservation reservation;
    rules::Digest catalog;
    IntentStage stage = IntentStage::Pending;
    // Freeze only at the FIRST actual physical claim/result attempt. A retry
    // never adopts today's revision or changes the original bytes/identity.
    ProgressRequest claim, result;
    rules::Span<std::uint8_t> claimBytes, resultBytes;
    rules::Fence originalClaimFence, resultBefore;
    rules::Identity originalClaimBoot, restoredBy;
    bool resultCommitted = false, settlementRequired = false;
    HandlerResult handlerResult = HandlerResult::None;
};
struct StateBank {
    rules::StateStorage storage;
    rules::State state;
};
struct Workspace {
    // Three disjoint banks: live remains untouched while recovery uses two
    // inactive banks. Every Evidence pointer refers to its bank's owned pool.
    StateBank banks[3];
    rules::Program historicalProgram;
    codec::Application prepared, decoded;
    codec::ApplicationStorage decodedStorage;
    EvidenceStorage preparedEvidence;
    rules::Plan computed;
    rules::PlannerWorkspace planner;
    Transaction envelope;
    rules::Pool<std::uint8_t> applicationBytes, decodedApplication, comparison;
    // Complete original historical catalog/transaction bytes; no borrowed
    // callback buffers. Capacities are an explicit host operating profile.
    rules::Pool<std::uint8_t> history, physicalBytes;
    rules::Pool<CatalogEntry> catalogs;
    rules::Pool<ApplicationEntry> applications;
    rules::Pool<ControlEntry> controls;
    rules::Pool<IntentEntry> intents;
};
enum class Phase : std::uint8_t {
    Uninitialized,
    Recovering,
    Stopped,
    Ready,
    Preparing,
    WritingCatalog,
    WritingApplication,
    WritingControl,
    Unknown,
    Fault,
    Dispatching
};
enum class Error : std::uint8_t {
    None,
    Busy,
    Argument,
    Profile,
    Capacity,
    Context,
    Stale,
    Crypto,
    Source,
    Admission,
    Clock,
    CheckpointOverrun,
    Codec,
    Range,
    Conflict,
    Recovery,
    Journal,
    Consumer
};
struct Result {
    Phase phase = Phase::Uninitialized;
    Error error = Error::None;
    rules::Result planner;
    rules_bundle::Result journal;
    // Resolved original durable Application only; never the caller's new
    // correlation operation. Zero for controls, unknown and unaccepted input.
    rules::Identity originalApplication;
};
// Actual semantic D1 coordinator. No radio, private-key provisioning, implicit
// startup, public role assignment, erase or partition changes. All IO/crypto on
// one exclusive worker; source events/authority changes obey ICommitOrdering.
// Signed/input/install ports are MANDATORY. Missing actual provenance is closed,
// never a synthetic or unsigned product fallback. <=32768 whole transaction,
// <=16 chunks+1 Final and all later intent slots checked BEFORE first write.
class Owner final : public rules::IManualTransaction,
                    private rules_bundle::IRecoveryConsumer,
                    private rules_bundle::ICommitFence,
                    private IProgressCommitFence {
  public:
    Owner(const rules::Program &installed, ProgressJournal &, rules_bundle::Journal &,
          rules_bundle::Workspace &, configuration::IPackageDigest &,
          rules_bundle::IIdentitySource &, IClock &, ICommitOrdering &, ISignedPlanEvidence &,
          ISignedControlEvidence &, IControlFinalFence &, IIntentEligibility &, IIntentHandler &,
          Workspace &);
    Owner(const Owner &) = delete;
    Owner &operator=(const Owner &) = delete;
    // Only after actual initialize or genuine backend reopen+lowlevel recover.
    // Rebuild ALL original catalogs/applications/effect receipts provisionally;
    // bad/missing evidence aborts the whole result. New process is stopped until
    // explicit qualified checkpoint-rest/lifecycle decision, never auto-resume.
    Result recover();
    Result activate(const rules::Identity &operation, const rules::Digest &request);
    Result control(ControlKind, const rules::Identity &operation, const rules::Digest &request);
    Result submit(const rules::Input &, const rules::Identity &operation,
                  const rules::Digest &request);
    rules::ManualResult submit(const rules::OverrideAdmission &,
                               rules::IManualFinalFence &) override;
    rules::ManualResult status(const rules::Context &, const rules::Identity &operation) override;
    Result service();
    Result dispatch(const rules::Identity &application, std::uint16_t index);
    Result settleUnknown(const rules::Identity &application, std::uint16_t index);
    // LastPublished diagnostic view only. Fault/Unknown may retain the old
    // Running mode and valid confirmed history. This reference is NEVER a
    // Current/F5a/D2 admission or permission to submit/execute. Action paths
    // separately require the complete successfully recovered owner scope.
    // Views expire at the next serialized mutation; the owner object itself
    // remains stable for the tightly coupled ManualOverrideOwner reference.
    const rules::State &state() const {
        return live_;
    }
    Phase phase() const {
        return phase_;
    }
    // Copy-only published original output, only after Committed/Duplicate.
    // Still requires actual D0 owner admission before any CurrentSolved/D2 use.
    bool outbox(const rules::Identity &application, std::size_t index, codec::SignedFact &,
                rules::Pool<std::uint8_t> &ownedCose);

  private:
    const rules::Program &installed_;
    ProgressJournal &journal_;
    rules_bundle::Journal &bundle_;
    rules_bundle::Workspace &bundleWork_;
    configuration::IPackageDigest &hash_;
    rules_bundle::IIdentitySource &ids_;
    IClock &clock_;
    ICommitOrdering &ordering_;
    ISignedPlanEvidence &evidence_;
    ISignedControlEvidence &controls_;
    IControlFinalFence &controlFence_;
    IIntentEligibility &intentEligibility_;
    IIntentHandler &handler_;
    Workspace &work_;
    rules::Planner planner_;
    rules::State live_, planning_;
    rules::Digest catalog_;
    ClockObservation anchor_;
    ClockWitness pendingClock_;
    // Transient only within the synchronous ManualOwner submit/service call;
    // clear on EVERY return. Never retain this as authority between worker steps.
    rules::IManualFinalFence *manualFence_ = nullptr;
    rules_bundle::Key pendingKey_;
    Control pendingControl_;
    Phase phase_ = Phase::Uninitialized;
    bool operating_ = false, ordered_ = false, recovered_ = false;
    bool rebuilding_ = false, activated_ = false, committing_ = false;
    bool pendingDefinitelyNotCommitted_ = false;
    rules::Identity heldOperation_;
    OrderingLease lease_;
    OrderingSnapshot recoveryOrdering_;
    bool recoveryPinned_ = false;
    ProgressRequest auxiliary_;
    Effect pendingEffect_;
    std::size_t dispatching_ = SIZE_MAX;
    bool effectScope_ = false;
    std::size_t baseApplications_ = 0, baseIntents_ = 0, baseControls_ = 0, preparedHistory_ = 0;
    rules::Digest activeCatalog_;
    rules::State rebuilt_, pendingState_;
    ApplicationEntry pendingApplication_;
    CatalogEntry pendingCatalog_;
    ControlEntry pendingControlEntry_;
    bool auxiliaryFrozen_ = false, pendingCatalogNew_ = false;
    std::uint8_t pendingBank_ = 0;
    ClockObservation recoveredAnchor_, replayAnchor_;
    std::uint8_t liveBank_ = 0, replayBank_ = 1;
    std::size_t reservedIntents_ = 0;
    Result result_;
    Error clockError_ = Error::None;
    bool begin(const ProgressSnapshot &) override;
    bool catalog(const rules_bundle::Key &, const std::uint8_t *, std::size_t) override;
    bool application(const rules_bundle::Key &, const ProgressRecord &, const std::uint8_t *,
                     std::size_t) override;
    bool auxiliary(const ProgressRecord &, const std::uint8_t *, std::size_t) override;
    bool prepare() override;
    void commit() override;
    void abort() override;
    bool allow(const rules_bundle::Key &, rules_bundle::Phase, const ProgressRecord &) override;
    bool allow(const ProgressRecord &) override;
    bool workspace(const void *output = nullptr, std::size_t outputBytes = 0,
                   const void *descriptor = nullptr, std::size_t descriptorBytes = 0,
                   const void *buffer = nullptr, std::size_t bufferBytes = 0) const;
    bool loadCatalog(const rules::Digest &);
    bool remember(const std::uint8_t *, std::size_t, std::uint32_t &);
    rules::Capacities capacities(const StateBank &) const;
    bool sameApplication(const codec::Application &, rules::Plan &);
    bool clock(ClockWitness &, rules::State &);
    bool finalClock(const rules::OverrideRecord *original = nullptr);
    bool acquire(const CommitBinding &);
    bool releaseOrdering(ProgressOutcome);
    bool recoveryResolution(ProgressOutcome &);
    bool physicalCapacity(std::size_t additionalPhases);
    bool loadApplication(const ApplicationEntry &);
    const ProgressRecord *record(const rules::Identity &) const;
    Result effect(std::size_t index, EffectPhase, HandlerResult, bool settlement = false);
    Result finishDispatch();
    Result advance();
    Result plan(const rules::Input &, const rules::Identity &, const rules::Digest &);
    Result writeControl(ControlKind, const rules::Identity &, const rules::Digest &);
    bool uniquePhase(const rules::Identity &) const;
    rules::ReleaseDisposition release(const codec::Application &) const;
    Result fail(Error, bool fault = false);
};
} // namespace rules_progress
} // namespace devices
} // namespace larp
