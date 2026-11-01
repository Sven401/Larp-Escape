#include "DeviceRuntime/RulesProgressOwner.h"
#include <cstring>
#include <limits>

namespace larp {
namespace devices {
namespace rules_progress {
namespace {
template <typename T> bool nonzero(const T &a) {
    for (auto b : a.bytes)
        if (b)
            return true;
    return false;
}
template <typename A, typename B> bool bytesEqual(const A &a, const B &b) {
    return sizeof(a) == sizeof(b) && !std::memcmp(&a, &b, sizeof(a));
}
bool hash(configuration::IPackageDigest &port, const std::uint8_t *data, std::size_t count,
          rules::Digest &out) {
    out = {};
    bool ok = port.abort();
    if (ok)
        ok = port.begin();
    for (std::size_t at = 0; ok && at < count;) {
        const std::size_t n = count - at < 256 ? count - at : 256;
        ok = port.update(data + at, n);
        at += n;
    }
    std::size_t written = 0;
    if (ok)
        ok = port.finish(out.bytes, written) && written == sizeof(out.bytes);
    const bool cleaned = port.abort();
    if (!ok || !cleaned)
        out = {};
    return ok && cleaned;
}
bool sameRef(const rules::FactRef &a, const rules::FactRef &b) {
    return bytesEqual(a.id.event, b.id.event) && bytesEqual(a.id.origin, b.id.origin) &&
           bytesEqual(a.id.epoch, b.id.epoch) && a.id.sequence == b.id.sequence &&
           bytesEqual(a.digest, b.digest) && bytesEqual(a.deployment, b.deployment) &&
           bytesEqual(a.binding, b.binding) && bytesEqual(a.run, b.run) &&
           bytesEqual(a.station, b.station) && bytesEqual(a.subject, b.subject) && a.type == b.type;
}
bool sameInput(const rules::Input &a, const rules::Input &b) {
    return rules::sameContext(a.context, b.context) && a.kind == b.kind &&
           a.bootGeneration == b.bootGeneration && a.sequence == b.sequence && a.port == b.port &&
           a.timer == b.timer && a.armGeneration == b.armGeneration && a.type == b.type &&
           a.known == b.known && a.value == b.value && a.fact.subject == b.fact.subject &&
           a.fact.kind == b.fact.kind && sameRef(a.fact.ref, b.fact.ref) &&
           a.overrideAdmission.scope() == b.overrideAdmission.scope() &&
           rules::sameOverrideRecord(a.overrideAdmission.record(), b.overrideAdmission.record());
}
struct ManualFenceScope {
    rules::IManualFinalFence *&slot;
    ManualFenceScope(rules::IManualFinalFence *&destination, rules::IManualFinalFence &value)
        : slot(destination) {
        slot = &value;
    }
    ~ManualFenceScope() {
        slot = nullptr;
    }
};
bool accepted(const EvidenceResult &result, EvidenceScope expected) {
    return result.scope == expected && result.error == EvidenceError::None;
}
struct Operation {
    bool &busy;
    explicit Operation(bool &b) : busy(b) {
        busy = true;
    }
    ~Operation() {
        busy = false;
    }
};
struct Range {
    std::uintptr_t first = 0, last = 0;
};
struct Ranges {
    Range ranges[128];
    std::size_t count = 0;
    bool add(const void *data, std::size_t size) {
        if (!size)
            return true;
        if (!data || count == 128)
            return false;
        const auto first = reinterpret_cast<std::uintptr_t>(data);
        if (size > UINTPTR_MAX - first)
            return false;
        Range next;
        next.first = first;
        next.last = first + size;
        for (std::size_t i = 0; i < count; ++i)
            if (next.first < ranges[i].last && ranges[i].first < next.last)
                return false;
        ranges[count++] = next;
        return true;
    }
    template <typename T> bool pool(const rules::Pool<T> &p) {
        return p.count <= p.capacity && p.capacity <= SIZE_MAX / sizeof(T) &&
               (!p.capacity || (reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0)) &&
               add(p.data, p.capacity * sizeof(T));
    }
    bool program(const rules::Program &p) {
        return pool(p.strings) && pool(p.offsets) && pool(p.nodes) && pool(p.actions) &&
               pool(p.rules) && pool(p.values) && pool(p.timers) && pool(p.ports) &&
               pool(p.subjects) && pool(p.resources) && pool(p.epochs) &&
               pool(p.allowedResources) && pool(p.triggers);
    }
    bool plan(const rules::Plan &p) {
        return pool(p.values) && pool(p.timers) && pool(p.latches) && pool(p.emissions) &&
               pool(p.intents) && pool(p.evidence);
    }
    bool state(const rules::StateStorage &s) {
        return pool(s.values) && pool(s.timers) && pool(s.facts) && pool(s.latches) &&
               pool(s.ledger) && pool(s.solved) && pool(s.evidence);
    }
};
bool sameClockFence(const rules::Fence &before, const rules::Fence &after) {
    return rules::overridePlanningFence(before, after);
}
} // namespace

Owner::Owner(const rules::Program &installed, ProgressJournal &journal,
             rules_bundle::Journal &bundle, rules_bundle::Workspace &bundleWork,
             configuration::IPackageDigest &hashPort, rules_bundle::IIdentitySource &ids,
             IClock &clockPort, ICommitOrdering &ordering, ISignedPlanEvidence &evidence,
             ISignedControlEvidence &controls, IControlFinalFence &controlFence,
             IIntentEligibility &eligibility, IIntentHandler &handler, Workspace &work)
    : installed_(installed), journal_(journal), bundle_(bundle), bundleWork_(bundleWork),
      hash_(hashPort), ids_(ids), clock_(clockPort), ordering_(ordering), evidence_(evidence),
      controls_(controls), controlFence_(controlFence), intentEligibility_(eligibility),
      handler_(handler), work_(work) {}

Result Owner::fail(Error error, bool fault) {
    result_.error = error;
    if (fault) {
        phase_ = Phase::Fault;
        recovered_ = false;
        if (recoveryPinned_) {
            ordering_.abortRecovery(recoveryOrdering_);
            recoveryPinned_ = false;
        }
    }
    result_.phase = phase_;
    return result_;
}
bool Owner::workspace(const void *output, std::size_t outputBytes, const void *descriptor,
                      std::size_t descriptorBytes, const void *buffer,
                      std::size_t bufferBytes) const {
    Ranges r;
    if (!r.add(this, sizeof(*this)) || !r.add(&work_, sizeof(work_)) ||
        !r.add(&installed_, sizeof(installed_)) || !r.add(&bundleWork_, sizeof(bundleWork_)) ||
        !r.program(installed_) || !r.program(work_.historicalProgram))
        return false;
    for (const auto &b : work_.banks)
        if (!r.state(b.storage))
            return false;
    for (const auto *app : {&work_.prepared, &work_.decoded})
        if (!r.plan(app->plan) || !r.pool(app->proofs) || !r.pool(app->outbox))
            return false;
    return r.plan(work_.computed) && r.pool(work_.planner.catalog) &&
           r.pool(work_.planner.proofs) && r.pool(work_.planner.proofRefs) &&
           r.pool(work_.preparedEvidence.bytes) && r.pool(work_.decodedStorage.bytes) &&
           r.pool(work_.applicationBytes) && r.pool(work_.decodedApplication) &&
           r.pool(work_.comparison) && r.pool(work_.history) && r.pool(work_.physicalBytes) &&
           r.pool(work_.catalogs) && r.pool(work_.applications) && r.pool(work_.controls) &&
           r.pool(work_.intents) && r.pool(work_.envelope.intents) && r.pool(bundleWork_.staged) &&
           r.pool(bundleWork_.recovered) && r.pool(bundleWork_.records) &&
           r.pool(bundleWork_.bundles) && r.add(output, outputBytes) &&
           r.add(descriptor, descriptorBytes) && r.add(buffer, bufferBytes);
}
rules::Capacities Owner::capacities(const StateBank &bank) const {
    rules::Capacities out;
    out.receipts = bank.storage.ledger.capacity;
    out.facts = bank.storage.facts.capacity;
    out.latches = bank.storage.latches.capacity;
    out.outbox = work_.prepared.outbox.capacity;
    out.intents = work_.intents.capacity - work_.intents.count;
    return out;
}
bool Owner::remember(const std::uint8_t *p, std::size_t n, std::uint32_t &at) {
    auto &bytes = work_.history;
    if (bytes.count > bytes.capacity || n > bytes.capacity - bytes.count ||
        bytes.count > UINT32_MAX || n > UINT32_MAX - bytes.count || (n && !p))
        return false;
    at = static_cast<std::uint32_t>(bytes.count);
    if (n)
        std::memcpy(bytes.data + bytes.count, p, n);
    bytes.count += n;
    return true;
}
bool Owner::loadCatalog(const rules::Digest &wanted) {
    for (std::size_t i = 0; i < work_.catalogs.count; ++i) {
        const auto &entry = work_.catalogs.data[i];
        if (!bytesEqual(wanted, entry.digest))
            continue;
        if (entry.first > work_.history.count || entry.count > work_.history.count - entry.first)
            return false;
        rules::Span<std::uint8_t> source;
        source.data = work_.history.data + entry.first;
        source.count = entry.count;
        return codec::decodeCatalog(source, work_.historicalProgram).scope ==
               codec::Scope::DecodedUnauthenticated;
    }
    return false;
}
bool Owner::sameApplication(const codec::Application &original, rules::Plan &computed) {
    codec::Application replay = original;
    replay.plan = computed;
    replay.plan.ready = true; // Only a local representation after pure historical replay.
    work_.comparison.count = 0;
    if (codec::encodeApplication(replay, work_.comparison).scope !=
        codec::Scope::EncodedUnpublished)
        return false;
    return work_.comparison.count == work_.envelope.application.count &&
           !std::memcmp(work_.comparison.data, work_.envelope.application.data,
                        work_.comparison.count);
}
rules::ReleaseDisposition Owner::release(const codec::Application &app) const {
    if (app.plan.receipt.input.kind != rules::InputKind::Override)
        return rules::ReleaseDisposition::NoRelease;
    bool latch = false, ownT2 = false;
    for (std::size_t i = 0; i < app.plan.latches.count; ++i)
        latch |= bytesEqual(app.plan.latches.data[i].rule, app.overrideRecord.rule);
    for (std::size_t i = 0; i < app.plan.emissions.count; ++i) {
        const auto &e = app.plan.emissions.data[i];
        ownT2 |=
            e.type == rules::FactType::LocalOverride && bytesEqual(e.rule, app.overrideRecord.rule);
    }
    return latch && ownT2 ? rules::ReleaseDisposition::Released
                          : rules::ReleaseDisposition::NoRelease;
}

Result Owner::recover() {
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    pendingDefinitelyNotCommitted_ = false;
    recovered_ = false;
    phase_ = Phase::Recovering;
    if (!workspace() || !installed_.ready)
        return fail(Error::Argument, true);
    OrderingSnapshot snapshot;
    const auto inventory = ordering_.beginRecovery(snapshot);
    if ((inventory != OrderingInventory::Empty && inventory != OrderingInventory::Held) ||
        snapshot.inventory != inventory || !nonzero(snapshot.boot) || !snapshot.generation)
        return fail(Error::Recovery, true);
    recoveryOrdering_ = snapshot;
    recoveryPinned_ = true;
    OrderingLease empty;
    if ((inventory == OrderingInventory::Empty && !sameOrderingLease(snapshot.lease, empty)) ||
        (inventory == OrderingInventory::Held &&
         (!nonzero(snapshot.lease.boot) || !snapshot.lease.generation ||
          !bytesEqual(snapshot.lease.boot, snapshot.boot) ||
          snapshot.lease.generation >= snapshot.generation)) ||
        (ordered_ &&
         (inventory != OrderingInventory::Held || !sameOrderingLease(snapshot.lease, lease_))))
        return fail(Error::Recovery, true);
    const auto physical = journal_.snapshot();
    if (!physical.ready || physical.poisoned || physical.operating ||
        std::memcmp(physical.origin.event.data(), installed_.context.event.bytes, 16) ||
        std::memcmp(physical.origin.box.data(), installed_.context.box.bytes, 16) ||
        std::memcmp(physical.origin.epoch.data(), installed_.context.sourceEpoch.bytes, 16))
        return fail(Error::Journal, true);
    if (codec::encodeCatalog(installed_, work_.comparison).scope !=
        codec::Scope::EncodedUnpublished)
        return fail(Error::Codec, true);
    rules::Span<std::uint8_t> encodedCatalog;
    encodedCatalog.data = work_.comparison.data;
    encodedCatalog.count = work_.comparison.count;
    if (codec::decodeCatalog(encodedCatalog, work_.historicalProgram).scope !=
        codec::Scope::DecodedUnauthenticated)
        return fail(Error::Codec, true); // Full shared semantics, never ready alone.
    if (!hash(hash_, work_.comparison.data, work_.comparison.count, catalog_))
        return fail(Error::Crypto, true);
    if (!clock_.read(recoveredAnchor_) || !nonzero(recoveredAnchor_.boot))
        return fail(Error::Clock, true);
    phase_ = Phase::Recovering;
    pendingDefinitelyNotCommitted_ = false;
    recovered_ = false;
    rebuilding_ = true;
    result_.journal = bundle_.recover(*this);
    rebuilding_ = false;
    if (result_.journal.outcome != rules_bundle::Outcome::Recovered)
        return fail(Error::Recovery, true);
    // Actual single-worker publisher: only releases the already prepared pin.
    // No queue/IO/authority decision is permitted in this callback.
    ordering_.publishRecovery(recoveryOrdering_);
    recoveryPinned_ = false;
    ordered_ = false;
    lease_ = {};
    result_.phase = phase_;
    return result_;
}
bool Owner::begin(const ProgressSnapshot &) {
    if (!rebuilding_)
        return false;
    work_.history.count = work_.catalogs.count = work_.applications.count = work_.controls.count =
        0;
    work_.physicalBytes.count = work_.intents.count = 0;
    rebuilt_ = {};
    activeCatalog_ = {};
    activated_ = false;
    replayBank_ = liveBank_ == 0 ? 1 : 0;
    reservedIntents_ = 0;
    return true;
}
bool Owner::catalog(const rules_bundle::Key &key, const std::uint8_t *bytes, std::size_t count) {
    if (!rebuilding_ || key.kind != rules_bundle::Kind::Catalog || count != key.bytes ||
        key.catalog != key.blob || work_.catalogs.count == work_.catalogs.capacity)
        return false;
    rules::Digest actual;
    if (!hash(hash_, bytes, count, actual) || std::memcmp(actual.bytes, key.blob.data(), 32))
        return false;
    rules::Span<std::uint8_t> source;
    source.data = bytes;
    source.count = count;
    if (codec::decodeCatalog(source, work_.historicalProgram).scope !=
            codec::Scope::DecodedUnauthenticated ||
        std::memcmp(work_.historicalProgram.context.deployment.bytes, key.deployment.data(), 32))
        return false;
    for (std::size_t i = 0; i < work_.catalogs.count; ++i) {
        const auto &old = work_.catalogs.data[i];
        if (!bytesEqual(old.digest, actual))
            continue;
        return old.count == count && !std::memcmp(work_.history.data + old.first, bytes, count);
    }
    CatalogEntry entry;
    entry.digest = actual;
    std::memcpy(entry.deployment.bytes, key.deployment.data(), 32);
    entry.count = static_cast<std::uint32_t>(count);
    if (!remember(bytes, count, entry.first))
        return false;
    work_.catalogs.data[work_.catalogs.count++] = entry;
    return true;
}

bool Owner::application(const rules_bundle::Key &key, const ProgressRecord &final,
                        const std::uint8_t *bytes, std::size_t count) {
    if (!rebuilding_ || !activated_ || key.kind != rules_bundle::Kind::Application ||
        count != key.bytes || work_.applications.count == work_.applications.capacity)
        return false;
    rules::Digest actual;
    if (!hash(hash_, bytes, count, actual) || std::memcmp(actual.bytes, key.blob.data(), 32))
        return false;
    if (!decodeTransaction(bytes, count, work_.envelope, work_.decodedApplication).valid)
        return false;
    const auto &envelope = work_.envelope;
    if (std::memcmp(envelope.operation.bytes, key.operation.data(), 16) ||
        std::memcmp(envelope.request.bytes, key.request.data(), 32) ||
        std::memcmp(envelope.catalog.bytes, key.catalog.data(), 32) ||
        !bytesEqual(envelope.catalog, activeCatalog_) || !loadCatalog(envelope.catalog))
        return false;
    auto &app = work_.decoded;
    if (codec::decodeApplication(envelope.application, app, work_.decodedStorage).scope !=
            codec::Scope::DecodedUnauthenticated ||
        !bytesEqual(app.operation, envelope.operation) ||
        !bytesEqual(app.request, envelope.request) || !bytesEqual(app.catalog, envelope.catalog) ||
        !rules::sameContext(app.plan.context, rebuilt_.context) ||
        !rules::sameContext(app.plan.context, work_.historicalProgram.context) ||
        std::memcmp(app.plan.context.deployment.bytes, key.deployment.data(), 32) ||
        final.expectedLastIssued != rebuilt_.fence.lastIssued ||
        final.profileRevision != rebuilt_.fence.profile ||
        final.trustRevision != rebuilt_.fence.trust ||
        final.issueCount != app.plan.emissions.count ||
        envelope.intents.count != app.plan.intents.count ||
        envelope.intents.count > work_.intents.capacity - work_.intents.count)
        return false;
    ClockWitness original;
    if (!accepted(evidence_.historical(work_.historicalProgram, rebuilt_, app, final, original),
                  EvidenceScope::VerifiedHistorical) ||
        !bytesEqual(original.boot, replayAnchor_.boot) ||
        original.monotonicBefore != replayAnchor_.monotonic ||
        original.monotonicAfter < original.monotonicBefore ||
        original.activeBefore != rebuilt_.fence.activeTime ||
        original.activeAfter < original.activeBefore ||
        original.activeAfter != app.plan.expected.activeTime ||
        !sameClockFence(rebuilt_.fence, app.plan.expected))
        return false;
    const auto elapsed = original.monotonicAfter - original.monotonicBefore;
    if (rebuilt_.mode == rules::Mode::Running
            ? (elapsed > 1000 || original.activeAfter - original.activeBefore != elapsed)
            : original.activeAfter != original.activeBefore)
        return false;
    planning_ = rebuilt_;
    planning_.fence.activeTime = original.activeAfter;
    // Alternate only the two recovery banks; the previously published live bank
    // is never touched before the single recovery consumer commit.
    std::uint8_t nextBank = 0;
    while (nextBank == replayBank_ || nextBank == liveBank_)
        ++nextBank;
    auto &destination = work_.banks[nextBank];
    const auto replay = planner_.replayHistorical(
        work_.historicalProgram, planning_, app.plan.receipt.input,
        app.plan.receipt.input.kind == rules::InputKind::Override ? &app.overrideRecord : nullptr,
        capacities(destination), work_.computed, work_.planner, destination.storage,
        destination.state);
    if (replay.scope != rules::ReplayScope::ReplayedInactive ||
        !sameApplication(app, work_.computed) ||
        (final.issueCount && destination.state.fence.lastIssued != final.lastIssued))
        return false;
    for (std::size_t i = 0; i < envelope.intents.count; ++i) {
        const auto &reservation = envelope.intents.data[i];
        for (std::size_t j = 0; j < work_.intents.count; ++j) {
            const auto &old = work_.intents.data[j].reservation;
            if (bytesEqual(reservation.claim, old.claim) ||
                bytesEqual(reservation.claim, old.result) ||
                bytesEqual(reservation.result, old.claim) ||
                bytesEqual(reservation.result, old.result))
                return false;
        }
        for (std::size_t j = 0; j < bundleWork_.records.count; ++j) {
            const auto &r = bundleWork_.records.data[j].record;
            if ((!std::memcmp(reservation.claim.bytes, r.operationId.data(), 16) &&
                 (r.index <= final.index || r.kind != ProgressKind::EffectClaim)) ||
                (!std::memcmp(reservation.result.bytes, r.operationId.data(), 16) &&
                 (r.index <= final.index || r.kind != ProgressKind::EffectResult)))
                return false;
        }
    }
    ApplicationEntry entry;
    entry.operation = app.operation;
    entry.request = app.request;
    entry.catalog = app.catalog;
    entry.blob = actual;
    entry.final = final;
    entry.before = rebuilt_.fence;
    entry.release = release(app);
    entry.count = static_cast<std::uint32_t>(count);
    if (!remember(bytes, count, entry.first))
        return false;
    for (std::size_t i = 0; i < envelope.intents.count; ++i) {
        IntentEntry intent;
        intent.application = app.operation;
        intent.catalog = app.catalog;
        intent.reservation = envelope.intents.data[i];
        work_.intents.data[work_.intents.count++] = intent;
        reservedIntents_ += 2;
    }
    work_.applications.data[work_.applications.count++] = entry;
    rebuilt_ = destination.state;
    replayBank_ = nextBank;
    replayAnchor_.monotonic = original.monotonicAfter;
    return true;
}

bool Owner::auxiliary(const ProgressRecord &record, const std::uint8_t *bytes, std::size_t count) {
    if (!rebuilding_ || count != record.payloadBytes)
        return false;
    rules::Digest actual;
    if (!hash(hash_, bytes, count, actual) ||
        std::memcmp(actual.bytes, record.requestDigest.data(), 32))
        return false;
    if (record.kind == ProgressKind::Control) {
        Control control;
        if (!decodeControl(bytes, count, control, work_.preparedEvidence.bytes).valid ||
            !loadCatalog(control.catalog) ||
            !rules::sameContext(control.context, work_.historicalProgram.context) ||
            record.issueCount || record.firstIssued || record.lastIssued ||
            record.expectedLastIssued != control.expected.lastIssued ||
            record.profileRevision != control.expected.profile ||
            record.trustRevision != control.expected.trust ||
            std::memcmp(record.operationId.data(), control.operation.bytes, 16) ||
            !rules::sameFence(control.expected, rebuilt_.fence))
            return false;
        if (control.kind == ControlKind::Activation) {
            if (activated_ && (!rules::sameContext(rebuilt_.context, control.context) ||
                               !bytesEqual(control.catalog, activeCatalog_)))
                return false; // No implicit rule/profile migration or state reset.
            if (!accepted(
                    controls_.historicalControl(work_.historicalProgram, rebuilt_, control, record),
                    EvidenceScope::VerifiedHistorical))
                return false;
            if (!activated_) {
                auto &bank = work_.banks[replayBank_];
                const auto initialized = rules::Planner::initialize(
                    work_.historicalProgram, control.targetFence, bank.storage, bank.state);
                if (initialized.decision != rules::Decision::Ready)
                    return false;
                rebuilt_ = bank.state;
                activated_ = true;
            } else {
                if (rebuilt_.mode != rules::Mode::Stopped)
                    return false;
                rebuilt_.fence = control.targetFence;
            }
            activeCatalog_ = control.catalog;
        } else {
            if (!activated_ || !bytesEqual(control.catalog, activeCatalog_) ||
                !rules::sameContext(control.context, rebuilt_.context) ||
                !accepted(
                    controls_.historicalControl(work_.historicalProgram, rebuilt_, control, record),
                    EvidenceScope::VerifiedHistorical))
                return false;
            // Only the actual original lifecycle proof can justify this
            // transition. Control.before by itself is never a restart proof.
            const bool restart = control.kind == ControlKind::RestoreStopped &&
                                 control.before == rules::Mode::RecoveryRequired;
            if ((!restart && (control.before != rebuilt_.mode ||
                              !bytesEqual(control.clock.boot, replayAnchor_.boot) ||
                              control.clock.monotonicBefore != replayAnchor_.monotonic)) ||
                control.clock.activeBefore != rebuilt_.fence.activeTime)
                return false;
            if (!restart && rebuilt_.mode == rules::Mode::Running &&
                control.clock.monotonicAfter - control.clock.monotonicBefore > 1000)
                return false;
            rebuilt_.fence = control.targetFence;
        }
        if (work_.controls.count == work_.controls.capacity)
            return false;
        ControlEntry entry;
        entry.operation = control.operation;
        entry.kind = control.kind;
        entry.request = control.request;
        entry.catalog = control.catalog;
        entry.before = control.expected;
        entry.raw = actual;
        entry.record = record;
        entry.count = static_cast<std::uint32_t>(count);
        if (!remember(bytes, count, entry.first))
            return false;
        work_.controls.data[work_.controls.count++] = entry;
        if (control.kind == ControlKind::RestoreStopped)
            for (std::size_t i = 0; i < work_.intents.count; ++i) {
                auto &intent = work_.intents.data[i];
                if (intent.stage == IntentStage::Attempted || intent.settlementRequired) {
                    intent.stage = IntentStage::Unknown;
                    intent.settlementRequired = true;
                    intent.restoredBy = control.operation;
                }
            }
        rebuilt_.mode = control.after;
        replayAnchor_.boot = control.clock.boot;
        replayAnchor_.monotonic = control.clock.monotonicAfter;
        return true;
    }
    if ((record.kind != ProgressKind::EffectClaim && record.kind != ProgressKind::EffectResult) ||
        !activated_)
        return false; // No silent adoption of another Received/control protocol.
    Effect effect;
    if (!decodeEffect(bytes, count, effect).valid ||
        record.profileRevision != effect.expected.profile ||
        record.trustRevision != effect.expected.trust)
        return false;
    for (std::size_t i = 0; i < work_.intents.count; ++i) {
        auto &intent = work_.intents.data[i];
        if (!bytesEqual(intent.application, effect.application) ||
            intent.reservation.index != effect.intentIndex)
            continue;
        if (!bytesEqual(intent.catalog, effect.catalog) ||
            !validateEffectRecord(effect, record, intent.reservation).valid)
            return false;
        if (effect.phase == EffectPhase::Claim) {
            if (intent.stage != IntentStage::Pending || rebuilt_.mode != rules::Mode::Running ||
                !bytesEqual(effect.catalog, activeCatalog_) ||
                !rules::sameFence(effect.expected, rebuilt_.fence) ||
                !bytesEqual(effect.ownerBoot, replayAnchor_.boot))
                return false;
            intent.originalClaimFence = effect.expected;
            intent.originalClaimBoot = effect.ownerBoot;
            intent.stage = IntentStage::Attempted;
        } else {
            // A late terminal result describes the ORIGINAL handoff. A later
            // control/restart cannot rebind it to today's fence or boot, and
            // this acknowledgment is never a new execution authorization.
            const bool settlement = intent.stage == IntentStage::Unknown &&
                                    intent.settlementRequired && nonzero(intent.restoredBy);
            if ((intent.stage != IntentStage::Attempted && !settlement) || intent.resultCommitted ||
                (settlement && effect.result != HandlerResult::Unknown) ||
                !rules::sameFence(effect.expected, intent.originalClaimFence) ||
                !bytesEqual(effect.ownerBoot, intent.originalClaimBoot))
                return false;
            intent.resultBefore = rebuilt_.fence;
            intent.resultCommitted = true;
            intent.settlementRequired = false;
            switch (effect.result) {
            case HandlerResult::Returned:
                intent.stage = IntentStage::Returned;
                break;
            case HandlerResult::Failed:
                intent.stage = IntentStage::Failed;
                break;
            case HandlerResult::Unknown:
                intent.stage = IntentStage::Unknown;
                break;
            case HandlerResult::None:
                return false;
            }
        }
        if (!reservedIntents_)
            return false;
        --reservedIntents_;
        return true;
    }
    return false;
}
bool Owner::prepare() {
    if (!rebuilding_ || !workspace())
        return false;
    if (activated_ && (!bytesEqual(activeCatalog_, catalog_) ||
                       !rules::sameContext(rebuilt_.context, installed_.context)))
        return false;
    const auto physical = journal_.snapshot();
    if (!physical.ready || physical.poisoned || physical.operating ||
        physical.lastIssued != rebuilt_.fence.lastIssued ||
        reservedIntents_ > ProgressLimits::Records - physical.records || !physicalCapacity(0))
        return false;
    // No callbacks or IO are permitted in the following commit publication.
    ClockObservation finalClock;
    if (!clock_.read(finalClock) || !bytesEqual(finalClock.boot, recoveredAnchor_.boot) ||
        finalClock.monotonic < recoveredAnchor_.monotonic)
        return false;
    recoveredAnchor_ = finalClock;
    OrderingSnapshot finalOrdering;
    if (!recoveryPinned_ || ordering_.readRecovery(finalOrdering) != recoveryOrdering_.inventory ||
        !sameOrderingSnapshot(finalOrdering, recoveryOrdering_))
        return false;
    ProgressOutcome resolution;
    if (!recoveryResolution(resolution) ||
        ordering_.prepareRecovery(recoveryOrdering_, resolution) != OrderingResolution::Resolved)
        return false;
    return true;
}
void Owner::commit() {
    // Only the already prepared inactive snapshot is made visible here.
    live_ = rebuilt_;
    if (activated_)
        live_.mode = rules::Mode::RecoveryRequired;
    for (std::size_t i = 0; i < work_.intents.count; ++i) {
        auto &intent = work_.intents.data[i];
        if (intent.stage == IntentStage::Attempted) {
            intent.stage = IntentStage::Unknown;
            intent.settlementRequired = true;
        }
        // This fresh recovered owner still needs its OWN explicitly accepted
        // checkpoint-rest decision; an old lifecycle command is not revived.
        if (intent.settlementRequired)
            intent.restoredBy = {};
    }
    liveBank_ = replayBank_;
    anchor_ = recoveredAnchor_;
    recovered_ = true;
    phase_ = activated_ ? Phase::Stopped : Phase::Uninitialized;
}
void Owner::abort() {
    if (recoveryPinned_) {
        ordering_.abortRecovery(recoveryOrdering_);
        recoveryPinned_ = false;
    }
    // The published live state remains untouched. Failed replay metadata is
    // inaccessible while Fault/Recovering and is never an older-prefix result.
    rebuilt_ = {};
    activated_ = false;
    recovered_ = false;
    pendingDefinitelyNotCommitted_ = false;
    phase_ = Phase::Fault;
}
bool Owner::uniquePhase(const rules::Identity &id) const {
    if (!nonzero(id))
        return false;
    for (std::size_t i = 0; i < bundleWork_.records.count; ++i)
        if (!std::memcmp(id.bytes, bundleWork_.records.data[i].record.operationId.data(), 16))
            return false;
    for (std::size_t i = 0; i < bundleWork_.bundles.count; ++i)
        if (!std::memcmp(id.bytes, bundleWork_.bundles.data[i].key.operation.data(), 16))
            return false;
    for (std::size_t i = 0; i < work_.intents.count; ++i) {
        const auto &r = work_.intents.data[i].reservation;
        if (bytesEqual(id, r.claim) || bytesEqual(id, r.result))
            return false;
    }
    return true;
}

bool Owner::clock(ClockWitness &witness, rules::State &planning) {
    clockError_ = Error::None;
    ClockObservation now;
    if (!clock_.read(now) || !nonzero(now.boot) || !bytesEqual(now.boot, anchor_.boot) ||
        now.monotonic < anchor_.monotonic) {
        clockError_ = Error::Clock;
        return false;
    }
    witness = {};
    witness.boot = now.boot;
    witness.monotonicBefore = anchor_.monotonic;
    witness.monotonicAfter = now.monotonic;
    witness.activeBefore = witness.activeAfter = live_.fence.activeTime;
    planning = live_;
    if (live_.mode == rules::Mode::Running) {
        const auto elapsed = now.monotonic - anchor_.monotonic;
        if (elapsed > 1000 || elapsed > UINT64_MAX - witness.activeBefore) {
            clockError_ = elapsed > 1000 ? Error::CheckpointOverrun : Error::Range;
            return false;
        }
        witness.activeAfter += elapsed;
        planning.fence.activeTime = witness.activeAfter;
    }
    return true;
}
Result Owner::plan(const rules::Input &originalInput, const rules::Identity &operation,
                   const rules::Digest &request) {
    const rules::Input input = originalInput;
    const rules::Identity copiedOperation = operation;
    const rules::Digest copiedRequest = request;
    if (!recovered_ || !activated_ || phase_ != Phase::Ready || !workspace() ||
        !rules::sameContext(input.context, live_.context) || !nonzero(copiedOperation))
        return fail(Error::Context);
    if (!uniquePhase(copiedOperation))
        return fail(Error::Conflict);
    if (work_.applications.count == work_.applications.capacity)
        return fail(Error::Capacity);
    if (!clock(pendingClock_, planning_))
        return fail(clockError_, true);
    if (input.kind == rules::InputKind::Override &&
        (input.overrideAdmission.scope() != rules::OverrideAdmissionScope::Live ||
         !bytesEqual(input.overrideAdmission.record().ownerBoot, pendingClock_.boot) ||
         !bytesEqual(input.overrideAdmission.record().program, catalog_)))
        return fail(Error::Admission);
    phase_ = Phase::Preparing;
    pendingDefinitelyNotCommitted_ = false;
    auto &app = work_.prepared;
    app.operation = copiedOperation;
    app.request = copiedRequest;
    app.catalog = catalog_;
    app.overrideRecord = input.kind == rules::InputKind::Override ? input.overrideAdmission.record()
                                                                  : rules::OverrideRecord{};
    app.proofs.count = app.outbox.count = 0;
    app.authorization = {};
    app.decoded = false;
    auto capacity = capacities(work_.banks[liveBank_]);
    const auto planned =
        planner_.plan(installed_, planning_, input, capacity, app.plan, work_.planner);
    result_.planner = planned;
    if (planned.decision != rules::Decision::Ready) {
        phase_ = Phase::Ready;
        return fail(planned.decision == rules::Decision::Deferred ? Error::Capacity
                                                                  : Error::Admission);
    }
    if (!accepted(evidence_.freeze(installed_, live_, pendingClock_, app, work_.preparedEvidence),
                  EvidenceScope::FrozenOriginal)) {
        phase_ = Phase::Ready;
        return fail(Error::Admission);
    }
    // Re-evaluate the frozen original input after every signing/policy callback.
    // An evidence adapter may fill ONLY its proof/outbox/authorization outputs;
    // the exact resulting plan is checked by canonical full-byte comparison.
    if (codec::encodeApplication(app, work_.applicationBytes).scope !=
            codec::Scope::EncodedUnpublished ||
        planner_.plan(installed_, planning_, input, capacity, work_.computed, work_.planner)
                .decision != rules::Decision::Ready) {
        phase_ = Phase::Ready;
        return fail(Error::Codec);
    }
    codec::Application comparison = app;
    comparison.plan = work_.computed;
    if (codec::encodeApplication(comparison, work_.comparison).scope !=
            codec::Scope::EncodedUnpublished ||
        work_.comparison.count != work_.applicationBytes.count ||
        std::memcmp(work_.comparison.data, work_.applicationBytes.data, work_.comparison.count)) {
        phase_ = Phase::Ready;
        return fail(Error::Consumer);
    }
    work_.envelope.operation = copiedOperation;
    work_.envelope.request = copiedRequest;
    work_.envelope.catalog = catalog_;
    work_.envelope.application.data = work_.applicationBytes.data;
    work_.envelope.application.count = work_.applicationBytes.count;
    work_.envelope.intents.count = 0;
    if (app.plan.intents.count > work_.envelope.intents.capacity ||
        app.plan.intents.count > work_.intents.capacity - work_.intents.count ||
        !physicalCapacity(app.plan.intents.count * 2)) {
        phase_ = Phase::Ready;
        return fail(Error::Capacity);
    }
    for (std::size_t i = 0; i < app.plan.intents.count; ++i) {
        IntentReservation reservation;
        reservation.index = static_cast<std::uint16_t>(i);
        rules_bundle::Identity claim{}, result{};
        const bool made = ids_.make(claim) && ids_.make(result);
        std::memcpy(reservation.claim.bytes, claim.data(), 16);
        std::memcpy(reservation.result.bytes, result.data(), 16);
        if (!made || !uniquePhase(reservation.claim) || !uniquePhase(reservation.result) ||
            bytesEqual(reservation.claim, reservation.result) ||
            bytesEqual(reservation.claim, copiedOperation) ||
            bytesEqual(reservation.result, copiedOperation)) {
            phase_ = Phase::Ready;
            return fail(Error::Conflict);
        }
        for (std::size_t j = 0; j < work_.envelope.intents.count; ++j) {
            const auto &old = work_.envelope.intents.data[j];
            if (bytesEqual(reservation.claim, old.claim) ||
                bytesEqual(reservation.claim, old.result) ||
                bytesEqual(reservation.result, old.claim) ||
                bytesEqual(reservation.result, old.result)) {
                phase_ = Phase::Ready;
                return fail(Error::Conflict);
            }
        }
        work_.envelope.intents.data[work_.envelope.intents.count++] = reservation;
    }
    if (!encodeTransaction(work_.envelope, bundleWork_.staged).valid) {
        phase_ = Phase::Ready;
        return fail(Error::Capacity);
    }
    rules::Digest blob;
    if (!hash(hash_, bundleWork_.staged.data, bundleWork_.staged.count, blob)) {
        phase_ = Phase::Ready;
        return fail(Error::Crypto);
    }
    pendingBank_ = liveBank_ == 0 ? 1 : 0;
    const auto ram = rules::Planner::commitRam(installed_, planning_, app.plan,
                                               work_.banks[pendingBank_].storage, pendingState_);
    if (ram.decision != rules::Decision::Ready) {
        phase_ = Phase::Ready;
        return fail(Error::Capacity);
    }
    pendingApplication_ = {};
    pendingApplication_.operation = copiedOperation;
    pendingApplication_.request = copiedRequest;
    pendingApplication_.catalog = catalog_;
    pendingApplication_.blob = blob;
    pendingApplication_.release = release(app);
    pendingApplication_.before = live_.fence;
    pendingApplication_.count = static_cast<std::uint32_t>(bundleWork_.staged.count);
    if (!remember(bundleWork_.staged.data, bundleWork_.staged.count, pendingApplication_.first)) {
        phase_ = Phase::Ready;
        return fail(Error::Capacity);
    }
    // Complete future evidence/intent record reservation precedes the first
    // inert chunk. The sole metadata owner serializes all logical changes.
    CommitBinding binding;
    binding.kind = CommitKind::Application;
    binding.operation = copiedOperation;
    binding.context = live_.context;
    binding.expected = live_.fence;
    binding.request = copiedRequest;
    binding.catalog = catalog_;
    binding.commitment = blob;
    if (!acquire(binding)) {
        work_.history.count = pendingApplication_.first;
        if (phase_ == Phase::Fault)
            return result_;
        phase_ = Phase::Ready;
        return fail(Error::Stale);
    }
    ordered_ = true;
    heldOperation_ = copiedOperation;
    baseApplications_ = work_.applications.count;
    baseIntents_ = work_.intents.count;
    baseControls_ = work_.controls.count;
    preparedHistory_ = work_.history.count;
    pendingKey_ = {};
    pendingKey_.kind = rules_bundle::Kind::Application;
    std::memcpy(pendingKey_.operation.data(), copiedOperation.bytes, 16);
    std::memcpy(pendingKey_.request.data(), copiedRequest.bytes, 32);
    std::memcpy(pendingKey_.catalog.data(), catalog_.bytes, 32);
    std::memcpy(pendingKey_.deployment.data(), live_.context.deployment.bytes, 32);
    std::memcpy(pendingKey_.blob.data(), blob.bytes, 32);
    pendingKey_.bytes = static_cast<std::uint32_t>(bundleWork_.staged.count);
    pendingKey_.chunks = static_cast<std::uint8_t>((pendingKey_.bytes + 2047) / 2048);
    rules_bundle::HeaderMeta meta;
    meta.profileRevision = live_.fence.profile;
    meta.trustRevision = live_.fence.trust;
    result_.journal =
        bundle_.begin(pendingKey_, meta, static_cast<std::uint8_t>(app.plan.emissions.count),
                      reservedIntents_ + app.plan.intents.count * 2);
    if (result_.journal.outcome != rules_bundle::Outcome::Prepared) {
        work_.history.count = pendingApplication_.first;
        if (!releaseOrdering(ProgressOutcome::Denied))
            return fail(Error::Recovery, true);
        ordered_ = false;
        phase_ = Phase::Ready;
        return fail(Error::Journal);
    }
    // Check every freshly generated chunk identity against reserved future
    // phases before ANY data write. There is no unbounded collision reroll.
    bool collision = false;
    for (std::size_t i = 0; i < work_.envelope.intents.count; ++i) {
        const auto &r = work_.envelope.intents.data[i];
        for (std::size_t j = 0; j < pendingKey_.chunks; ++j) {
            const auto &id = bundleWork_.chunks[j].operation;
            collision |= !std::memcmp(r.claim.bytes, id.data(), 16) ||
                         !std::memcmp(r.result.bytes, id.data(), 16);
        }
    }
    if (collision) {
        const auto abandoned = bundle_.cancelPrepared();
        if (abandoned.outcome != rules_bundle::Outcome::Abandoned) {
            phase_ = Phase::Unknown;
            return fail(Error::Recovery);
        }
        if (!releaseOrdering(ProgressOutcome::Denied))
            return fail(Error::Recovery, true);
        ordered_ = false;
        work_.history.count = pendingApplication_.first;
        phase_ = Phase::Ready;
        return fail(Error::Conflict);
    }
    phase_ = Phase::WritingApplication;
    result_.phase = phase_;
    return result_;
}

bool Owner::finalClock(const rules::OverrideRecord *original) {
    ClockObservation now;
    if (!clock_.read(now) || !bytesEqual(now.boot, pendingClock_.boot) ||
        now.monotonic < pendingClock_.monotonicAfter) {
        clockError_ = Error::Clock;
        return false;
    }
    if (live_.mode == rules::Mode::Running) {
        const auto elapsed = now.monotonic - pendingClock_.monotonicBefore;
        if (elapsed > 1000 || elapsed > UINT64_MAX - live_.fence.activeTime) {
            clockError_ = elapsed > 1000 ? Error::CheckpointOverrun : Error::Range;
            return false;
        }
    }
    return !original || now.monotonic <= original->expiresAt;
}

bool Owner::allow(const rules_bundle::Key &key, rules_bundle::Phase phase,
                  const ProgressRecord &record) {
    if (!committing_ || !operating_ || !ordered_ || !workspace() ||
        work_.applications.count != baseApplications_ || work_.intents.count != baseIntents_ ||
        work_.controls.count != baseControls_ || work_.history.count != preparedHistory_ ||
        key.kind != pendingKey_.kind || key.operation != pendingKey_.operation ||
        key.request != pendingKey_.request || key.deployment != pendingKey_.deployment ||
        key.catalog != pendingKey_.catalog || key.blob != pendingKey_.blob ||
        key.bytes != pendingKey_.bytes || key.chunks != pendingKey_.chunks ||
        record.expectedLastIssued != live_.fence.lastIssued ||
        record.profileRevision != live_.fence.profile || record.trustRevision != live_.fence.trust)
        return false;
    const auto physical = journal_.snapshot();
    // ProgressJournal calls its fence while its own operating bit is set.
    if (!physical.ready || physical.poisoned || physical.revision != record.expectedRevision ||
        physical.lastIssued != record.expectedLastIssued)
        return false;
    rules::Digest fresh;
    if (codec::encodeCatalog(installed_, work_.comparison).scope !=
            codec::Scope::EncodedUnpublished ||
        !hash(hash_, work_.comparison.data, work_.comparison.count, fresh) ||
        !bytesEqual(fresh, catalog_))
        return false;
    ClockObservation now;
    if (!clock_.read(now) || !bytesEqual(now.boot, pendingClock_.boot) ||
        now.monotonic < pendingClock_.monotonicAfter ||
        (live_.mode == rules::Mode::Running &&
         now.monotonic - pendingClock_.monotonicBefore > 1000))
        return false;
    if (phase == rules_bundle::Phase::Final && key.kind == rules_bundle::Kind::Application) {
        if (record.kind != ProgressKind::Application ||
            std::memcmp(record.operationId.data(), work_.prepared.operation.bytes, 16) ||
            record.issueCount != work_.prepared.plan.emissions.count)
            return false;
        if (work_.prepared.plan.receipt.input.kind == rules::InputKind::Override &&
            (!manualFence_ || manualFence_->check(work_.prepared.overrideRecord) !=
                                  rules::ManualAuthorityResult::Current))
            return false;
    }
    if (key.kind == rules_bundle::Kind::Catalog &&
        controlFence_.preflight(installed_, live_, pendingControl_) != ControlEligibility::Current)
        return false;
    // The last external authorization callback may block. Observe the same
    // actual boot/clock without changing any original witness or grant.
    const auto *original =
        key.kind == rules_bundle::Kind::Application &&
                work_.prepared.plan.receipt.input.kind == rules::InputKind::Override
            ? &work_.prepared.overrideRecord
            : nullptr;
    return finalClock(original);
}
bool Owner::allow(const ProgressRecord &record) {
    if (effectScope_) {
        if (!operating_ || !committing_ || !ordered_ || phase_ != Phase::Dispatching ||
            !workspace() || dispatching_ >= work_.intents.count)
            return false;
        const auto &intent = work_.intents.data[dispatching_];
        const bool claim = pendingEffect_.phase == EffectPhase::Claim;
        const auto &request = claim ? intent.claim : intent.result;
        if (record.operationId != request.operationId ||
            record.requestDigest != request.requestDigest || record.kind != request.kind ||
            record.expectedRevision != request.expectedRevision ||
            record.expectedLastIssued != request.expectedLastIssued ||
            record.profileRevision != request.profileRevision ||
            record.trustRevision != request.trustRevision ||
            !validateEffectRecord(pendingEffect_, record, intent.reservation).valid ||
            !bytesEqual(pendingEffect_.application, intent.application) ||
            !bytesEqual(pendingEffect_.catalog, intent.catalog) ||
            work_.decoded.plan.intents.count <= intent.reservation.index ||
            work_.envelope.intents.count <= intent.reservation.index)
            return false;
        const auto &reservation = work_.envelope.intents.data[intent.reservation.index];
        if (!bytesEqual(reservation.claim, intent.reservation.claim) ||
            !bytesEqual(reservation.result, intent.reservation.result) ||
            reservation.index != intent.reservation.index)
            return false;
        if (!claim)
            return !intent.resultCommitted &&
                   (intent.stage == IntentStage::Attempted ||
                    (intent.stage == IntentStage::Unknown && intent.settlementRequired &&
                     nonzero(intent.restoredBy) &&
                     pendingEffect_.result == HandlerResult::Unknown)) &&
                   rules::sameFence(pendingEffect_.expected, intent.originalClaimFence) &&
                   bytesEqual(pendingEffect_.ownerBoot, intent.originalClaimBoot);
        if (intent.stage != IntentStage::Pending || live_.mode != rules::Mode::Running ||
            !rules::sameFence(pendingEffect_.expected, live_.fence) ||
            !bytesEqual(pendingEffect_.catalog, activeCatalog_) ||
            !bytesEqual(pendingEffect_.ownerBoot, anchor_.boot))
            return false;
        if (intentEligibility_.check(work_.historicalProgram, installed_, live_, work_.envelope,
                                     work_.decoded, reservation,
                                     work_.decoded.plan.intents.data[intent.reservation.index],
                                     pendingEffect_, record) != IntentEligibility::Current)
            return false;
        return finalClock();
    }
    if (!committing_ || !operating_ || !ordered_ || phase_ != Phase::WritingControl ||
        record.kind != auxiliary_.kind || record.operationId != auxiliary_.operationId ||
        record.requestDigest != auxiliary_.requestDigest ||
        record.expectedRevision != auxiliary_.expectedRevision ||
        record.expectedLastIssued != auxiliary_.expectedLastIssued ||
        record.profileRevision != auxiliary_.profileRevision ||
        record.trustRevision != auxiliary_.trustRevision || record.issueCount ||
        record.firstIssued || record.lastIssued || !workspace())
        return false;
    if (record.kind != ProgressKind::Control)
        return false; // Intent claim/result adds its separate dispatch scope.
    rules::Digest fresh;
    ClockObservation now;
    if (codec::encodeCatalog(installed_, work_.comparison).scope !=
            codec::Scope::EncodedUnpublished ||
        !hash(hash_, work_.comparison.data, work_.comparison.count, fresh) ||
        !bytesEqual(fresh, catalog_) || !clock_.read(now) ||
        !bytesEqual(now.boot, pendingClock_.boot) || now.monotonic < pendingClock_.monotonicAfter ||
        (live_.mode == rules::Mode::Running &&
         now.monotonic - pendingClock_.monotonicBefore > 1000))
        return false;
    if (controlFence_.check(installed_, live_, pendingControl_, record) !=
        ControlEligibility::Current)
        return false;
    return finalClock();
}
Result Owner::advance() {
    if (!ordered_ || !workspace())
        return fail(Error::Argument, true);
    if (phase_ == Phase::WritingApplication || phase_ == Phase::WritingCatalog) {
        if (phase_ == Phase::WritingApplication &&
            work_.prepared.plan.receipt.input.kind == rules::InputKind::Override && !manualFence_)
            return fail(Error::Admission);
        committing_ = true;
        result_.journal = bundle_.step(*this);
        committing_ = false;
        const auto outcome = result_.journal.outcome;
        if (outcome == rules_bundle::Outcome::Unknown) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        if (outcome == rules_bundle::Outcome::Progress) {
            result_.phase = phase_;
            return result_;
        }
        if (outcome != rules_bundle::Outcome::Committed &&
            outcome != rules_bundle::Outcome::Duplicate) {
            if (outcome == rules_bundle::Outcome::Deferred && bundle_.active())
                return fail(Error::Capacity);
            if (!releaseOrdering(ProgressOutcome::Denied))
                return fail(Error::Recovery, true);
            ordered_ = false;
            recovered_ = false;
            pendingDefinitelyNotCommitted_ = true;
            phase_ = Phase::Fault;
            return fail(Error::Journal);
        }
        // A confirmed Final followed by any local inconsistency is Unknown,
        // never a safe rejection or permission to choose a fresh operation.
        if (!workspace() || work_.applications.count != baseApplications_ ||
            work_.intents.count != baseIntents_ || work_.history.count != preparedHistory_ ||
            !bundleWork_.records.count) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        if (phase_ == Phase::WritingCatalog) {
            const auto &final = bundleWork_.records.data[bundleWork_.records.count - 1].record;
            if (!pendingCatalogNew_ || final.kind != ProgressKind::Control || final.issueCount ||
                final.firstIssued || final.lastIssued ||
                final.operationId != pendingKey_.operation ||
                work_.catalogs.count == work_.catalogs.capacity) {
                phase_ = Phase::Unknown;
                recovered_ = false;
                return fail(Error::Recovery);
            }
            work_.catalogs.data[work_.catalogs.count++] = pendingCatalog_;
            pendingCatalogNew_ = false;
            phase_ = Phase::WritingControl;
            // Catalog finalization is inert. It is not the requested control
            // application and must never be reported as that operation's commit.
            result_.journal.outcome = rules_bundle::Outcome::Progress;
            result_.phase = phase_;
            return result_;
        }
        const auto &final = bundleWork_.records.data[bundleWork_.records.count - 1].record;
        if (final.kind != ProgressKind::Application ||
            std::memcmp(final.operationId.data(), pendingApplication_.operation.bytes, 16) ||
            final.issueCount != work_.prepared.plan.emissions.count ||
            journal_.snapshot().lastIssued != pendingState_.fence.lastIssued ||
            work_.applications.count == work_.applications.capacity ||
            work_.envelope.intents.count > work_.intents.capacity - work_.intents.count) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        pendingApplication_.final = final;
        for (std::size_t i = 0; i < work_.envelope.intents.count; ++i) {
            IntentEntry intent;
            intent.application = pendingApplication_.operation;
            intent.catalog = pendingApplication_.catalog;
            intent.reservation = work_.envelope.intents.data[i];
            work_.intents.data[work_.intents.count++] = intent;
            reservedIntents_ += 2;
        }
        work_.applications.data[work_.applications.count++] = pendingApplication_;
        live_ = pendingState_;
        liveBank_ = pendingBank_;
        result_.originalApplication = pendingApplication_.operation;
        anchor_.boot = pendingClock_.boot;
        anchor_.monotonic = pendingClock_.monotonicAfter;
        phase_ = Phase::Ready;
        // Publication above is a bounded no-callback metadata operation.
        // Releasing serialized ordering cannot revoke the existing commit.
        ordered_ = false;
        if (!releaseOrdering(ProgressOutcome::Committed))
            return fail(Error::Recovery, true);
        result_.phase = phase_;
        result_.error = Error::None;
        return result_;
    }
    if (phase_ == Phase::WritingControl) {
        if (!auxiliaryFrozen_) {
            const auto physical = journal_.snapshot();
            if (!physical.ready || physical.poisoned || physical.operating ||
                physical.revision == UINT64_MAX || physical.lastIssued != live_.fence.lastIssued)
                return fail(Error::Journal, true);
            auxiliary_.expectedRevision = physical.revision;
            auxiliary_.expectedLastIssued = physical.lastIssued;
            auxiliaryFrozen_ = true;
        }
        committing_ = true;
        result_.journal = bundle_.auxiliary(auxiliary_, *this, reservedIntents_);
        committing_ = false;
        const auto outcome = result_.journal.outcome;
        if (outcome == rules_bundle::Outcome::Unknown) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        if (outcome != rules_bundle::Outcome::Committed &&
            outcome != rules_bundle::Outcome::Duplicate) {
            if (outcome == rules_bundle::Outcome::Deferred)
                return fail(Error::Capacity);
            ordered_ = false;
            if (!releaseOrdering(ProgressOutcome::Denied))
                return fail(Error::Recovery, true);
            recovered_ = false;
            phase_ = Phase::Fault;
            return fail(Error::Journal);
        }
        if (!workspace() || !bundleWork_.records.count ||
            work_.applications.count != baseApplications_ || work_.intents.count != baseIntents_ ||
            work_.controls.count != baseControls_ ||
            work_.controls.count == work_.controls.capacity ||
            work_.history.count != preparedHistory_) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        const auto &record = bundleWork_.records.data[bundleWork_.records.count - 1].record;
        if (record.kind != ProgressKind::Control || record.operationId != auxiliary_.operationId ||
            record.requestDigest != auxiliary_.requestDigest || record.issueCount ||
            journal_.snapshot().lastIssued != live_.fence.lastIssued) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        pendingControlEntry_.record = record;
        work_.controls.data[work_.controls.count++] = pendingControlEntry_;
        if (pendingControl_.kind == ControlKind::RestoreStopped)
            for (std::size_t i = 0; i < work_.intents.count; ++i) {
                auto &intent = work_.intents.data[i];
                if (intent.settlementRequired)
                    intent.restoredBy = pendingControl_.operation;
            }
        live_ = pendingState_;
        liveBank_ = pendingBank_;
        activated_ = true;
        activeCatalog_ = pendingControl_.catalog;
        anchor_.boot = pendingClock_.boot;
        anchor_.monotonic = pendingClock_.monotonicAfter;
        phase_ = live_.mode == rules::Mode::Stopped || live_.mode == rules::Mode::RecoveryRequired
                     ? Phase::Stopped
                     : Phase::Ready;
        ordered_ = false;
        if (!releaseOrdering(ProgressOutcome::Committed))
            return fail(Error::Recovery, true);
        result_.phase = phase_;
        result_.error = Error::None;
        return result_;
    }
    return fail(Error::Argument);
}
Result Owner::submit(const rules::Input &source, const rules::Identity &operation,
                     const rules::Digest &request) {
    const rules::Input input = source;
    const rules::Identity op = operation;
    const rules::Digest digest = request;
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    if (input.kind == rules::InputKind::Override ||
        input.overrideAdmission.scope() != rules::OverrideAdmissionScope::Empty)
        return fail(Error::Admission);
    if (phase_ == Phase::Unknown)
        return fail(Error::Recovery);
    if (phase_ == Phase::WritingApplication) {
        if (!bytesEqual(op, work_.prepared.operation) ||
            !bytesEqual(digest, work_.prepared.request) ||
            !sameInput(input, work_.prepared.plan.receipt.input))
            return fail(Error::Conflict);
        return advance();
    }
    // Only the completely published/recovered history may answer a retry.
    // Load the original raw application instead of trusting its caller-supplied
    // request digest as a substitute for the immutable input identity.
    if (!recovered_ || !activated_ || !workspace() ||
        (phase_ != Phase::Ready && phase_ != Phase::Stopped) ||
        !rules::sameContext(input.context, live_.context) ||
        !rules::sameContext(input.context, installed_.context) || !nonzero(op))
        return fail(Error::Context);
    const auto duplicate = [&](const ApplicationEntry &entry) {
        result_.phase = phase_;
        result_.originalApplication = entry.operation;
        result_.journal.outcome = rules_bundle::Outcome::Duplicate;
        auto &original = result_.journal.journal;
        original.outcome = ProgressOutcome::Duplicate;
        original.revision = entry.final.revision;
        original.index = entry.final.index;
        original.firstIssued = entry.final.firstIssued;
        original.lastIssued = entry.final.lastIssued;
        return result_;
    };
    for (std::size_t i = 0; i < work_.applications.count; ++i) {
        const ApplicationEntry entry = work_.applications.data[i];
        if (!bytesEqual(entry.operation, op))
            continue;
        if (!bytesEqual(entry.request, digest))
            return fail(Error::Conflict);
        if (!loadApplication(entry))
            return fail(Error::Recovery, true);
        if (!sameInput(input, work_.decoded.plan.receipt.input))
            return fail(Error::Conflict);
        return duplicate(entry);
    }
    // A new correlation ID must not collide with any existing semantic or
    // reserved physical phase. It is NOT installed as an alias/receipt below.
    if (!uniquePhase(op))
        return fail(Error::Conflict);
    if (input.kind == rules::InputKind::Fact || input.kind == rules::InputKind::Local ||
        input.kind == rules::InputKind::Timer) {
        for (std::size_t i = 0; i < work_.applications.count; ++i) {
            const ApplicationEntry entry = work_.applications.data[i];
            if (!loadApplication(entry))
                return fail(Error::Recovery, true);
            const auto &prior = work_.decoded.plan.receipt.input;
            if (prior.kind != input.kind)
                continue;
            bool originalInput = false;
            if (input.kind == rules::InputKind::Fact) {
                const auto &a = input.fact.ref.id;
                const auto &b = prior.fact.ref.id;
                originalInput = bytesEqual(a.event, b.event) && bytesEqual(a.origin, b.origin) &&
                                bytesEqual(a.epoch, b.epoch) && a.sequence == b.sequence;
            } else {
                // Local/timer operation IDs are scoped to event/run/station,
                // never global FactIDs and never truncated clock-boot labels.
                if (!bytesEqual(input.context.event, prior.context.event) ||
                    !bytesEqual(input.context.run, prior.context.run) ||
                    !bytesEqual(input.context.station, prior.context.station))
                    continue;
                originalInput =
                    input.kind == rules::InputKind::Local
                        ? input.bootGeneration == prior.bootGeneration &&
                              input.sequence == prior.sequence
                        : input.timer == prior.timer && input.armGeneration == prior.armGeneration;
            }
            if (!originalInput)
                continue;
            if (!sameInput(input, prior))
                return fail(Error::Conflict);
            return duplicate(entry);
        }
    }
    const auto prepared = plan(input, op, digest);
    return prepared.error == Error::None ? advance() : prepared;
}
Result Owner::service() {
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    if (phase_ == Phase::Unknown)
        return fail(Error::Recovery);
    if (phase_ == Phase::Dispatching)
        return finishDispatch();
    if (phase_ == Phase::Ready || phase_ == Phase::Stopped || phase_ == Phase::Uninitialized) {
        if (phase_ == Phase::Ready && live_.mode == rules::Mode::Running) {
            ClockWitness observed;
            rules::State unchanged;
            if (!clock(observed, unchanged))
                return fail(clockError_, true);
        }
        result_.phase = phase_;
        return result_;
    }
    return advance();
}

rules::ManualResult Owner::submit(const rules::OverrideAdmission &source,
                                  rules::IManualFinalFence &fence) {
    const rules::OverrideAdmission admission = source;
    const auto record = admission.record();
    rules::ManualResult out;
    out.operation = record.operation;
    out.token = record.token;
    out.expiresAt = record.expiresAt;
    if (operating_) {
        out.status = rules::ManualStatus::Deferred;
        out.error = rules::ManualError::Busy;
        return out;
    }
    Operation guard(operating_);
    ManualFenceScope finalScope(manualFence_, fence);
    result_ = {};
    if (admission.scope() != rules::OverrideAdmissionScope::Live ||
        !rules::sameContext(record.context, installed_.context) ||
        !rules::sameContext(record.context, live_.context) ||
        !bytesEqual(record.program, catalog_)) {
        out.error = rules::ManualError::Context;
        return out;
    }
    if (!workspace()) {
        out.status = rules::ManualStatus::Unknown;
        out.error = rules::ManualError::RecoveryRequired;
        return out;
    }
    if (!recovered_ || phase_ == Phase::Recovering || phase_ == Phase::Fault ||
        phase_ == Phase::Unknown) {
        out.status = pendingDefinitelyNotCommitted_ &&
                             bytesEqual(record.operation, pendingApplication_.operation)
                         ? rules::ManualStatus::NotCommitted
                         : rules::ManualStatus::Unknown;
        out.error = out.status == rules::ManualStatus::Unknown
                        ? rules::ManualError::RecoveryRequired
                        : rules::ManualError::None;
        out.release = out.status == rules::ManualStatus::NotCommitted
                          ? rules::ReleaseDisposition::NoRelease
                          : rules::ReleaseDisposition::Unresolved;
        return out;
    }
    for (std::size_t i = 0; i < work_.applications.count; ++i) {
        const auto &entry = work_.applications.data[i];
        if (!bytesEqual(entry.operation, record.operation))
            continue;
        if (!bytesEqual(entry.request, record.confirmDigest) ||
            !bytesEqual(entry.catalog, record.program)) {
            out.error = rules::ManualError::Conflict;
            return out;
        }
        // The private live admission was minted by the original ManualOwner;
        // its complete canonical Confirm digest binds that immutable record.
        // This is a read of the already verified original application only.
        out.status = rules::ManualStatus::Duplicate;
        out.release = entry.release;
        return out;
    }
    if (phase_ == Phase::Unknown || (!recovered_ && !pendingDefinitelyNotCommitted_)) {
        out.status = rules::ManualStatus::Unknown;
        out.error = rules::ManualError::RecoveryRequired;
        return out;
    }
    if (pendingDefinitelyNotCommitted_ &&
        bytesEqual(record.operation, pendingApplication_.operation)) {
        out.status = rules::ManualStatus::NotCommitted;
        out.release = rules::ReleaseDisposition::NoRelease;
        return out;
    }
    if (phase_ == Phase::WritingApplication) {
        if (!bytesEqual(record.operation, work_.prepared.operation) ||
            !rules::sameOverrideRecord(record, work_.prepared.overrideRecord)) {
            out.error = rules::ManualError::Conflict;
            return out;
        }
        advance();
    } else {
        rules::Input input;
        input.context = record.context;
        input.kind = rules::InputKind::Override;
        input.overrideAdmission = admission;
        const auto prepared = plan(input, record.operation, record.confirmDigest);
        if (prepared.error == Error::None)
            advance();
    }
    // A known confirmed Application remains committed even if subsequent
    // metadata lease release fails. This is durable truth, not a Ready scope.
    // This path is unreachable during provisional recovery (gated above).
    if (result_.journal.outcome == rules_bundle::Outcome::Committed ||
        result_.journal.outcome == rules_bundle::Outcome::Duplicate)
        for (std::size_t i = 0; i < work_.applications.count; ++i) {
            const auto &entry = work_.applications.data[i];
            if (bytesEqual(entry.operation, record.operation) &&
                bytesEqual(entry.request, record.confirmDigest)) {
                out.status = rules::ManualStatus::Committed;
                out.release = entry.release;
                if (phase_ == Phase::Fault)
                    out.error = rules::ManualError::RecoveryRequired;
                return out;
            }
        }
    if (phase_ == Phase::Unknown) {
        out.status = rules::ManualStatus::Unknown;
        out.error = rules::ManualError::RecoveryRequired;
    } else if (phase_ == Phase::WritingApplication) {
        out.status = rules::ManualStatus::Confirming;
    } else if (phase_ == Phase::Ready && result_.error == Error::None) {
        out.status = rules::ManualStatus::Committed;
        out.release = pendingApplication_.release;
    } else if (pendingDefinitelyNotCommitted_) {
        out.status = rules::ManualStatus::NotCommitted;
        out.release = rules::ReleaseDisposition::NoRelease;
    } else {
        out.status = result_.error == Error::Capacity ? rules::ManualStatus::Deferred
                                                      : rules::ManualStatus::Rejected;
        out.error = result_.error == Error::Capacity ? rules::ManualError::Capacity
                                                     : rules::ManualError::Transaction;
    }
    return out;
}
rules::ManualResult Owner::status(const rules::Context &sourceContext,
                                  const rules::Identity &sourceOperation) {
    const rules::Context context = sourceContext;
    const rules::Identity operation = sourceOperation;
    rules::ManualResult out;
    out.operation = operation;
    if (operating_) {
        out.status = rules::ManualStatus::Deferred;
        out.error = rules::ManualError::Busy;
        return out;
    }
    Operation guard(operating_);
    if (!rules::sameContext(context, installed_.context) ||
        !rules::sameContext(context, live_.context) || !workspace()) {
        out.error = rules::ManualError::Context;
        return out;
    }
    if (!recovered_ || phase_ == Phase::Recovering || phase_ == Phase::Fault ||
        phase_ == Phase::Unknown) {
        out.status =
            pendingDefinitelyNotCommitted_ && bytesEqual(operation, pendingApplication_.operation)
                ? rules::ManualStatus::NotCommitted
                : rules::ManualStatus::Unknown;
        out.error = out.status == rules::ManualStatus::Unknown
                        ? rules::ManualError::RecoveryRequired
                        : rules::ManualError::None;
        out.release = out.status == rules::ManualStatus::NotCommitted
                          ? rules::ReleaseDisposition::NoRelease
                          : rules::ReleaseDisposition::Unresolved;
        return out;
    }
    for (std::size_t i = 0; i < work_.applications.count; ++i) {
        const auto &entry = work_.applications.data[i];
        if (bytesEqual(entry.operation, operation)) {
            out.status = rules::ManualStatus::Committed;
            out.release = entry.release;
            return out;
        }
    }
    if (record(operation)) {
        out.error = rules::ManualError::Conflict;
        return out; // A Control/Effect ID is not an absent Application.
    }
    for (std::size_t i = 0; i < bundleWork_.bundles.count; ++i)
        if (!std::memcmp(bundleWork_.bundles.data[i].key.operation.data(), operation.bytes, 16) &&
            bundleWork_.bundles.data[i].key.kind != rules_bundle::Kind::Application) {
            out.error = rules::ManualError::Conflict;
            return out;
        }
    if (phase_ == Phase::Unknown || (!recovered_ && !pendingDefinitelyNotCommitted_)) {
        out.status = rules::ManualStatus::Unknown;
        out.error = rules::ManualError::RecoveryRequired;
    } else if (phase_ == Phase::WritingApplication &&
               bytesEqual(operation, work_.prepared.operation)) {
        out.status = rules::ManualStatus::Confirming;
    } else if (recovered_ || (pendingDefinitelyNotCommitted_ &&
                              bytesEqual(operation, pendingApplication_.operation))) {
        // Only qualified complete replay or a known safe noncommit proves
        // absence. RAM emptiness/old caches alone never establish this result.
        out.status = rules::ManualStatus::NotCommitted;
        out.release = rules::ReleaseDisposition::NoRelease;
    } else {
        out.status = rules::ManualStatus::Unknown;
        out.error = rules::ManualError::RecoveryRequired;
    }
    return out;
}

Result Owner::writeControl(ControlKind kind, const rules::Identity &operation,
                           const rules::Digest &request) {
    const auto op = operation;
    const auto copiedRequest = request;
    if (!recovered_ || !workspace() || !nonzero(op) || ordered_ ||
        (kind == ControlKind::Activation
             ? (phase_ != Phase::Uninitialized && phase_ != Phase::Stopped &&
                phase_ != Phase::Ready)
             : (!activated_ || (phase_ != Phase::Ready && phase_ != Phase::Stopped))))
        return fail(Error::Context);
    for (std::size_t i = 0; i < work_.controls.count; ++i) {
        const auto &old = work_.controls.data[i];
        if (!bytesEqual(old.operation, op))
            continue;
        if (!bytesEqual(old.request, copiedRequest) || old.kind != kind)
            return fail(Error::Conflict);
        result_.journal.outcome = rules_bundle::Outcome::Duplicate;
        result_.journal.journal.outcome = ProgressOutcome::Duplicate;
        result_.journal.journal.index = old.record.index;
        result_.phase = phase_;
        return result_;
    }
    if (!uniquePhase(op) || work_.controls.count == work_.controls.capacity)
        return fail(Error::Capacity);
    if (!clock(pendingClock_, planning_))
        return fail(clockError_, true);
    pendingControl_ = {};
    auto &control = pendingControl_;
    control.operation = op;
    control.request = copiedRequest;
    control.catalog = catalog_;
    control.context = installed_.context;
    control.expected = live_.fence;
    control.targetFence = live_.fence;
    control.clock = pendingClock_;
    control.before = activated_ ? live_.mode : rules::Mode::Stopped;
    control.kind = kind;
    control.targetFence.activeTime = pendingClock_.activeAfter;
    if (kind != ControlKind::Checkpoint) {
        if (control.targetFence.progress == UINT64_MAX)
            return fail(Error::Range);
        ++control.targetFence.progress;
    }
    switch (kind) {
    case ControlKind::Start:
    case ControlKind::Resume:
    case ControlKind::Checkpoint:
        control.after = rules::Mode::Running;
        break;
    case ControlKind::Pause:
        control.after = rules::Mode::Paused;
        break;
    case ControlKind::Stop:
    case ControlKind::RestoreStopped:
    case ControlKind::Activation:
        control.after = rules::Mode::Stopped;
        break;
    }
    const Control original = control;
    const auto proof = controls_.freezeControl(installed_, live_, control, work_.preparedEvidence);
    // Original provenance is mandatory BEFORE even inert catalog writes. Only
    // Activation may obtain target install revisions from its actual proof.
    if (!accepted(proof, EvidenceScope::FrozenOriginal) ||
        !bytesEqual(original.operation, control.operation) ||
        !bytesEqual(original.request, control.request) ||
        !bytesEqual(original.catalog, control.catalog) ||
        !rules::sameContext(original.context, control.context) ||
        !rules::sameFence(original.expected, control.expected) || original.kind != control.kind ||
        original.before != control.before || original.after != control.after ||
        !bytesEqual(original.clock.boot, control.clock.boot) ||
        original.clock.monotonicBefore != control.clock.monotonicBefore ||
        original.clock.monotonicAfter != control.clock.monotonicAfter ||
        original.clock.activeBefore != control.clock.activeBefore ||
        original.clock.activeAfter != control.clock.activeAfter ||
        (kind != ControlKind::Activation &&
         !rules::sameFence(original.targetFence, control.targetFence)))
        return fail(Error::Admission);
    const auto first = reinterpret_cast<std::uintptr_t>(work_.preparedEvidence.bytes.data);
    const auto auth = reinterpret_cast<std::uintptr_t>(control.authorization.data);
    if (!control.authorization.count || control.authorization.count > 2048 || auth < first ||
        auth - first > work_.preparedEvidence.bytes.count ||
        control.authorization.count > work_.preparedEvidence.bytes.count - (auth - first) ||
        !encodeControl(control, work_.applicationBytes).valid)
        return fail(Error::Codec);
    if (controlFence_.preflight(installed_, live_, control) != ControlEligibility::Current ||
        !finalClock())
        return fail(clockError_ == Error::None ? Error::Admission : clockError_);
    pendingState_ = live_;
    pendingBank_ = liveBank_;
    if (!activated_) {
        pendingBank_ = liveBank_ == 0 ? 1 : 0;
        if (kind != ControlKind::Activation ||
            rules::Planner::initialize(installed_, control.targetFence,
                                       work_.banks[pendingBank_].storage, pendingState_)
                    .decision != rules::Decision::Ready)
            return fail(Error::Context);
    } else {
        if (!rules::sameContext(live_.context, installed_.context) ||
            !bytesEqual(activeCatalog_, catalog_))
            return fail(Error::Context); // No implicit migration/reset.
        pendingState_.fence = control.targetFence;
    }
    pendingState_.mode = control.after;
    pendingControlEntry_ = {};
    pendingControlEntry_.operation = op;
    pendingControlEntry_.kind = kind;
    pendingControlEntry_.request = copiedRequest;
    pendingControlEntry_.catalog = catalog_;
    pendingControlEntry_.before = control.expected;
    pendingControlEntry_.count = static_cast<std::uint32_t>(work_.applicationBytes.count);
    if (!hash(hash_, work_.applicationBytes.data, work_.applicationBytes.count,
              pendingControlEntry_.raw))
        return fail(Error::Crypto);
    const auto historyBefore = work_.history.count;
    pendingCatalogNew_ = true;
    for (std::size_t i = 0; i < work_.catalogs.count; ++i)
        if (bytesEqual(work_.catalogs.data[i].digest, catalog_))
            pendingCatalogNew_ = false;
    pendingCatalog_ = {};
    rules::Identity catalogOperation;
    if (pendingCatalogNew_) {
        rules_bundle::Identity generated{};
        if (work_.catalogs.count == work_.catalogs.capacity || !ids_.make(generated))
            return fail(Error::Capacity);
        std::memcpy(catalogOperation.bytes, generated.data(), 16);
        if (!uniquePhase(catalogOperation) || bytesEqual(catalogOperation, op) ||
            codec::encodeCatalog(installed_, bundleWork_.staged).scope !=
                codec::Scope::EncodedUnpublished)
            return fail(Error::Conflict);
        rules::Digest actual;
        if (!hash(hash_, bundleWork_.staged.data, bundleWork_.staged.count, actual) ||
            !bytesEqual(actual, catalog_))
            return fail(Error::Crypto);
        pendingCatalog_.digest = catalog_;
        pendingCatalog_.deployment = installed_.context.deployment;
        pendingCatalog_.count = static_cast<std::uint32_t>(bundleWork_.staged.count);
        if (!remember(bundleWork_.staged.data, bundleWork_.staged.count, pendingCatalog_.first))
            return fail(Error::Capacity);
    }
    if (!remember(work_.applicationBytes.data, work_.applicationBytes.count,
                  pendingControlEntry_.first)) {
        work_.history.count = historyBefore;
        return fail(Error::Capacity);
    }
    CommitBinding binding;
    binding.kind = CommitKind::Control;
    binding.operation = op;
    binding.context = installed_.context;
    binding.expected = live_.fence;
    binding.request = copiedRequest;
    binding.catalog = catalog_;
    binding.commitment = pendingControlEntry_.raw;
    if (!acquire(binding)) {
        work_.history.count = historyBefore;
        return phase_ == Phase::Fault ? result_ : fail(Error::Stale);
    }
    heldOperation_ = op;
    ordered_ = true;
    baseApplications_ = work_.applications.count;
    baseIntents_ = work_.intents.count;
    baseControls_ = work_.controls.count;
    preparedHistory_ = work_.history.count;
    auxiliary_ = {};
    auxiliary_.kind = ProgressKind::Control;
    std::memcpy(auxiliary_.operationId.data(), op.bytes, 16);
    std::memcpy(auxiliary_.requestDigest.data(), pendingControlEntry_.raw.bytes, 32);
    auxiliary_.profileRevision = control.expected.profile;
    auxiliary_.trustRevision = control.expected.trust;
    auxiliary_.payload = work_.applicationBytes.data;
    auxiliary_.payloadBytes = work_.applicationBytes.count;
    auxiliaryFrozen_ = false;
    if (pendingCatalogNew_) {
        pendingKey_ = {};
        pendingKey_.kind = rules_bundle::Kind::Catalog;
        std::memcpy(pendingKey_.operation.data(), catalogOperation.bytes, 16);
        std::memcpy(pendingKey_.request.data(), catalog_.bytes, 32);
        std::memcpy(pendingKey_.deployment.data(), installed_.context.deployment.bytes, 32);
        std::memcpy(pendingKey_.catalog.data(), catalog_.bytes, 32);
        pendingKey_.blob = pendingKey_.catalog;
        pendingKey_.bytes = static_cast<std::uint32_t>(bundleWork_.staged.count);
        pendingKey_.chunks = static_cast<std::uint8_t>((pendingKey_.bytes + 2047) / 2048);
        rules_bundle::HeaderMeta meta;
        meta.profileRevision = control.expected.profile;
        meta.trustRevision = control.expected.trust;
        result_.journal = bundle_.begin(pendingKey_, meta, 0, reservedIntents_ + 1);
        if (result_.journal.outcome != rules_bundle::Outcome::Prepared) {
            ordered_ = false;
            if (!releaseOrdering(ProgressOutcome::Denied))
                return fail(Error::Recovery, true);
            work_.history.count = historyBefore;
            return fail(Error::Journal);
        }
        for (std::size_t i = 0; i < pendingKey_.chunks; ++i)
            if (!std::memcmp(bundleWork_.chunks[i].operation.data(), op.bytes, 16)) {
                const auto canceled = bundle_.cancelPrepared();
                if (canceled.outcome != rules_bundle::Outcome::Abandoned) {
                    phase_ = Phase::Unknown;
                    return fail(Error::Recovery);
                }
                ordered_ = false;
                if (!releaseOrdering(ProgressOutcome::Denied))
                    return fail(Error::Recovery, true);
                work_.history.count = historyBefore;
                return fail(Error::Conflict);
            }
        phase_ = Phase::WritingCatalog;
    } else {
        const auto physical = journal_.snapshot();
        if (physical.records >= ProgressLimits::Records ||
            reservedIntents_ >= ProgressLimits::Records - physical.records) {
            ordered_ = false;
            if (!releaseOrdering(ProgressOutcome::Denied))
                return fail(Error::Recovery, true);
            work_.history.count = historyBefore;
            return fail(Error::Capacity);
        }
        phase_ = Phase::WritingControl;
    }
    result_.phase = phase_;
    return result_;
}
Result Owner::activate(const rules::Identity &operation, const rules::Digest &request) {
    return control(ControlKind::Activation, operation, request);
}
Result Owner::control(ControlKind kind, const rules::Identity &sourceOperation,
                      const rules::Digest &sourceRequest) {
    const auto op = sourceOperation;
    const auto request = sourceRequest;
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    if (phase_ == Phase::Unknown)
        return fail(Error::Recovery);
    if (phase_ == Phase::WritingControl || phase_ == Phase::WritingCatalog) {
        if (!bytesEqual(pendingControl_.operation, op) ||
            !bytesEqual(pendingControl_.request, request) || pendingControl_.kind != kind)
            return fail(Error::Conflict);
        return advance();
    }
    const auto prepared = writeControl(kind, op, request);
    return prepared.error == Error::None &&
                   result_.journal.outcome != rules_bundle::Outcome::Duplicate
               ? advance()
               : prepared;
}

bool Owner::outbox(const rules::Identity &sourceApplication, std::size_t index,
                   codec::SignedFact &destination, rules::Pool<std::uint8_t> &owned) {
    const auto operation = sourceApplication;
    if (operating_ || owned.count > owned.capacity ||
        !workspace(&destination, sizeof(destination), &owned, sizeof(owned), owned.data,
                   owned.capacity))
        return false;
    Operation guard(operating_);
    destination = {};
    owned.count = 0;
    // Decode scratch is exclusively reusable only while no transaction is in
    // flight. The receiver owns a full copy, never a view into that scratch.
    if (!recovered_ || (phase_ != Phase::Ready && phase_ != Phase::Stopped))
        return false;
    for (std::size_t i = 0; i < work_.applications.count; ++i) {
        const auto &entry = work_.applications.data[i];
        if (!bytesEqual(entry.operation, operation))
            continue;
        if (entry.first > work_.history.count || entry.count > work_.history.count - entry.first ||
            !decodeTransaction(work_.history.data + entry.first, entry.count, work_.envelope,
                               work_.decodedApplication)
                 .valid ||
            codec::decodeApplication(work_.envelope.application, work_.decoded,
                                     work_.decodedStorage)
                    .scope != codec::Scope::DecodedUnauthenticated ||
            !bytesEqual(work_.decoded.operation, operation) ||
            !bytesEqual(work_.decoded.request, entry.request) ||
            !bytesEqual(work_.decoded.catalog, entry.catalog) ||
            index >= work_.decoded.outbox.count)
            return false;
        const auto &fact = work_.decoded.outbox.data[index];
        if (!fact.cose.count || fact.cose.count > owned.capacity)
            return false;
        std::memcpy(owned.data, fact.cose.data, fact.cose.count);
        owned.count = fact.cose.count;
        destination.ref = fact.ref;
        std::memcpy(destination.publicKey, fact.publicKey, sizeof(destination.publicKey));
        destination.cose.data = owned.data;
        destination.cose.count = owned.count;
        return true;
    }
    return false;
}

bool Owner::physicalCapacity(std::size_t additional) {
    Effect worst;
    for (auto &b : worst.application.bytes)
        b = 255;
    for (auto &b : worst.catalog.bytes)
        b = 255;
    for (auto &b : worst.ownerBoot.bytes)
        b = 255;
    worst.intentIndex = static_cast<std::uint16_t>(MaximumIntentReservations - 1);
    worst.expected.progress = worst.expected.profile = worst.expected.trust = UINT64_MAX;
    worst.expected.facts = worst.expected.ledger = worst.expected.resources = UINT64_MAX;
    worst.expected.rights = worst.expected.capacity = UINT64_MAX;
    worst.expected.lastIssued = worst.expected.activeTime = UINT64_MAX;
    worst.phase = EffectPhase::Result;
    worst.result = HandlerResult::Unknown;
    // Actual fixed-version codec gives its worst-case complete byte size;
    // no guessed truncation of future full-width physical fences.
    if (!encodeEffect(worst, work_.comparison).valid || !work_.comparison.count)
        return false;
    std::size_t missing = additional;
    for (std::size_t i = 0; i < work_.intents.count; ++i) {
        const auto &entry = work_.intents.data[i];
        const std::size_t n =
            (entry.stage == IntentStage::Pending && !entry.claimBytes.count ? 1 : 0) +
            (!entry.resultCommitted && !entry.resultBytes.count ? 1 : 0);
        if (n > SIZE_MAX - missing)
            return false;
        missing += n;
    }
    return missing <=
           (work_.physicalBytes.capacity - work_.physicalBytes.count) / work_.comparison.count;
}
bool Owner::loadApplication(const ApplicationEntry &entry) {
    if (entry.first > work_.history.count || entry.count > work_.history.count - entry.first)
        return false;
    rules::Digest actual;
    if (!hash(hash_, work_.history.data + entry.first, entry.count, actual) ||
        !bytesEqual(actual, entry.blob) ||
        !decodeTransaction(work_.history.data + entry.first, entry.count, work_.envelope,
                           work_.decodedApplication)
             .valid ||
        !bytesEqual(work_.envelope.operation, entry.operation) ||
        !bytesEqual(work_.envelope.request, entry.request) ||
        !bytesEqual(work_.envelope.catalog, entry.catalog) ||
        codec::decodeApplication(work_.envelope.application, work_.decoded, work_.decodedStorage)
                .scope != codec::Scope::DecodedUnauthenticated ||
        !bytesEqual(work_.decoded.operation, entry.operation) ||
        !bytesEqual(work_.decoded.request, entry.request) ||
        !bytesEqual(work_.decoded.catalog, entry.catalog) ||
        !rules::sameContext(work_.decoded.plan.context, live_.context) ||
        work_.envelope.intents.count != work_.decoded.plan.intents.count)
        return false;
    return loadCatalog(entry.catalog) &&
           rules::sameContext(work_.historicalProgram.context, work_.decoded.plan.context);
}

Result Owner::effect(std::size_t index, EffectPhase phase, HandlerResult handlerResult,
                     bool settlement) {
    if (!workspace() || index >= work_.intents.count || !reservedIntents_)
        return fail(Error::Argument, true);
    auto &intent = work_.intents.data[index];
    auto &request = phase == EffectPhase::Claim ? intent.claim : intent.result;
    auto &encoded = phase == EffectPhase::Claim ? intent.claimBytes : intent.resultBytes;
    const auto physicalBefore = journal_.snapshot();
    if (!physicalBefore.ready || physicalBefore.poisoned || physicalBefore.operating ||
        physicalBefore.revision == UINT64_MAX ||
        physicalBefore.lastIssued != live_.fence.lastIssued)
        return fail(Error::Journal, true);
    const std::size_t beforeBytes = work_.physicalBytes.count;
    const bool newlyCaptured = !encoded.count;
    if (newlyCaptured) {
        pendingEffect_ = {};
        pendingEffect_.application = intent.application;
        pendingEffect_.catalog = intent.catalog;
        pendingEffect_.intentIndex = intent.reservation.index;
        pendingEffect_.phase = phase;
        pendingEffect_.result = handlerResult;
        pendingEffect_.expected =
            phase == EffectPhase::Claim ? live_.fence : intent.originalClaimFence;
        pendingEffect_.ownerBoot =
            phase == EffectPhase::Claim ? anchor_.boot : intent.originalClaimBoot;
        if (!encodeEffect(pendingEffect_, work_.comparison).valid ||
            work_.comparison.count > work_.physicalBytes.capacity - work_.physicalBytes.count)
            return fail(Error::Capacity);
        rules::Digest digest;
        if (!hash(hash_, work_.comparison.data, work_.comparison.count, digest))
            return fail(Error::Crypto);
        auto *destination = work_.physicalBytes.data + work_.physicalBytes.count;
        std::memcpy(destination, work_.comparison.data, work_.comparison.count);
        encoded.data = destination;
        encoded.count = work_.comparison.count;
        work_.physicalBytes.count += encoded.count;
        request = {};
        const auto &id =
            phase == EffectPhase::Claim ? intent.reservation.claim : intent.reservation.result;
        std::memcpy(request.operationId.data(), id.bytes, 16);
        std::memcpy(request.requestDigest.data(), digest.bytes, 32);
        request.kind =
            phase == EffectPhase::Claim ? ProgressKind::EffectClaim : ProgressKind::EffectResult;
        request.expectedRevision = physicalBefore.revision;
        request.expectedLastIssued = physicalBefore.lastIssued;
        request.profileRevision = pendingEffect_.expected.profile;
        request.trustRevision = pendingEffect_.expected.trust;
        request.payload = encoded.data;
        request.payloadBytes = encoded.count;
    } else if (!decodeEffect(encoded.data, encoded.count, pendingEffect_).valid ||
               request.payload != encoded.data || request.payloadBytes != encoded.count) {
        return fail(Error::Recovery, true);
    }
    dispatching_ = index;
    phase_ = Phase::Dispatching;
    if (!ordered_) {
        CommitBinding binding;
        binding.kind = phase == EffectPhase::Claim ? CommitKind::Claim : CommitKind::Result;
        binding.operation =
            phase == EffectPhase::Claim ? intent.reservation.claim : intent.reservation.result;
        binding.parent = intent.application;
        binding.context = live_.context;
        binding.expected = live_.fence;
        binding.request = work_.decoded.request;
        binding.catalog = intent.catalog;
        std::memcpy(binding.commitment.bytes, request.requestDigest.data(), 32);
        if (!acquire(binding)) {
            if (phase_ == Phase::Fault)
                return result_;
            if (newlyCaptured && work_.physicalBytes.count == beforeBytes + encoded.count) {
                work_.physicalBytes.count = beforeBytes;
                encoded = {};
                request = {};
            }
            phase_ = live_.mode == rules::Mode::Stopped ? Phase::Stopped : Phase::Ready;
            return fail(Error::Stale);
        }
    }
    if (phase == EffectPhase::Result && lease_.binding.kind == CommitKind::Claim) {
        CommitBinding binding = lease_.binding;
        binding.kind = CommitKind::Result;
        binding.operation = intent.reservation.result;
        binding.expected = live_.fence;
        std::memcpy(binding.commitment.bytes, request.requestDigest.data(), 32);
        OrderingLease advanced;
        if (ordering_.transition(lease_, binding, advanced) != Ordering::Held ||
            !nonzero(advanced.boot) || !advanced.generation ||
            !sameCommitBinding(advanced.binding, binding) ||
            !bytesEqual(advanced.boot, lease_.boot) || advanced.generation <= lease_.generation) {
            phase_ = Phase::Fault;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        lease_ = advanced;
        heldOperation_ = binding.operation;
    }
    if (phase == EffectPhase::Claim) {
        if (!clock(pendingClock_, planning_))
            return fail(clockError_, true);
        ProgressRecord candidate;
        candidate.operationId = request.operationId;
        candidate.requestDigest = request.requestDigest;
        candidate.kind = request.kind;
        candidate.expectedRevision = request.expectedRevision;
        candidate.revision = request.expectedRevision + 1;
        candidate.expectedLastIssued = request.expectedLastIssued;
        candidate.profileRevision = request.profileRevision;
        candidate.trustRevision = request.trustRevision;
        candidate.payloadBytes = static_cast<std::uint16_t>(request.payloadBytes);
        candidate.index = physicalBefore.records;
        bool eligible;
        {
            Operation committing(committing_);
            Operation active(effectScope_);
            eligible = allow(candidate);
        }
        if (!eligible) {
            // No commit call has occurred. Discard ONLY this private never-
            // written capture, retaining every immutable envelope identity.
            const auto after = journal_.snapshot();
            bool present = false;
            for (std::size_t i = 0; i < bundleWork_.records.count; ++i)
                present |= bundleWork_.records.data[i].record.operationId == request.operationId;
            if (after.ready && !after.poisoned && !after.operating &&
                after.revision == physicalBefore.revision &&
                after.records == physicalBefore.records &&
                after.lastIssued == physicalBefore.lastIssued && !present && newlyCaptured &&
                work_.physicalBytes.count == beforeBytes + encoded.count) {
                work_.physicalBytes.count = beforeBytes;
                encoded = {};
                request = {};
                phase_ = Phase::Ready;
                return fail(Error::Admission);
            }
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
    } else if ((settlement && (!intent.settlementRequired || !nonzero(intent.restoredBy) ||
                               handlerResult != HandlerResult::Unknown)) ||
               (!settlement && intent.stage != IntentStage::Attempted)) {
        return fail(Error::Admission);
    }
    {
        Operation committing(committing_);
        Operation active(effectScope_);
        result_.journal = bundle_.auxiliary(request, *this, reservedIntents_ - 1);
    }
    const auto outcome = result_.journal.outcome;
    if (outcome == rules_bundle::Outcome::Unknown) {
        phase_ = Phase::Unknown;
        recovered_ = false;
        return fail(Error::Recovery);
    }
    if (outcome != rules_bundle::Outcome::Committed &&
        outcome != rules_bundle::Outcome::Duplicate) {
        const auto after = journal_.snapshot();
        bool present = false;
        for (std::size_t i = 0; i < bundleWork_.records.count; ++i)
            present |= bundleWork_.records.data[i].record.operationId == request.operationId;
        // Sole tightly qualified exception: actual Denied is before
        // reserveRecord. Generic Stale/Conflict/IO/Unknown never reset a phase.
        if (result_.journal.journal.outcome == ProgressOutcome::Denied && after.ready &&
            !after.poisoned && !after.operating && after.revision == physicalBefore.revision &&
            after.records == physicalBefore.records &&
            after.lastIssued == physicalBefore.lastIssued && !present && newlyCaptured &&
            work_.physicalBytes.count == beforeBytes + encoded.count) {
            work_.physicalBytes.count = beforeBytes;
            encoded = {};
            request = {};
            phase_ = phase == EffectPhase::Claim ? Phase::Ready : Phase::Dispatching;
            return fail(Error::Stale);
        }
        if (outcome == rules_bundle::Outcome::Deferred)
            return fail(Error::Capacity);
        phase_ = Phase::Fault;
        recovered_ = false;
        return fail(Error::Journal);
    }
    if (!workspace() || !bundleWork_.records.count ||
        !decodeEffect(encoded.data, encoded.count, pendingEffect_).valid) {
        phase_ = Phase::Unknown;
        recovered_ = false;
        return fail(Error::Recovery);
    }
    const auto &record = bundleWork_.records.data[bundleWork_.records.count - 1].record;
    if (!validateEffectRecord(pendingEffect_, record, intent.reservation).valid ||
        record.operationId != request.operationId ||
        record.requestDigest != request.requestDigest) {
        phase_ = Phase::Unknown;
        recovered_ = false;
        return fail(Error::Recovery);
    }
    if (phase == EffectPhase::Claim) {
        intent.originalClaimFence = pendingEffect_.expected;
        intent.originalClaimBoot = pendingEffect_.ownerBoot;
        intent.stage = IntentStage::Attempted;
    } else {
        intent.resultBefore = live_.fence;
        intent.resultCommitted = true;
        intent.settlementRequired = false;
        intent.stage = handlerResult == HandlerResult::Returned ? IntentStage::Returned
                       : handlerResult == HandlerResult::Failed ? IntentStage::Failed
                                                                : IntentStage::Unknown;
    }
    --reservedIntents_;
    result_.error = Error::None;
    result_.phase = phase_;
    return result_;
}

const ProgressRecord *Owner::record(const rules::Identity &id) const {
    for (std::size_t i = 0; i < bundleWork_.records.count; ++i)
        if (!std::memcmp(bundleWork_.records.data[i].record.operationId.data(), id.bytes, 16))
            return &bundleWork_.records.data[i].record;
    return nullptr;
}
Result Owner::dispatch(const rules::Identity &sourceApplication, std::uint16_t sourceIndex) {
    const auto application = sourceApplication;
    const auto index = sourceIndex;
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    if (!recovered_ || !activated_ || !workspace() ||
        (phase_ != Phase::Ready && phase_ != Phase::Dispatching) ||
        live_.mode != rules::Mode::Running)
        return fail(Error::Recovery);
    std::size_t target = work_.intents.count;
    for (std::size_t i = 0; i < work_.intents.count; ++i)
        if (bytesEqual(work_.intents.data[i].application, application) &&
            work_.intents.data[i].reservation.index == index)
            target = i;
    if (target == work_.intents.count)
        return fail(Error::Argument);
    auto &intent = work_.intents.data[target];
    if (intent.resultCommitted) {
        result_.journal.outcome = rules_bundle::Outcome::Duplicate;
        result_.phase = phase_;
        return result_;
    }
    if (intent.stage == IntentStage::Unknown)
        return fail(Error::Recovery); // Never automatic physical retry.
    if (phase_ == Phase::Dispatching && (dispatching_ != target || !ordered_))
        return fail(Error::Conflict);
    if (phase_ != Phase::Dispatching) {
        // This first synchronous owner uses a deterministic FIFO handoff queue.
        // Ineligible earlier work is blocked, never silently skipped/rebound.
        for (std::size_t i = 0; i < target; ++i)
            if (work_.intents.data[i].stage == IntentStage::Pending)
                return fail(Error::Conflict);
        bool loaded = false;
        for (std::size_t i = 0; i < work_.applications.count; ++i) {
            const auto &entry = work_.applications.data[i];
            if (bytesEqual(entry.operation, application)) {
                loaded = loadApplication(entry);
                break;
            }
        }
        if (!loaded || !bytesEqual(intent.catalog, catalog_) ||
            !bytesEqual(work_.decoded.catalog, catalog_) ||
            index >= work_.decoded.plan.intents.count || index >= work_.envelope.intents.count)
            return fail(Error::Recovery, true);
        const auto &reservation = work_.envelope.intents.data[index];
        if (reservation.index != intent.reservation.index ||
            !bytesEqual(reservation.claim, intent.reservation.claim) ||
            !bytesEqual(reservation.result, intent.reservation.result))
            return fail(Error::Conflict, true);
        if (!physicalCapacity(0))
            return fail(Error::Capacity);
        // The exact raw Claim commitment is captured before acquiring its
        // semantic metadata lease inside effect(), never an unbound ID hold.
        dispatching_ = target;
        phase_ = Phase::Dispatching;
    }
    return finishDispatch();
}

Result Owner::settleUnknown(const rules::Identity &sourceApplication, std::uint16_t sourceIndex) {
    const auto application = sourceApplication;
    const auto index = sourceIndex;
    if (operating_) {
        Result busy;
        busy.phase = phase_;
        busy.error = Error::Busy;
        return busy;
    }
    Operation guard(operating_);
    result_ = {};
    if (!recovered_ || !activated_ || !workspace() ||
        (phase_ != Phase::Ready && phase_ != Phase::Stopped && phase_ != Phase::Dispatching))
        return fail(Error::Recovery);
    std::size_t target = work_.intents.count;
    for (std::size_t i = 0; i < work_.intents.count; ++i)
        if (bytesEqual(work_.intents.data[i].application, application) &&
            work_.intents.data[i].reservation.index == index)
            target = i;
    if (target == work_.intents.count)
        return fail(Error::Argument);
    auto &intent = work_.intents.data[target];
    if (intent.resultCommitted) {
        result_.journal.outcome = rules_bundle::Outcome::Duplicate;
        result_.phase = phase_;
        return result_;
    }
    if (intent.stage != IntentStage::Unknown || !intent.settlementRequired ||
        !nonzero(intent.restoredBy))
        return fail(Error::Admission);
    if (phase_ == Phase::Dispatching && (!ordered_ || dispatching_ != target))
        return fail(Error::Conflict);
    if (phase_ != Phase::Dispatching) {
        bool loaded = false, restored = false;
        for (std::size_t i = 0; i < work_.controls.count; ++i)
            restored |= work_.controls.data[i].kind == ControlKind::RestoreStopped &&
                        bytesEqual(work_.controls.data[i].operation, intent.restoredBy);
        for (std::size_t i = 0; i < work_.applications.count; ++i)
            if (bytesEqual(work_.applications.data[i].operation, application)) {
                loaded = loadApplication(work_.applications.data[i]);
                break;
            }
        if (!restored || !loaded || !physicalCapacity(0))
            return fail(Error::Admission);
        // Settlement acquires its exact frozen Result binding in effect().
        dispatching_ = target;
        phase_ = Phase::Dispatching;
    }
    return finishDispatch();
}

Result Owner::finishDispatch() {
    if (!operating_ || phase_ != Phase::Dispatching || !workspace() ||
        dispatching_ >= work_.intents.count)
        return fail(Error::Argument, true);
    const auto target = dispatching_;
    auto &intent = work_.intents.data[target];
    const auto application = intent.application;
    const auto index = intent.reservation.index;
    const bool settlement = intent.stage == IntentStage::Unknown && intent.settlementRequired &&
                            nonzero(intent.restoredBy);
    if (intent.stage == IntentStage::Pending) {
        const auto claim = effect(target, EffectPhase::Claim, HandlerResult::None);
        if (claim.error != Error::None) {
            if (phase_ == Phase::Ready) {
                if (ordered_ && !releaseOrdering(ProgressOutcome::Denied))
                    return fail(Error::Recovery, true);
                dispatching_ = SIZE_MAX;
            }
            return claim;
        }
        const auto *acceptedClaim = record(intent.reservation.claim);
        if (!acceptedClaim) {
            phase_ = Phase::Unknown;
            recovered_ = false;
            return fail(Error::Recovery);
        }
        const auto &savedIntent = work_.decoded.plan.intents.data[index];
        // Durable claim precedes the last original-source eligibility check and
        // the single actual handoff. No new actor may adopt an old plan.
        const auto eligibility = intentEligibility_.check(
            work_.historicalProgram, installed_, live_, work_.envelope, work_.decoded,
            intent.reservation, savedIntent, pendingEffect_, *acceptedClaim);
        intent.handlerResult = HandlerResult::Unknown;
        if (eligibility == IntentEligibility::Current && finalClock()) {
            const auto returned = handler_.deliver(live_.context, application, index, savedIntent);
            if (returned == HandlerResult::Returned || returned == HandlerResult::Failed)
                intent.handlerResult = returned;
        }
    } else if (!settlement && (intent.stage != IntentStage::Attempted ||
                               intent.handlerResult == HandlerResult::None)) {
        return fail(Error::Recovery, true);
    }
    const auto completed =
        effect(target, EffectPhase::Result,
               settlement ? HandlerResult::Unknown : intent.handlerResult, settlement);
    if (completed.error != Error::None)
        return completed; // Unknown preserves hold and never calls handler again.
    ordered_ = false;
    if (!releaseOrdering(ProgressOutcome::Committed))
        return fail(Error::Recovery, true);
    dispatching_ = SIZE_MAX;
    phase_ = live_.mode == rules::Mode::Stopped ? Phase::Stopped : Phase::Ready;
    // A slow actual handler may have exceeded the checkpoint target. Its
    // acknowledgment is still durable truth, but no further live input proceeds.
    ClockWitness after;
    rules::State ignored;
    if (!clock(after, ignored))
        return fail(clockError_, true);
    result_.phase = phase_;
    return result_;
}

bool Owner::acquire(const CommitBinding &input) {
    const auto binding = input;
    OrderingLease candidate;
    if (ordering_.hold(binding, candidate) != Ordering::Held)
        return false;
    lease_ = candidate;
    ordered_ = true;
    heldOperation_ = binding.operation;
    if (!nonzero(candidate.boot) || !candidate.generation ||
        !sameCommitBinding(candidate.binding, binding)) {
        fail(Error::Recovery, true);
        return false;
    }
    return true;
}
bool Owner::releaseOrdering(ProgressOutcome outcome) {
    if (ordering_.resolved(lease_, outcome) != OrderingResolution::Resolved) {
        ordered_ = true;
        pendingDefinitelyNotCommitted_ = false;
        return false;
    }
    ordered_ = false;
    lease_ = {};
    return true;
}

bool Owner::recoveryResolution(ProgressOutcome &outcome) {
    if (recoveryOrdering_.inventory == OrderingInventory::Empty) {
        outcome = ProgressOutcome::Recovered;
        return true;
    }
    if (recoveryOrdering_.inventory != OrderingInventory::Held)
        return false;
    const auto &binding = recoveryOrdering_.lease.binding;
    if (!nonzero(binding.operation) || !rules::sameContext(binding.context, installed_.context))
        return false;
    CommitBinding actual;
    actual.kind = binding.kind;
    actual.operation = binding.operation;
    actual.context = installed_.context;
    if (binding.kind == CommitKind::Application) {
        for (std::size_t i = 0; i < work_.applications.count; ++i) {
            const auto &entry = work_.applications.data[i];
            if (!bytesEqual(entry.operation, binding.operation))
                continue;
            actual.expected = entry.before;
            actual.request = entry.request;
            actual.catalog = entry.catalog;
            actual.commitment = entry.blob;
            if (!sameCommitBinding(actual, binding))
                return false;
            outcome = ProgressOutcome::Committed;
            return true;
        }
    } else if (binding.kind == CommitKind::Control) {
        for (std::size_t i = 0; i < work_.controls.count; ++i) {
            const auto &entry = work_.controls.data[i];
            if (!bytesEqual(entry.operation, binding.operation))
                continue;
            actual.expected = entry.before;
            actual.request = entry.request;
            actual.catalog = entry.catalog;
            actual.commitment = entry.raw;
            if (!sameCommitBinding(actual, binding))
                return false;
            outcome = ProgressOutcome::Committed;
            return true;
        }
    } else if (binding.kind == CommitKind::Claim || binding.kind == CommitKind::Result) {
        const bool claim = binding.kind == CommitKind::Claim;
        for (std::size_t i = 0; i < work_.intents.count; ++i) {
            const auto &intent = work_.intents.data[i];
            const auto &operation = claim ? intent.reservation.claim : intent.reservation.result;
            if (!bytesEqual(operation, binding.operation))
                continue;
            actual.parent = intent.application;
            actual.catalog = intent.catalog;
            bool parent = false;
            for (std::size_t j = 0; j < work_.applications.count; ++j)
                if (bytesEqual(work_.applications.data[j].operation, intent.application)) {
                    actual.request = work_.applications.data[j].request;
                    parent = true;
                    break;
                }
            if (!parent || !bytesEqual(actual.parent, binding.parent) ||
                !bytesEqual(actual.request, binding.request) ||
                !bytesEqual(actual.catalog, binding.catalog))
                return false;
            const auto *physical = record(operation);
            if (physical) {
                if (physical->kind !=
                    (claim ? ProgressKind::EffectClaim : ProgressKind::EffectResult))
                    return false;
                actual.expected = claim ? intent.originalClaimFence : intent.resultBefore;
                std::memcpy(actual.commitment.bytes, physical->requestDigest.data(), 32);
                if (!sameCommitBinding(actual, binding))
                    return false;
                outcome = ProgressOutcome::Committed;
                return true;
            }
            if (!rules::sameFence(binding.expected, rebuilt_.fence))
                return false;
            outcome = ProgressOutcome::Denied;
            return true;
        }
        return false; // A foreign/unreserved phase is incoherent, not empty.
    } else
        return false;
    // A record of another semantic kind with this ID is a conflict, never a
    // success and never proof of absence. Catalog finals alone remain inert.
    if (record(binding.operation))
        return false;
    for (std::size_t i = 0; i < work_.intents.count; ++i)
        if (bytesEqual(binding.operation, work_.intents.data[i].reservation.claim) ||
            bytesEqual(binding.operation, work_.intents.data[i].reservation.result))
            return false;
    for (std::size_t i = 0; i < bundleWork_.bundles.count; ++i) {
        const auto &descriptor = bundleWork_.bundles.data[i];
        if (std::memcmp(descriptor.key.operation.data(), binding.operation.bytes, 16))
            continue;
        if (binding.kind != CommitKind::Application || descriptor.final ||
            descriptor.key.kind != rules_bundle::Kind::Application ||
            std::memcmp(descriptor.key.request.data(), binding.request.bytes, 32) ||
            std::memcmp(descriptor.key.catalog.data(), binding.catalog.bytes, 32) ||
            std::memcmp(descriptor.key.blob.data(), binding.commitment.bytes, 32) ||
            std::memcmp(descriptor.key.deployment.data(), binding.context.deployment.bytes, 32))
            return false;
    }
    if (!rules::sameFence(binding.expected, rebuilt_.fence))
        return false;
    outcome = ProgressOutcome::Denied;
    return true;
}

} // namespace rules_progress
} // namespace devices
} // namespace larp
