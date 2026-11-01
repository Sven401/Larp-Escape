#include "GameExecution/ManualOverrideOwner.h"
#include "GameExecution/RulesProgressCodec.h"
#ifdef MANUAL_OWNER_REVIEW_PSA
#define PACKAGE_MODULE_REVIEW_PSA
#endif
#include "../rules-planner-review/CompilerSupport.h"
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
namespace xr = larp::execution::rules;
namespace rp = larp::execution::rules_progress;
#include "Memory.h"
namespace {
std::size_t checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
xr::Identity id(std::uint64_t n) {
    xr::Identity out;
    for (unsigned i = 0; i < 8; ++i)
        out.bytes[15 - i] = static_cast<std::uint8_t>(n >> (i * 8));
    return out;
}
bool equal(const xr::Identity &a, const xr::Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool equal(const xr::Digest &a, const xr::Digest &b) {
    return !std::memcmp(a.bytes, b.bytes, 32);
}
bool equal(const xr::ManualResult &a, const xr::ManualResult &b) {
    return a.status == b.status && a.error == b.error && a.release == b.release &&
           equal(a.token, b.token) && equal(a.operation, b.operation) && a.expiresAt == b.expiresAt;
}
std::vector<std::uint8_t> unhex(const std::string &s) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return out;
}
std::string hex(const std::uint8_t *p, std::size_t n) {
    const char *d = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i) {
        out += d[p[i] >> 4];
        out += d[p[i] & 15];
    }
    return out;
}
xr::Digest sha(IPackageDigest &h, const std::uint8_t *p, std::size_t n) {
    xr::Digest out;
    std::size_t written = 0;
    check(h.begin() && h.update(p, n) && h.finish(out.bytes, written) && written == 32 && h.abort(),
          "actual SHA256");
    return out;
}
struct OwnerHash final : IPackageDigest {
    Hash real;
    std::function<void()> callback;
    int failure = 0;
    bool begin() override {
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        return failure != 1 && real.begin();
    }
    bool update(const std::uint8_t *p, std::size_t n) override {
        return failure != 2 && real.update(p, n);
    }
    bool finish(std::uint8_t *p, std::size_t &n) override {
        if (failure == 3)
            return false;
        const auto ok = real.finish(p, n);
        if (failure == 4)
            n = 31;
        return ok;
    }
    bool abort() override {
        const auto ok = real.abort();
        return ok && failure != 5;
    }
};
struct Clock final : xr::IManualClock {
    std::uint64_t time = 10;
    bool available = true;
    unsigned calls = 0;
    std::function<void()> callback;
    bool now(std::uint64_t &out) override {
        ++calls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        out = time;
        return available;
    }
};
struct Authority final : xr::IManualAuthority {
    xr::Context context;
    xr::OverrideAuthority actor;
    xr::ManualAuthorityResult outcome = xr::ManualAuthorityResult::Current;
    unsigned calls = 0;
    std::function<void()> callback;
    bool same(const xr::OverrideAuthority &a) {
        return equal(a.peer, actor.peer) && a.principal == actor.principal &&
               a.roleEpoch == actor.roleEpoch && a.connectionToken == actor.connectionToken &&
               a.registration == actor.registration && a.lifecycle == actor.lifecycle &&
               a.connectionHandle == actor.connectionHandle &&
               a.registryRevision == actor.registryRevision &&
               a.eventRevision == actor.eventRevision;
    }
    xr::ManualAuthorityResult check(const xr::Context &c, const xr::OverrideAuthority &a) override {
        ++calls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        return xr::sameContext(c, context) && same(a) ? outcome : xr::ManualAuthorityResult::Stale;
    }
};
struct Identities final : xr::IManualIdentitySource {
    std::uint64_t next = 0xa000;
    bool available = true, zero = false;
    unsigned calls = 0;
    std::function<void()> callback;
    bool make(xr::Identity &out) override {
        ++calls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        out = zero ? xr::Identity{} : id(next++);
        return available;
    }
};
struct Transaction final : xr::IManualTransaction {
    unsigned calls = 0, statusCalls = 0;
    xr::ManualStatus outcome = xr::ManualStatus::Committed,
                     statusOutcome = xr::ManualStatus::Committed;
    xr::ReleaseDisposition release = xr::ReleaseDisposition::Unresolved;
    xr::OverrideRecord captured;
    xr::OverrideAdmission copied;
    xr::IManualFinalFence *retained = nullptr;
    std::function<void()> callback;
    std::function<void(xr::OverrideRecord &)> alter;
    bool wrongOperation = false, statusWrongOperation = false;
    std::vector<xr::OverrideRecord> history;
    xr::ManualResult submit(const xr::OverrideAdmission &a, xr::IManualFinalFence &f) override {
        ++calls;
        copied = a;
        captured = a.record();
        history.push_back(captured);
        retained = &f;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        auto record = captured;
        if (alter)
            alter(record);
        const auto allowed = f.check(record);
        xr::ManualResult answer;
        answer.release = release;
        answer.operation = wrongOperation ? id(0xbad) : captured.operation;
        answer.status =
            allowed == xr::ManualAuthorityResult::Current ? outcome : xr::ManualStatus::Rejected;
        answer.error = allowed == xr::ManualAuthorityResult::Current ? xr::ManualError::None
                                                                     : xr::ManualError::Stale;
        return answer;
    }
    xr::ManualResult status(const xr::Context &, const xr::Identity &operation) override {
        ++statusCalls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        xr::ManualResult answer;
        answer.release = release;
        answer.operation = statusWrongOperation ? id(0xbad) : operation;
        answer.status = statusOutcome;
        return answer;
    }
};
std::vector<std::uint8_t> originalCatalog;
xr::Digest originalHash;
struct Fixture {
    ProgramMemory program;
    StateMemory storageState;
    xr::State state;
    std::vector<xr::ManualReplay> replay;
    std::vector<std::uint8_t> bytes;
    xr::ManualWorkspace work;
    OwnerHash hash;
    Clock clock;
    Authority authority;
    Identities ids;
    Transaction transaction;
    xr::Identity boot = id(0x5000), rule;
    std::unique_ptr<xr::ManualOverrideOwner> owner;
    Fixture(std::uint64_t lifetime = 30, std::size_t active = 4, std::size_t total = 16)
        : replay(total), bytes(32768) {
        xr::Span<std::uint8_t> source;
        source.data = originalCatalog.data();
        source.count = originalCatalog.size();
        check(rp::decodeCatalog(source, program.program).scope ==
                      rp::Scope::DecodedUnauthenticated &&
                  program.program.ready,
              "actual preserved compiler catalog decoded/validated");
        for (std::size_t i = 0; i < program.program.rules.count; ++i)
            if (program.program.rules.data[i].ir.policy == 3) {
                rule = program.program.rules.data[i].stable;
                break;
            }
        check(!equal(rule, xr::Identity{}), "actual compiled original/fallback pair");
        xr::Fence fence;
        fence.progress = fence.profile = fence.trust = fence.facts = fence.ledger =
            fence.resources = fence.rights = fence.capacity = 1;
        check(xr::Planner::initialize(program.program, fence, storageState.pools, state).decision ==
                  xr::Decision::Ready,
              "actual owned initialized State snapshot");
        check(xr::Planner::control(state, xr::Mode::Running).decision == xr::Decision::Ready,
              "actual running snapshot");
        authority.context = state.context;
        auto &a = authority.actor;
        a.peer = id(0x7000);
        a.principal = 2;
        a.roleEpoch = 3;
        a.connectionToken = 4;
        a.registration = 5;
        a.lifecycle = 6;
        a.connectionHandle = 7;
        a.registryRevision = 8;
        a.eventRevision = 9;
        work.replays.data = replay.data();
        work.replays.capacity = replay.size();
        work.maximumActive = active;
        work.catalogBytes.data = bytes.data();
        work.catalogBytes.capacity = bytes.size();
        owner.reset(new xr::ManualOverrideOwner(program.program, state, boot, lifetime, hash, clock,
                                                authority, ids, transaction, work));
    }
    xr::ManualPrepareRequest request(unsigned phase = 1) {
        xr::ManualView v;
        check(owner->view(rule, authority.actor, v).status == xr::ManualStatus::Prepared,
              "actual owner view");
        xr::ManualPrepareRequest r;
        r.ownerBoot = v.ownerBoot;
        r.phase = id(0x1000 + phase);
        r.rule = v.rule;
        r.view = v.digest;
        r.authority = authority.actor;
        return r;
    }
    xr::ManualConfirmRequest confirmRequest(const xr::ManualResult &p, unsigned phase = 1) {
        xr::ManualConfirmRequest r;
        r.ownerBoot = boot;
        r.token = p.token;
        r.phase = id(0x2000 + phase);
        r.authority = authority.actor;
        return r;
    }
    xr::ManualResult prepare(unsigned phase = 1) {
        return owner->prepare(request(phase));
    }
};
void basic() {
    Fixture f;
    const auto request = f.request();
    const auto p = f.owner->prepare(request);
    check(p.status == xr::ManualStatus::Prepared && p.expiresAt == 40 && f.transaction.calls == 0,
          "prepare effect-free fixed lifetime");
    check(equal(f.replay[0].record.program, originalHash),
          "actual complete canonical catalog hash frozen");
    const auto first = f.replay[0].record;
    f.clock.time = 35;
    check(equal(f.owner->prepare(request), p) && xr::sameOverrideRecord(f.replay[0].record, first),
          "repeat prepare byte-for-byte unchanged");
    auto changed = request;
    changed.view.bytes[0] ^= 1;
    check(f.owner->prepare(changed).error == xr::ManualError::Conflict,
          "same phase changed request conflict");
    const auto c = f.confirmRequest(p);
    const auto done = f.owner->confirm(c);
    check(done.status == xr::ManualStatus::Committed &&
              f.transaction.copied.scope() == xr::OverrideAdmissionScope::Live,
          "only actual owner produces copied live admission in synthetic ports scope");
    f.clock.time = 100;
    check(equal(f.owner->confirm(c), done) && f.transaction.calls == 1,
          "terminal confirm never expires or reexecutes");
    auto phase = c;
    phase.phase = id(0x2002);
    check(f.owner->confirm(phase).error == xr::ManualError::Conflict,
          "terminal confirm first phase immutable");
    check(f.transaction.retained->check(f.transaction.captured) == xr::ManualAuthorityResult::Stale,
          "final fence unusable outside actual active submit");
    std::cout << "PASS basic\n";
}
void unknown() {
    Fixture f;
    const auto one = f.prepare(1), two = f.prepare(2);
    const auto request = f.request(3);
    f.transaction.outcome = xr::ManualStatus::Unknown;
    const auto u = f.owner->confirm(f.confirmRequest(one, 1));
    check(u.status == xr::ManualStatus::Unknown, "initial unknown");
    const auto ids = f.ids.calls, calls = f.transaction.calls;
    xr::ManualView v;
    check(f.owner->view(f.rule, f.authority.actor, v).error == xr::ManualError::RecoveryRequired,
          "unknown globally blocks views");
    check(f.owner->prepare(request).error == xr::ManualError::RecoveryRequired &&
              f.owner->confirm(f.confirmRequest(two, 2)).error ==
                  xr::ManualError::RecoveryRequired &&
              f.owner->service().error == xr::ManualError::RecoveryRequired,
          "unknown globally blocks grants/submits/service");
    check(ids == f.ids.calls && calls == f.transaction.calls,
          "unknown allocates no identities/submissions");
    f.transaction.statusWrongOperation = true;
    check(f.owner->status(f.state.context, f.authority.actor, u.operation).error ==
                  xr::ManualError::Transaction &&
              f.replay[0].status == xr::ManualStatus::Unknown,
          "status wrong operation cannot unlock unknown");
    f.transaction.statusWrongOperation = false;
    f.transaction.statusOutcome = xr::ManualStatus::Committed;
    check(f.owner->status(f.state.context, f.authority.actor, u.operation).status ==
                  xr::ManualStatus::Committed &&
              f.replay[0].status == xr::ManualStatus::Committed,
          "same operation qualified-status port resolves unknown (synthetic)");
    std::cout << "PASS unknown\n";
}
void statusUnknown() {
    Fixture f;
    const auto p = f.prepare();
    f.transaction.outcome = xr::ManualStatus::Confirming;
    const auto started = f.owner->confirm(f.confirmRequest(p));
    check(started.status == xr::ManualStatus::Confirming, "status unknown pending prerequisite");
    f.transaction.statusOutcome = xr::ManualStatus::Unknown;
    check(f.owner->status(f.state.context, f.authority.actor, started.operation).status ==
              xr::ManualStatus::Unknown,
          "status reports actual transaction unknown");
    const auto before = f.transaction.calls;
    const auto r = f.owner->service();
    if (r.error != xr::ManualError::RecoveryRequired || f.transaction.calls != before) {
        std::cerr << "STATUS_UNKNOWN observed cached=" << unsigned(f.replay[0].status)
                  << " service=" << unsigned(r.status) << '/' << unsigned(r.error)
                  << " submitCalls=" << f.transaction.calls - before << '\n';
    }
    check(r.error == xr::ManualError::RecoveryRequired && f.transaction.calls == before,
          "status Unknown must impose same global barrier");
    std::cout << "PASS status-unknown\n";
}

void terminalStatus() {
    Fixture f;
    const auto p = f.prepare();
    const auto c = f.confirmRequest(p);
    const auto committed = f.owner->confirm(c);
    const auto original = f.replay[0];
    check(committed.status == xr::ManualStatus::Committed, "immutable terminal prerequisite");
    f.transaction.statusOutcome = xr::ManualStatus::NotCommitted;
    const auto r = f.owner->status(f.state.context, f.authority.actor, committed.operation);
    if (f.replay[0].status != original.status)
        std::cerr << "TERMINAL observed original=" << unsigned(original.status)
                  << " new=" << unsigned(f.replay[0].status) << " answer=" << unsigned(r.status)
                  << '\n';
    check(f.replay[0].status == original.status && f.replay[0].error == original.error &&
              xr::sameOverrideRecord(original.record, f.replay[0].record),
          "contradictory backend status must not rewrite known immutable terminal receipt");
    check(equal(f.owner->confirm(c), committed),
          "terminal Confirm answer remains original after contradictory status");
    std::cout << "PASS terminal-status\n";
}
void clockFences() {
    {
        Fixture f;
        const auto req = f.request();
        const auto p = f.owner->prepare(req);
        const auto old = f.replay[0].record;
        f.state.fence.activeTime = 123;
        check(equal(f.owner->prepare(req), p) && xr::sameOverrideRecord(old, f.replay[0].record),
              "real monotone State clock does not rebase original Prepare");
        const auto done = f.owner->confirm(f.confirmRequest(p));
        check(done.status == xr::ManualStatus::Committed &&
                  f.transaction.captured.expected.activeTime == 0 &&
                  f.transaction.captured.expiresAt == 40,
              "original clock/time metadata frozen through Final");
    }
    for (unsigned field = 0; field < 10; ++field) {
        Fixture f;
        f.state.fence.activeTime = 5;
        const auto p = f.prepare();
        auto &x = f.state.fence;
        switch (field) {
        case 0:
            ++x.progress;
            break;
        case 1:
            ++x.profile;
            break;
        case 2:
            ++x.trust;
            break;
        case 3:
            ++x.facts;
            break;
        case 4:
            ++x.ledger;
            break;
        case 5:
            ++x.resources;
            break;
        case 6:
            ++x.rights;
            break;
        case 7:
            ++x.capacity;
            break;
        case 8:
            ++x.lastIssued;
            break;
        default:
            x.activeTime = 4;
        }
        const auto r = f.owner->confirm(f.confirmRequest(p));
        check(r.status == xr::ManualStatus::Rejected && f.transaction.calls == 0,
              "logical fence change / clock rollback rejects before submit");
    }
    for (std::uint64_t time : {39u, 40u, 41u}) {
        Fixture f;
        const auto p = f.prepare();
        f.clock.time = time;
        const auto r = f.owner->confirm(f.confirmRequest(p));
        check(r.status == (time <= 40 ? xr::ManualStatus::Committed : xr::ManualStatus::Expired),
              "absolute TTL inclusive end");
    }
    {
        Fixture f;
        f.clock.time = std::numeric_limits<std::uint64_t>::max() - 10;
        const auto r = f.prepare();
        check(r.status == xr::ManualStatus::Rejected && r.error == xr::ManualError::Identity &&
                  f.ids.calls == 0 && f.work.replays.count == 0,
              "TTL addition overflow before identities");
    }
    std::cout << "PASS clock-fences\n";
}
void completeProgram() {
    Fixture f;
    const auto p = f.prepare();
    const auto old = f.replay[0].record;
    f.program.program.rules.data[0].ir.priority++;
    Pool<std::uint8_t> output;
    std::vector<std::uint8_t> bytes(32768);
    output.data = bytes.data();
    output.capacity = bytes.size();
    check(rp::encodeCatalog(f.program.program, output).scope == rp::Scope::EncodedUnpublished,
          "changed complete program still encodes");
    const auto fresh = sha(f.hash, bytes.data(), output.count);
    check(!equal(fresh, old.program) && xr::sameContext(f.state.context, old.context),
          "actual complete catalog changes while Context identical");
    check(f.owner->confirm(f.confirmRequest(p)).error == xr::ManualError::Stale &&
              f.transaction.calls == 0,
          "complete program change rejects original grant");
    std::cout << "PASS complete-program\n";
}
void pendingIdentity() {
    Fixture f;
    const auto p = f.prepare();
    const auto c = f.confirmRequest(p);
    f.transaction.outcome = xr::ManualStatus::Confirming;
    const auto started = f.owner->confirm(c);
    const auto original = f.replay[0].record;
    f.clock.time = 20;
    const auto replay = f.owner->confirm(c);
    check(replay.status == xr::ManualStatus::Confirming &&
              equal(replay.operation, started.operation) &&
              xr::sameOverrideRecord(original, f.replay[0].record) &&
              xr::sameOverrideRecord(f.transaction.history[0], f.transaction.history[1]),
          "pending retries preserve first Confirm/operation/digest/time");
    auto changed = c;
    changed.phase = id(0x2002);
    check(f.owner->confirm(changed).error == xr::ManualError::Conflict,
          "pending different Confirm phase conflict");
    f.clock.time = 41;
    const auto expired = f.owner->service();
    check(expired.status == xr::ManualStatus::Rejected &&
              equal(expired.operation, started.operation) &&
              xr::sameOverrideRecord(original, f.replay[0].record),
          "worker terminates expired pending under original identity via final fence");
    std::cout << "PASS pending-identity\n";
}
void expiryFirstPhase() {
    Fixture f;
    const auto p = f.prepare();
    f.clock.time = 41;
    const auto c = f.confirmRequest(p);
    const auto expired = f.owner->confirm(c);
    check(expired.status == xr::ManualStatus::Expired,
          "expired current-authority Confirm terminalizes before acceptance");
    const auto identities = f.ids.calls;
    auto another = c;
    another.phase = id(0x2002);
    check(
        equal(f.owner->confirm(another), expired) &&
            equal(f.replay[0].record.confirmId, xr::Identity{}) &&
            equal(f.replay[0].record.operation, xr::Identity{}) && identities == f.ids.calls &&
            f.transaction.calls == 0,
        "unaccepted expired phases return identical terminal result without reserving identities");
    std::cout << "PASS expired-first-phase\n";
}
#include "Cases.h"
} // namespace
int main(int argc, char **argv) {
    std::string raw, expected;
    std::getline(std::cin, raw);
    std::getline(std::cin, expected);
    originalCatalog = unhex(raw);
    Hash h;
    originalHash = sha(h, originalCatalog.data(), originalCatalog.size());
    check(hex(originalHash.bytes, 32) == expected, "independent preserved actual catalog SHA");
    const std::string name = argc > 1 ? argv[1] : "all";
    struct Case {
        const char *name;
        void (*run)();
    };
    const Case cases[] = {{"basic", basic},
                          {"unknown", unknown},
                          {"status-unknown", statusUnknown},
                          {"terminal-status", terminalStatus},
                          {"clock-fences", clockFences},
                          {"complete-program", completeProgram},
                          {"pending-identity", pendingIdentity},
                          {"expired-first-phase", expiryFirstPhase},
                          {"status-fences", statusFences},
                          {"final-record", finalRecords},
                          {"callback-reentry", callbackReentry},
                          {"capacities-retire", capacitiesRetire},
                          {"boot-and-copy", bootAndCopy},
                          {"admission-scopes", admissionScopes},
                          {"final-clock", finalClock},
                          {"hash-identities", hashIdentities},
                          {"ram-release", ramRelease},
                          {"release-disposition", releaseDisposition}};
    bool ran = false;
    for (const auto &c : cases)
        if (name == "all" || name == c.name) {
            c.run();
            ran = true;
        }
    check(ran, "known named scenario");
    std::cout << "CHECKS " << checks << " SYNTHETIC_AUTHORITY_AND_TRANSACTION_ONLY\n";
}
