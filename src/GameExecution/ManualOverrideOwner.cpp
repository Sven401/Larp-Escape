#include "GameExecution/ManualOverrideOwner.h"
#include "GameExecution/RulesProgressCodec.h"
#include <cstring>
#include <limits>

namespace larp {
namespace execution {
namespace rules {
namespace {
struct Operation {
    bool &busy;
    explicit Operation(bool &b) : busy(b) {
        busy = true;
    }
    ~Operation() {
        busy = false;
    }
};
bool nz(const Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
bool eq(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool eq(const Digest &a, const Digest &b) {
    return !std::memcmp(a.bytes, b.bytes, 32);
}
bool eq(const OverrideAuthority &a, const OverrideAuthority &b) {
    return eq(a.peer, b.peer) && a.principal == b.principal && a.roleEpoch == b.roleEpoch &&
           a.connectionToken == b.connectionToken && a.connectionHandle == b.connectionHandle &&
           a.registration == b.registration && a.lifecycle == b.lifecycle &&
           a.registryRevision == b.registryRevision && a.eventRevision == b.eventRevision;
}
bool actor(const OverrideAuthority &a) {
    return nz(a.peer) && a.principal && a.roleEpoch && a.connectionToken && a.registration &&
           a.lifecycle && a.registryRevision && a.eventRevision;
}
bool active(ManualStatus s) {
    return s == ManualStatus::Prepared || s == ManualStatus::Confirming ||
           s == ManualStatus::Unknown;
}
ManualResult failure(ManualError error, bool deferred = false) {
    ManualResult r;
    r.status = deferred ? ManualStatus::Deferred : ManualStatus::Rejected;
    r.error = error;
    return r;
}
ManualResult result(const ManualReplay &entry) {
    ManualResult r;
    r.status = entry.status;
    r.error = entry.error;
    r.release = entry.release;
    r.token = entry.record.token;
    r.operation = entry.record.operation;
    r.expiresAt = entry.record.expiresAt;
    return r;
}
ManualResult denied(ManualAuthorityResult state) {
    return failure(state == ManualAuthorityResult::Unavailable ? ManualError::Unavailable
                   : state == ManualAuthorityResult::Stale     ? ManualError::Stale
                                                               : ManualError::Authority,
                   state == ManualAuthorityResult::Unavailable);
}
} // namespace
ManualOverrideOwner::ManualOverrideOwner(
    const Program &p, const State &s, const Identity &boot, std::uint64_t lifetime,
    configuration::IPackageDigest &hash, IManualClock &clock, IManualAuthority &authority,
    IManualIdentitySource &identities, IManualTransaction &transaction, ManualWorkspace &workspace)
    : program_(p), state_(s), boot_(boot), lifetime_(lifetime), hash_(hash), clock_(clock),
      authority_(authority), ids_(identities), transaction_(transaction), work_(workspace) {}
bool ManualOverrideOwner::validWorkspace() const {
    if (!nz(boot_) || !lifetime_ || !work_.maximumActive ||
        work_.maximumActive > work_.replays.capacity ||
        work_.replays.count > work_.replays.capacity || work_.replays.capacity > 65534 ||
        !work_.replays.data ||
        reinterpret_cast<std::uintptr_t>(work_.replays.data) % alignof(ManualReplay))
        return false;
    if (!work_.catalogBytes.data || work_.catalogBytes.count > work_.catalogBytes.capacity ||
        work_.catalogBytes.capacity > 32768)
        return false;
    const auto first = reinterpret_cast<std::uintptr_t>(work_.catalogBytes.data);
    const auto records = reinterpret_cast<std::uintptr_t>(work_.replays.data);
    if (work_.catalogBytes.capacity > std::numeric_limits<std::uintptr_t>::max() - first ||
        work_.replays.capacity * sizeof(ManualReplay) >
            std::numeric_limits<std::uintptr_t>::max() - records ||
        (first < records + work_.replays.capacity * sizeof(ManualReplay) &&
         records < first + work_.catalogBytes.capacity))
        return false;
    for (std::size_t i = 0; i < work_.replays.count; ++i)
        if (!eq(work_.replays.data[i].record.ownerBoot, boot_))
            return false;
    return true;
}
bool ManualOverrideOwner::target(const Identity &rule, Id &original, Id &replacement) const {
    original = replacement = 0;
    if (!program_.ready || program_.language != 2 || !program_.manualOverride ||
        !sameContext(program_.context, state_.context))
        return false;
    for (std::size_t i = 0; i < program_.rules.count; ++i) {
        const auto &r = program_.rules.data[i];
        if (!eq(r.stable, rule) || r.ir.policy != 3)
            continue;
        if (original || r.ir.group != i + 1 || !r.ir.fallback ||
            r.ir.fallback > program_.rules.count)
            return false;
        const auto &other = program_.rules.data[r.ir.fallback - 1];
        if (other.ir.group != i + 1 || !eq(other.stable, rule) ||
            !configuration::module_ir::manualReleasePair(program_.actions, r.ir, other.ir))
            return false;
        original = static_cast<Id>(i + 1);
        replacement = r.ir.fallback;
    }
    return original && replacement;
}
bool ManualOverrideOwner::target(const OverrideRecord &record) const {
    Id original = 0, replacement = 0;
    return target(record.rule, original, replacement) && original == record.original &&
           replacement == record.replacement &&
           program_.rules.data[original - 1].ir.generation == record.originalGeneration &&
           program_.rules.data[replacement - 1].ir.generation == record.replacementGeneration;
}
bool ManualOverrideOwner::released(const OverrideRecord &record) const {
    for (std::size_t i = 0; i < state_.latches.count; ++i)
        if (eq(state_.latches.data[i].rule, record.rule))
            return true;
    return false;
}
bool ManualOverrideOwner::unique(const Identity &id) const {
    if (!nz(id) || eq(id, boot_))
        return false;
    for (std::size_t i = 0; i < work_.replays.count; ++i) {
        const auto &r = work_.replays.data[i].record;
        if (eq(id, r.token) || eq(id, r.operation) || eq(id, r.prepareId) || eq(id, r.confirmId))
            return false;
    }
    return true;
}
bool ManualOverrideOwner::digest(const OverrideRecord &r, unsigned purpose, Digest &out) {
    out = {};
    bool ok = hash_.abort();
    if (ok)
        ok = hash_.begin();
    const auto bytes = [&](const void *p, std::size_t n) {
        if (ok)
            ok = hash_.update(static_cast<const std::uint8_t *>(p), n);
    };
    const auto u = [&](std::uint64_t n) {
        std::uint8_t b[8];
        for (unsigned i = 0; i < 8; ++i)
            b[7 - i] = static_cast<std::uint8_t>(n >> (8 * i));
        bytes(b, 8);
    };
    const char domain[] = "larp.manual.owner.v2";
    bytes(domain, sizeof(domain) - 1);
    u(purpose);
    bytes(r.context.event.bytes, 16);
    bytes(r.context.run.bytes, 16);
    bytes(r.context.station.bytes, 16);
    bytes(r.context.box.bytes, 16);
    bytes(r.context.sourceEpoch.bytes, 16);
    bytes(r.program.bytes, 32);
    bytes(r.context.deployment.bytes, 32);
    bytes(r.context.binding.bytes, 32);
    bytes(r.context.content.bytes, 32);
    u(r.context.bindingGeneration);
    u(r.expected.progress);
    u(r.expected.profile);
    u(r.expected.trust);
    u(r.expected.facts);
    u(r.expected.ledger);
    u(r.expected.resources);
    u(r.expected.rights);
    u(r.expected.capacity);
    u(r.expected.lastIssued);
    // View binds logical fences; a persisted clock-only checkpoint is not a
    // new decision. Frozen Prepare/Confirm digests still retain original time.
    u(purpose ? r.expected.activeTime : 0);
    bytes(r.authority.peer.bytes, 16);
    u(r.authority.principal);
    u(r.authority.roleEpoch);
    u(r.authority.connectionToken);
    u(r.authority.connectionHandle);
    u(r.authority.registration);
    u(r.authority.lifecycle);
    u(r.authority.registryRevision);
    u(r.authority.eventRevision);
    bytes(r.ownerBoot.bytes, 16);
    bytes(r.rule.bytes, 16);
    u(r.original);
    u(r.replacement);
    u(r.originalGeneration);
    u(r.replacementGeneration);
    if (purpose)
        bytes(r.prepareId.bytes, 16);
    if (purpose == 2) {
        bytes(r.token.bytes, 16);
        bytes(r.confirmId.bytes, 16);
        bytes(r.operation.bytes, 16);
        bytes(r.prepareDigest.bytes, 32);
        u(r.preparedAt);
        u(r.expiresAt);
        u(r.confirmedAt);
    }
    std::size_t written = 0;
    if (ok)
        ok = hash_.finish(out.bytes, written) && written == 32;
    const bool closed = hash_.abort();
    if (!ok || !closed) {
        out = {};
        return false;
    }
    return true;
}
void ManualOverrideOwner::retire(std::uint64_t now) {
    Digest current;
    const bool fingerprintAvailable = fingerprint(current);
    for (std::size_t i = 0; i < work_.replays.count; ++i) {
        auto &entry = work_.replays.data[i];
        auto &r = entry.record;
        // Pending/Unknown belongs to the transaction until qualified resolution.
        if (entry.status != ManualStatus::Prepared)
            continue;
        if (now > r.expiresAt) {
            entry.status = ManualStatus::Expired;
            entry.error = ManualError::Expired;
        } else if (!sameContext(r.context, program_.context) || !target(r) ||
                   !overridePlanningFence(r.expected, state_.fence) ||
                   (fingerprintAvailable && !eq(current, r.program))) {
            entry.status = ManualStatus::Rejected;
            entry.error = ManualError::Stale;
        } else {
            const auto checked = authority_.check(r.context, r.authority);
            if (checked == ManualAuthorityResult::Closed ||
                checked == ManualAuthorityResult::Stale) {
                entry.status = ManualStatus::Rejected;
                entry.error = ManualError::Authority;
            }
        }
    }
}
bool ManualOverrideOwner::unknown() const {
    for (std::size_t i = 0; i < work_.replays.count; ++i)
        if (work_.replays.data[i].status == ManualStatus::Unknown)
            return true;
    return false;
}
bool ManualOverrideOwner::fingerprint(Digest &out) {
    out = {};
    if (rules_progress::encodeCatalog(program_, work_.catalogBytes).scope !=
        rules_progress::Scope::EncodedUnpublished)
        return false;
    bool ok = hash_.abort();
    if (ok)
        ok = hash_.begin();
    if (ok)
        ok = hash_.update(work_.catalogBytes.data, work_.catalogBytes.count);
    std::size_t written = 0;
    if (ok)
        ok = hash_.finish(out.bytes, written) && written == 32;
    const bool closed = hash_.abort();
    if (!ok || !closed) {
        out = {};
        return false;
    }
    return true;
}
ManualAuthorityResult ManualOverrideOwner::check(const OverrideRecord &record) {
    if (!operating_ || !submitting_ || submitting_ > work_.replays.count ||
        work_.replays.data[submitting_ - 1].status != ManualStatus::Confirming ||
        !sameOverrideRecord(work_.replays.data[submitting_ - 1].record, record))
        return ManualAuthorityResult::Stale;
    return validate(record);
}
ManualAuthorityResult ManualOverrideOwner::validate(const OverrideRecord &record) {
    Digest catalog;
    if (!fingerprint(catalog))
        return ManualAuthorityResult::Unavailable;
    if (!eq(catalog, record.program))
        return ManualAuthorityResult::Stale;
    std::uint64_t now = 0;
    if (!clock_.now(now) || now < now_)
        return ManualAuthorityResult::Unavailable;
    now_ = now;
    if (!eq(record.ownerBoot, boot_) || !sameContext(record.context, program_.context) ||
        !sameContext(record.context, state_.context) ||
        !overridePlanningFence(record.expected, state_.fence) || !target(record) ||
        state_.mode != Mode::Running || now > record.expiresAt)
        return ManualAuthorityResult::Stale;
    const auto authority = authority_.check(record.context, record.authority);
    if (authority != ManualAuthorityResult::Current)
        return authority;
    if (!fingerprint(catalog))
        return ManualAuthorityResult::Unavailable;
    if (!eq(catalog, record.program))
        return ManualAuthorityResult::Stale;
    if (!clock_.now(now) || now < now_)
        return ManualAuthorityResult::Unavailable;
    now_ = now;
    if (now > record.expiresAt || !sameContext(record.context, program_.context) ||
        !sameContext(record.context, state_.context) ||
        !overridePlanningFence(record.expected, state_.fence) || !target(record))
        return ManualAuthorityResult::Stale;
    return ManualAuthorityResult::Current;
}
ManualResult ManualOverrideOwner::view(const Identity &requested,
                                       const OverrideAuthority &actorRequest, ManualView &output) {
    const Identity rule = requested;
    const OverrideAuthority authority = actorRequest;
    if (operating_)
        return failure(ManualError::Busy, true);
    Operation operation(operating_);
    output = {};
    if (!validWorkspace() || !actor(authority))
        return failure(ManualError::InvalidArgument);
    if (unknown())
        return failure(ManualError::RecoveryRequired, true);
    OverrideRecord r;
    r.context = program_.context;
    r.expected = state_.fence;
    r.authority = authority;
    r.ownerBoot = boot_;
    r.rule = rule;
    if (!target(rule, r.original, r.replacement))
        return failure(ManualError::Context);
    r.originalGeneration = program_.rules.data[r.original - 1].ir.generation;
    r.replacementGeneration = program_.rules.data[r.replacement - 1].ir.generation;
    if (!fingerprint(r.program))
        return failure(ManualError::Crypto);
    auto allowed = authority_.check(r.context, authority);
    if (allowed != ManualAuthorityResult::Current)
        return denied(allowed);
    Digest hashed;
    if (!digest(r, 0, hashed))
        return failure(ManualError::Crypto);
    if (!sameContext(r.context, program_.context) ||
        !overridePlanningFence(r.expected, state_.fence) || !target(r))
        return failure(ManualError::Stale);
    output.ownerBoot = boot_;
    output.rule = rule;
    output.digest = hashed;
    ManualResult answer;
    answer.status = ManualStatus::Prepared;
    return answer;
}
ManualResult ManualOverrideOwner::prepare(const ManualPrepareRequest &source) {
    const ManualPrepareRequest request = source;
    if (operating_)
        return failure(ManualError::Busy, true);
    Operation operation(operating_);
    if (!validWorkspace() || !eq(request.ownerBoot, boot_) || !nz(request.phase) ||
        !actor(request.authority))
        return failure(ManualError::InvalidArgument);
    if (unknown())
        return failure(ManualError::RecoveryRequired, true);
    std::uint64_t now = 0;
    if (!clock_.now(now) || now < now_)
        return failure(ManualError::Unavailable, true);
    now_ = now;
    OverrideRecord record;
    record.context = program_.context;
    record.expected = state_.fence;
    record.authority = request.authority;
    record.ownerBoot = boot_;
    record.rule = request.rule;
    record.prepareId = request.phase;
    if (!target(request.rule, record.original, record.replacement))
        return failure(ManualError::Context);
    if (!fingerprint(record.program))
        return failure(ManualError::Crypto);
    record.originalGeneration = program_.rules.data[record.original - 1].ir.generation;
    record.replacementGeneration = program_.rules.data[record.replacement - 1].ir.generation;
    auto allowed = authority_.check(record.context, request.authority);
    if (allowed != ManualAuthorityResult::Current)
        return denied(allowed);
    // A retransmitted phase identity is compared to the ORIGINAL request, not a
    // freshly generated view/time. It never creates a refreshed grant/deadline.
    for (std::size_t i = 0; i < work_.replays.count; ++i) {
        auto &entry = work_.replays.data[i];
        const auto &old = entry.record;
        if (!eq(old.prepareId, request.phase))
            continue;
        Digest viewDigest;
        if (!digest(old, 0, viewDigest))
            return failure(ManualError::Crypto);
        if (!eq(old.ownerBoot, request.ownerBoot) || !eq(old.rule, request.rule) ||
            !eq(old.authority, request.authority) || !eq(viewDigest, request.view))
            return failure(ManualError::Conflict);
        if (!sameContext(old.context, program_.context))
            return failure(ManualError::Context);
        retire(now);
        return result(entry);
    }
    Digest viewDigest;
    if (!digest(record, 0, viewDigest))
        return failure(ManualError::Crypto);
    if (!eq(viewDigest, request.view))
        return failure(ManualError::Stale);
    if (released(record)) {
        ManualResult answer;
        answer.status = ManualStatus::AlreadyReleased;
        answer.release = ReleaseDisposition::Released;
        return answer;
    }
    retire(now);
    std::size_t count = 0;
    for (std::size_t i = 0; i < work_.replays.count; ++i)
        count += active(work_.replays.data[i].status);
    if (count >= work_.maximumActive || work_.replays.count == work_.replays.capacity)
        return failure(ManualError::Capacity, true);
    if (!unique(request.phase) || now > std::numeric_limits<std::uint64_t>::max() - lifetime_)
        return failure(ManualError::Identity);
    record.preparedAt = now;
    record.expiresAt = now + lifetime_;
    if (!ids_.make(record.token) || !unique(record.token) || eq(record.token, request.phase))
        return failure(ManualError::Identity);
    if (!digest(record, 1, record.prepareDigest))
        return failure(ManualError::Crypto);
    allowed = validate(record);
    if (allowed != ManualAuthorityResult::Current)
        return denied(allowed);
    auto &entry = work_.replays.data[work_.replays.count++];
    entry = {};
    entry.record = record;
    return result(entry);
}
ManualResult ManualOverrideOwner::confirm(const ManualConfirmRequest &source) {
    const ManualConfirmRequest request = source;
    if (operating_)
        return failure(ManualError::Busy, true);
    Operation operation(operating_);
    if (!validWorkspace() || !eq(request.ownerBoot, boot_) || !nz(request.phase) ||
        !actor(request.authority))
        return failure(ManualError::InvalidArgument);
    std::uint64_t now = 0;
    if (!clock_.now(now) || now < now_)
        return failure(ManualError::Unavailable, true);
    now_ = now;
    ManualReplay *entry = nullptr;
    for (std::size_t i = 0; i < work_.replays.count; ++i)
        if (eq(work_.replays.data[i].record.token, request.token))
            entry = &work_.replays.data[i];
    if (!entry)
        return failure(ManualError::UnknownOperation);
    auto &r = entry->record;
    if (!eq(r.authority, request.authority))
        return failure(ManualError::Conflict);
    if (!sameContext(r.context, program_.context) || !sameContext(r.context, state_.context) ||
        !target(r))
        return failure(ManualError::Context);
    auto allowed = authority_.check(r.context, request.authority);
    if (allowed != ManualAuthorityResult::Current)
        return denied(allowed);
    Digest currentProgram;
    if (!fingerprint(currentProgram))
        return failure(ManualError::Crypto);
    if (!eq(currentProgram, r.program)) {
        if (entry->status == ManualStatus::Prepared) {
            entry->status = ManualStatus::Rejected;
            entry->error = ManualError::Stale;
        }
        return failure(ManualError::Stale);
    }
    if (nz(r.confirmId) && !eq(r.confirmId, request.phase))
        return failure(ManualError::Conflict);
    if (entry->status != ManualStatus::Prepared && entry->status != ManualStatus::Confirming)
        return result(*entry);
    if (!clock_.now(now) || now < now_)
        return failure(ManualError::Unavailable, true);
    now_ = now;
    if (unknown())
        return failure(ManualError::RecoveryRequired, true);
    if (now > r.expiresAt) {
        entry->status = ManualStatus::Expired;
        entry->error = ManualError::Expired;
        return result(*entry);
    }
    if (released(r)) {
        if (!nz(r.confirmId)) {
            if (!unique(request.phase))
                return failure(ManualError::Conflict);
            OverrideRecord received = r;
            received.confirmId = request.phase;
            received.confirmedAt = now;
            if (!digest(received, 2, received.confirmDigest))
                return failure(ManualError::Crypto);
            r = received;
        }
        entry->status = ManualStatus::AlreadyReleased;
        entry->release = ReleaseDisposition::Released;
        entry->error = ManualError::None;
        return result(*entry);
    }
    if (entry->status != ManualStatus::Prepared && entry->status != ManualStatus::Confirming)
        return result(*entry);
    if (now > r.expiresAt) {
        entry->status = ManualStatus::Expired;
        entry->error = ManualError::Expired;
        return result(*entry);
    }
    allowed = validate(r);
    if (allowed != ManualAuthorityResult::Current) {
        entry->status = ManualStatus::Rejected;
        entry->error = ManualError::Stale;
        return denied(allowed);
    }
    if (!nz(r.confirmId)) {
        if (!unique(request.phase))
            return failure(ManualError::Conflict);
        OverrideRecord confirmed = r;
        confirmed.confirmId = request.phase;
        confirmed.confirmedAt = now;
        if (!ids_.make(confirmed.operation) || !unique(confirmed.operation) ||
            eq(confirmed.operation, request.phase))
            return failure(ManualError::Identity);
        if (!digest(confirmed, 2, confirmed.confirmDigest))
            return failure(ManualError::Crypto);
        allowed = validate(confirmed);
        if (allowed != ManualAuthorityResult::Current)
            return denied(allowed);
        r = confirmed;
        entry->status = ManualStatus::Confirming;
    }
    const OverrideAdmission admission(r);
    submitting_ = static_cast<Id>(entry - work_.replays.data + 1);
    const ManualResult transaction = transaction_.submit(admission, *this);
    submitting_ = 0;
    entry->release = (transaction.status == ManualStatus::Committed ||
                      transaction.status == ManualStatus::Duplicate)
                         ? transaction.release
                     : transaction.status == ManualStatus::NotCommitted
                         ? ReleaseDisposition::NoRelease
                         : ReleaseDisposition::Unresolved;
    // A backend cannot change the semantic operation/token or extend its lease.
    // An incoherent response is UNKNOWN, never a claim that no write happened.
    if (!eq(transaction.operation, r.operation)) {
        entry->status = ManualStatus::Unknown;
        entry->release = ReleaseDisposition::Unresolved;
        entry->error = ManualError::Transaction;
        return result(*entry);
    }
    switch (transaction.status) {
    case ManualStatus::Committed:
    case ManualStatus::Duplicate:
    case ManualStatus::Unknown:
    case ManualStatus::NotCommitted:
        entry->status = transaction.status;
        entry->error = transaction.error;
        break;
    case ManualStatus::Deferred:
    case ManualStatus::Confirming:
        break;
    case ManualStatus::Rejected:
        entry->status = ManualStatus::Rejected;
        entry->error = transaction.error;
        break;
    default:
        entry->status = ManualStatus::Unknown;
        entry->release = ReleaseDisposition::Unresolved;
        entry->error = ManualError::Transaction;
        break;
    }
    return result(*entry);
}
ManualResult ManualOverrideOwner::status(const Context &source,
                                         const OverrideAuthority &actorSource,
                                         const Identity &operationSource) {
    const Context context = source;
    const OverrideAuthority authority = actorSource;
    const Identity id = operationSource;
    if (operating_)
        return failure(ManualError::Busy, true);
    Operation operation(operating_);
    if (!validWorkspace() || !actor(authority) || !nz(id))
        return failure(ManualError::InvalidArgument);
    if (!sameContext(context, program_.context) || !sameContext(context, state_.context))
        return failure(ManualError::Context);
    const auto allowed = authority_.check(context, authority);
    if (allowed != ManualAuthorityResult::Current)
        return denied(allowed);
    ManualReplay *known = nullptr;
    for (std::size_t i = 0; i < work_.replays.count; ++i)
        if (eq(work_.replays.data[i].record.operation, id))
            known = &work_.replays.data[i];
    if (known && known->status != ManualStatus::Confirming &&
        known->status != ManualStatus::Unknown)
        return result(*known); // A terminal original receipt cannot be rewritten.
    const auto answer = transaction_.status(context, id);
    if (!eq(answer.operation, id)) {
        if (known) {
            known->status = ManualStatus::Unknown;
            known->release = ReleaseDisposition::Unresolved;
            known->error = ManualError::Transaction;
        }
        return failure(ManualError::Transaction);
    }
    // Internal uncertainty must latch BEFORE any later status-disclosure fence.
    // Revocation may deny the reply but cannot make a pending write retryable.
    if (known &&
        (answer.status == ManualStatus::Unknown || answer.status == ManualStatus::Committed ||
         answer.status == ManualStatus::Duplicate || answer.status == ManualStatus::NotCommitted)) {
        known->status = answer.status;
        known->error = answer.error;
        known->release =
            (answer.status == ManualStatus::Committed || answer.status == ManualStatus::Duplicate)
                ? answer.release
            : answer.status == ManualStatus::NotCommitted ? ReleaseDisposition::NoRelease
                                                          : ReleaseDisposition::Unresolved;
    }
    if (!sameContext(context, program_.context) || !sameContext(context, state_.context))
        return failure(ManualError::Context);
    const auto checked = authority_.check(context, authority);
    if (checked != ManualAuthorityResult::Current)
        return denied(checked);
    if (known)
        return result(*known);
    return answer;
}
ManualResult ManualOverrideOwner::service() {
    if (operating_)
        return failure(ManualError::Busy, true);
    Operation operation(operating_);
    if (!validWorkspace())
        return failure(ManualError::InvalidArgument);
    std::uint64_t now = 0;
    if (!clock_.now(now) || now < now_)
        return failure(ManualError::Unavailable, true);
    now_ = now;
    if (unknown())
        return failure(ManualError::RecoveryRequired, true);
    retire(now);
    for (std::size_t i = 0; i < work_.replays.count; ++i) {
        auto &entry = work_.replays.data[i];
        if (entry.status != ManualStatus::Confirming)
            continue;
        const OverrideAdmission admission(entry.record);
        submitting_ = static_cast<Id>(i + 1);
        const auto answer = transaction_.submit(admission, *this);
        submitting_ = 0;
        entry.release =
            (answer.status == ManualStatus::Committed || answer.status == ManualStatus::Duplicate)
                ? answer.release
            : answer.status == ManualStatus::NotCommitted ? ReleaseDisposition::NoRelease
                                                          : ReleaseDisposition::Unresolved;
        if (!eq(answer.operation, entry.record.operation)) {
            entry.status = ManualStatus::Unknown;
            entry.release = ReleaseDisposition::Unresolved;
            entry.error = ManualError::Transaction;
        } else
            switch (answer.status) {
            case ManualStatus::Committed:
            case ManualStatus::Duplicate:
            case ManualStatus::Unknown:
            case ManualStatus::NotCommitted:
            case ManualStatus::Rejected:
                entry.status = answer.status;
                entry.error = answer.error;
                break;
            case ManualStatus::Confirming:
            case ManualStatus::Deferred:
                break;
            default:
                entry.status = ManualStatus::Unknown;
                entry.release = ReleaseDisposition::Unresolved;
                entry.error = ManualError::Transaction;
                break;
            }
        return result(entry);
    }
    ManualResult answer;
    answer.status = ManualStatus::Deferred;
    return answer;
}
} // namespace rules
} // namespace execution
} // namespace larp
