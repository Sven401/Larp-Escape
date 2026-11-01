// Own runtime smoke: actual SHA/Journal/Codec/Planner/ManualOwner; deliberately
// synthetic install/authority/input ports. NO emitted Fact is accepted here.
#include "../rules-bundle-review/Store.h"
#include "../rules-planner-review/CompilerSupport.h"
#include "DeviceRuntime/RulesProgressOwner.h"
#include <cstdlib>
#include <memory>
namespace xr = larp::execution::rules;
namespace rp = larp::devices::rules_progress;
namespace codec = larp::execution::rules_progress;
namespace rb = larp::devices::rules_bundle;
#include "../manual-owner-review/Memory.h"
using namespace larp::devices;
namespace {
unsigned checks;
void require(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
xr::Identity identity(unsigned n) {
    xr::Identity out;
    for (unsigned i = 0; i < 4; ++i)
        out.bytes[15 - i] = static_cast<std::uint8_t>(n >> (i * 8));
    return out;
}
struct Clock final : rp::IClock, xr::IManualClock {
    xr::Identity boot = identity(50);
    std::uint64_t value = 10;
    bool read(rp::ClockObservation &out) override {
        out.boot = boot;
        out.monotonic = value;
        return true;
    }
    bool now(std::uint64_t &out) override {
        out = value;
        return true;
    }
};
struct Identities final : rb::IIdentitySource, xr::IManualIdentitySource {
    unsigned value = 10000;
    bool make(rb::Identity &out) override {
        auto id = identity(++value);
        std::memcpy(out.data(), id.bytes, 16);
        return true;
    }
    bool make(xr::Identity &out) override {
        out = identity(++value);
        return true;
    }
};
struct Metadata final : rp::ICommitMetadata {
    const xr::Program *program = nullptr;
    const xr::State *state = nullptr;
    bool available = true;
    rp::MetadataScope read(rp::CommitMetadata &out) override {
        if (!available || !program || !state)
            return rp::MetadataScope::Unavailable;
        out.context = program->context;
        out.fence = state->fence;
        return rp::MetadataScope::Current;
    }
};
struct ControlProof final : rp::ISignedControlEvidence {
    bool available = true;
    rp::EvidenceResult freezeControl(const xr::Program &, const xr::State &, rp::Control &c,
                                     rp::EvidenceStorage &s) override {
        rp::EvidenceResult out;
        if (!available || !s.bytes.capacity) {
            out.scope = rp::EvidenceScope::Unavailable;
            return out;
        }
        if (c.kind == rp::ControlKind::Activation) {
            c.targetFence.profile = c.targetFence.trust = c.targetFence.rights =
                c.targetFence.capacity = 1;
            c.targetFence.resources = c.targetFence.facts = c.targetFence.ledger = 1;
        }
        s.bytes.data[0] = 7;
        s.bytes.count = 1;
        c.authorization.data = s.bytes.data;
        c.authorization.count = 1;
        out.scope = rp::EvidenceScope::FrozenOriginal;
        return out;
    }
    rp::EvidenceResult historicalControl(const xr::Program &, const xr::State &,
                                         const rp::Control &c, const ProgressRecord &) override {
        rp::EvidenceResult out;
        if (available && c.authorization.count == 1 && c.authorization.data[0] == 7)
            out.scope = rp::EvidenceScope::VerifiedHistorical;
        return out;
    }
};
struct ControlFence final : rp::IControlFinalFence {
    bool available = true;
    rp::ControlEligibility preflight(const xr::Program &, const xr::State &,
                                     const rp::Control &) override {
        return available ? rp::ControlEligibility::Current : rp::ControlEligibility::Unavailable;
    }
    rp::ControlEligibility check(const xr::Program &, const xr::State &, const rp::Control &,
                                 const ProgressRecord &) override {
        return available ? rp::ControlEligibility::Current : rp::ControlEligibility::Unavailable;
    }
};
struct Eligibility final : rp::IIntentEligibility {
    rp::IntentEligibility check(const xr::Program &, const xr::Program &, const xr::State &,
                                const rp::Transaction &, const codec::Application &,
                                const rp::IntentReservation &, const xr::Intent &,
                                const rp::Effect &, const ProgressRecord &) override {
        return rp::IntentEligibility::Unavailable;
    }
};
struct Handler final : rp::IIntentHandler {
    unsigned calls = 0;
    rp::HandlerResult deliver(const xr::Context &, const xr::Identity &, std::uint16_t,
                              const xr::Intent &) override {
        ++calls;
        return rp::HandlerResult::Unknown;
    }
};
struct Authority final : xr::IManualAuthority {
    bool available = true;
    xr::ManualAuthorityResult check(const xr::Context &, const xr::OverrideAuthority &) override {
        return available ? xr::ManualAuthorityResult::Current : xr::ManualAuthorityResult::Closed;
    }
};
struct FinalFence final : xr::IManualFinalFence {
    unsigned calls = 0;
    xr::ManualAuthorityResult check(const xr::OverrideRecord &) override {
        ++calls;
        return xr::ManualAuthorityResult::Current;
    }
};
struct NoOutputProof final : rp::ISignedPlanEvidence {
    bool available = true, positiveError = false;
    unsigned historicalCalls = 0, failHistorical = 0;
    static void put(std::uint8_t *p, std::uint64_t n) {
        for (unsigned i = 0; i < 8; ++i)
            p[7 - i] = static_cast<std::uint8_t>(n >> (i * 8));
    }
    static std::uint64_t get(const std::uint8_t *p) {
        std::uint64_t n = 0;
        for (unsigned i = 0; i < 8; ++i)
            n = (n << 8) | p[i];
        return n;
    }
    rp::EvidenceResult freeze(const xr::Program &, const xr::State &, const rp::ClockWitness &w,
                              codec::Application &a, rp::EvidenceStorage &s) override {
        rp::EvidenceResult out;
        // Never fabricate a signature/outgoing Fact or physical intent.
        if (!available || a.plan.emissions.count || a.plan.intents.count || s.bytes.capacity < 48) {
            out.scope = rp::EvidenceScope::Unavailable;
            return out;
        }
        std::memcpy(s.bytes.data, w.boot.bytes, 16);
        put(s.bytes.data + 16, w.monotonicBefore);
        put(s.bytes.data + 24, w.monotonicAfter);
        put(s.bytes.data + 32, w.activeBefore);
        put(s.bytes.data + 40, w.activeAfter);
        s.bytes.count = 48;
        a.authorization.data = s.bytes.data;
        a.authorization.count = 48;
        a.proofs.count = a.outbox.count = 0;
        out.scope = rp::EvidenceScope::FrozenOriginal;
        if (positiveError)
            out.error = rp::EvidenceError::Crypto;
        return out;
    }
    rp::EvidenceResult historical(const xr::Program &, const xr::State &,
                                  const codec::Application &a, const ProgressRecord &,
                                  rp::ClockWitness &w) override {
        rp::EvidenceResult out;
        ++historicalCalls;
        if (a.authorization.count != 48 || (failHistorical && historicalCalls == failHistorical))
            return out;
        std::memcpy(w.boot.bytes, a.authorization.data, 16);
        w.monotonicBefore = get(a.authorization.data + 16);
        w.monotonicAfter = get(a.authorization.data + 24);
        w.activeBefore = get(a.authorization.data + 32);
        w.activeAfter = get(a.authorization.data + 40);
        out.scope = rp::EvidenceScope::VerifiedHistorical;
        return out;
    }
};
struct Memory {
    rp::Workspace work;
    ProgramMemory historical;
    StateMemory banks[3];
    PlannerMemory prepared, decoded, computed;
    std::vector<std::uint8_t> preparedBytes, decodedBytes, app, decodedApp, compare, history,
        physical;
    std::vector<codec::SignedFact> proofs[2], outbox[2];
    std::vector<rp::CatalogEntry> catalogs;
    std::vector<rp::ApplicationEntry> applications;
    std::vector<rp::ControlEntry> controls;
    std::vector<rp::IntentEntry> intents;
    std::vector<rp::IntentReservation> reservations;
    Memory() {
        work.historicalProgram = historical.program;
        for (unsigned i = 0; i < 3; ++i)
            work.banks[i].storage = banks[i].pools;
        work.prepared.plan = prepared.plan;
        work.decoded.plan = decoded.plan;
        work.computed = computed.plan;
        work.planner = computed.workspace;
        storage(work.preparedEvidence.bytes, preparedBytes, 32768);
        storage(work.decodedStorage.bytes, decodedBytes, 32768);
        storage(work.applicationBytes, app, 32768);
        storage(work.decodedApplication, decodedApp, 32768);
        storage(work.comparison, compare, 32768);
        storage(work.history, history, 262144);
        storage(work.physicalBytes, physical, 16384);
        storage(work.prepared.proofs, proofs[0], 128);
        storage(work.prepared.outbox, outbox[0], 8);
        storage(work.decoded.proofs, proofs[1], 128);
        storage(work.decoded.outbox, outbox[1], 8);
        storage(work.catalogs, catalogs, 63);
        storage(work.applications, applications, 63);
        storage(work.controls, controls, 63);
        storage(work.intents, intents, 64);
        storage(work.envelope.intents, reservations, 32);
    }
};
struct BundleMemory {
    rb::Workspace work;
    std::vector<std::uint8_t> staged, recovered;
    std::vector<rb::Descriptor> bundles;
    std::vector<rb::RecordLink> records;
    BundleMemory() {
        storage(work.staged, staged, 32768);
        storage(work.recovered, recovered, 32768);
        storage(work.bundles, bundles, 63);
        storage(work.records, records, 63);
    }
};
struct Fixture {
    ProgramMemory program;
    Memory memory;
    BundleMemory bundleMemory;
    bundle_review::Medium medium;
    bundle_review::Session session;
    ProgressJournal journal;
    Hash hash;
    Identities ids;
    rb::Journal bundle;
    Clock clock;
    Metadata metadata;
    rp::SerializedOrdering ordering;
    ControlProof controlProof;
    ControlFence controlFence;
    Eligibility eligibility;
    Handler handler;
    NoOutputProof proof;
    rp::Owner owner;
    Authority authority;
    std::vector<xr::ManualReplay> replays;
    std::vector<std::uint8_t> manualBytes;
    xr::ManualWorkspace manualWork;
    std::unique_ptr<xr::ManualOverrideOwner> manual;
    Fixture(const std::vector<std::uint8_t> &catalog, const bundle_review::Medium *prior = nullptr,
            rp::ICommitOrdering *survivor = nullptr, unsigned boot = 50)
        : medium(prior ? *prior : bundle_review::Medium()), session(medium), journal(session),
          bundle(journal, hash, ids, bundleMemory.work), ordering(metadata, identity(9000)),
          owner(program.program, journal, bundle, bundleMemory.work, hash, ids, clock,
                survivor ? *survivor : ordering, proof, controlProof, controlFence, eligibility,
                handler, memory.work) {
        xr::Span<std::uint8_t> bytes;
        bytes.data = catalog.data();
        bytes.count = catalog.size();
        require(codec::decodeCatalog(bytes, program.program).scope ==
                    codec::Scope::DecodedUnauthenticated,
                "actual catalog decode");
        for (std::size_t i = 0; i < program.program.rules.count; ++i)
            if (program.program.rules.data[i].ir.trigger == 4) {
                auto &n = program.program.nodes.data[program.program.rules.data[i].ir.guard - 1];
                n.op = Op::Literal;
                n.type = Type::Boolean;
                n.value = 0;
                n.ref = 0;
                n.count = 0;
                for (auto &c : n.children)
                    c = 0;
            }
        require(xr::validateOwnedProgram(program.program).decision == xr::Decision::Ready,
                "typed false guard modification valid, not new artifact claim");
        program.program.ready =
            true; // Explicit test fixture publication after full shared validation.
        clock.boot = identity(boot);
        metadata.program = &program.program;
        metadata.state = &owner.state();
        ProgressOrigin origin;
        std::memcpy(origin.event.data(), program.program.context.event.bytes, 16);
        std::memcpy(origin.box.data(), program.program.context.box.bytes, 16);
        std::memcpy(origin.epoch.data(), program.program.context.sourceEpoch.bytes, 16);
        require(prior ? journal.recover(origin).outcome == ProgressOutcome::Recovered
                      : journal.initialize(origin).outcome == ProgressOutcome::Initialized,
                "actual explicit fresh initialize or NEW durable backend session");
        const auto recovered = owner.recover();
        if (recovered.error != rp::Error::None)
            std::cerr << "recover error=" << int(recovered.error)
                      << " phase=" << int(recovered.phase)
                      << " bundle=" << int(recovered.journal.error)
                      << " outcome=" << int(recovered.journal.outcome) << '\n';
        require(recovered.error == rp::Error::None, "qualified fresh semantic recover");
        storage(manualWork.replays, replays, 16);
        storage(manualWork.catalogBytes, manualBytes, 32768);
        manualWork.maximumActive = 4;
        manual.reset(new xr::ManualOverrideOwner(program.program, owner.state(), clock.boot, 30,
                                                 hash, clock, authority, ids, owner, manualWork));
    }
    rp::Result finish(rp::Result r) {
        for (unsigned i = 0; i < 20 && r.error == rp::Error::None &&
                             (owner.phase() == rp::Phase::WritingCatalog ||
                              owner.phase() == rp::Phase::WritingControl);
             ++i)
            r = owner.service();
        return r;
    }
    void start() {
        require(finish(owner.activate(identity(100), xr::Digest{})).error == rp::Error::None,
                "synthetic install/control, actual catalog and activation durable");
        require(owner.state().mode == xr::Mode::Stopped, "activation never auto starts");
        require(finish(owner.control(rp::ControlKind::Start, identity(101), xr::Digest{})).error ==
                    rp::Error::None,
                "synthetic authorized start durable");
        require(owner.state().mode == xr::Mode::Running, "start publication");
    }
    xr::ManualResult confirm(unsigned phase = 200, bool complete = true) {
        xr::Identity rule;
        for (std::size_t i = 0; i < program.program.rules.count; ++i)
            if (program.program.rules.data[i].ir.trigger == 3)
                rule = program.program.rules.data[i].stable;
        xr::OverrideAuthority actor;
        actor.peer = identity(9);
        actor.principal = 1;
        actor.roleEpoch = 1;
        actor.connectionToken = 1;
        actor.registration = 1;
        actor.lifecycle = 1;
        actor.registryRevision = 1;
        actor.eventRevision = 1;
        xr::ManualView view;
        require(manual->view(rule, actor, view).status == xr::ManualStatus::Prepared,
                "actual private owner view");
        xr::ManualPrepareRequest p;
        p.ownerBoot = clock.boot;
        p.phase = identity(phase);
        p.rule = rule;
        p.view = view.digest;
        p.authority = actor;
        const auto prepared = manual->prepare(p);
        require(prepared.status == xr::ManualStatus::Prepared, "actual prepare");
        xr::ManualConfirmRequest c;
        c.ownerBoot = clock.boot;
        c.token = prepared.token;
        c.phase = identity(phase + 1);
        c.authority = actor;
        auto answer = manual->confirm(c);
        for (unsigned i = 0; complete && i < 20 && answer.status == xr::ManualStatus::Confirming;
             ++i)
            answer = manual->service();
        return answer;
    }
};
} // namespace
int main() {
    std::string line;
    std::getline(std::cin, line);
    std::vector<std::uint8_t> catalog;
    for (std::size_t i = 0; i < line.size(); i += 2)
        catalog.push_back(static_cast<std::uint8_t>(std::stoul(line.substr(i, 2), nullptr, 16)));
    {
        Fixture f(catalog);
        const auto programs = f.session.programs;
        f.controlProof.available = false;
        require(f.owner.activate(identity(100), xr::Digest{}).error == rp::Error::Admission,
                "missing original evidence closes");
        require(f.session.programs == programs && !f.journal.snapshot().records,
                "no writes without evidence");
    }
    {
        Fixture f(catalog);
        const auto programs = f.session.programs;
        f.controlFence.available = false;
        require(f.owner.activate(identity(100), xr::Digest{}).error == rp::Error::Admission,
                "missing live fence closes");
        require(f.session.programs == programs && !f.journal.snapshot().records,
                "no catalog consumption without fence");
    }
    {
        Fixture f(catalog);
        f.start();
        const auto records = f.journal.snapshot().records;
        f.proof.positiveError = true;
        const auto answer = f.confirm();
        require(answer.status != xr::ManualStatus::Committed &&
                    answer.status != xr::ManualStatus::Duplicate,
                "positive scope plus error never authorizes");
        require(f.journal.snapshot().records == records, "contradictory evidence no writes");
    }
    {
        Fixture f(catalog);
        f.start();
        const auto answer = f.confirm();
        require(answer.status == xr::ManualStatus::Committed,
                "actual no-fire application committed with synthetic input proof");
        require(answer.release == xr::ReleaseDisposition::NoRelease,
                "durability never implies release");
        require(f.owner.state().latches.count == 0 && f.journal.snapshot().lastIssued == 0 &&
                    f.handler.calls == 0,
                "no latch/ID/physical effect");
        require(f.owner.state().ledger.count == 1, "input receipt durable");
        require(f.owner.status(f.program.program.context, answer.operation).release ==
                    xr::ReleaseDisposition::NoRelease,
                "same copied disposition");
    }
    {
        Fixture f(catalog);
        f.start();
        const auto first = f.confirm(300);
        const auto second = f.confirm(400);
        require(first.status == xr::ManualStatus::Committed &&
                    second.status == xr::ManualStatus::Committed,
                "two actual no-fire applications");
        const auto originalLive = f.owner.state().ledger.data[0].input.overrideAdmission;
        require(originalLive.scope() == xr::OverrideAdmissionScope::Live,
                "private original admission came from actual ManualOwner");
        f.clock.boot = identity(51);
        f.proof.available = false;
        f.authority.available = false;
        require(f.owner.recover().error == rp::Error::None,
                "cold historical rebuild does not resubmit/current-authorize/resign");
        require(f.owner.state().mode == xr::Mode::RecoveryRequired &&
                    f.owner.state().ledger.count == 2,
                "recovered mode stopped, all receipts retained");
        require(f.owner.state().ledger.data[0].input.overrideAdmission.scope() ==
                    xr::OverrideAdmissionScope::Historical,
                "actual typed replay admission is historical only");
        require(f.owner.status(f.program.program.context, first.operation).release ==
                    xr::ReleaseDisposition::NoRelease,
                "historical original NoRelease preserved");
        FinalFence fence;
        const auto historical = f.owner.state().ledger.data[0].input.overrideAdmission;
        const auto programs = f.session.programs;
        require(f.owner.submit(historical, fence).status == xr::ManualStatus::Rejected,
                "historical admission cannot live submit");
        require(f.session.programs == programs && !fence.calls,
                "historical rejection before writer/finalfence");
        f.proof.historicalCalls = 0;
        f.proof.failHistorical = 2;
        require(f.owner.recover().error == rp::Error::Recovery,
                "later semantic evidence failure aborts WHOLE recovery");
        require(f.owner.status(f.program.program.context, first.operation).status ==
                    xr::ManualStatus::Unknown,
                "no first valid prefix status publication");
        require(f.owner.status(f.program.program.context, second.operation).status ==
                    xr::ManualStatus::Unknown,
                "no later status publication");
        require(f.owner.submit(originalLive, fence).status == xr::ManualStatus::Unknown,
                "old live duplicate cannot bypass failed full recovery");
        require(f.session.programs == programs && !fence.calls,
                "failed recovery and queries perform no write/new final");
    }
    {
        Fixture old(catalog);
        auto prepared = old.owner.activate(identity(100), xr::Digest{});
        require(prepared.error == rp::Error::None && old.owner.phase() == rp::Phase::WritingCatalog,
                "catalog starts inertly");
        require(old.owner.service().error == rp::Error::None &&
                    old.owner.phase() == rp::Phase::WritingControl,
                "inert catalog final is NOT semantic control");
        old.session.budget = 0;
        require(old.owner.service().phase == rp::Phase::Unknown,
                "zero-byte uncertain control remains Unknown");
        require(old.ordering.mutationsBlocked(), "unknown control lease remains held");
        Fixture reopened(catalog, &old.medium, &old.ordering, 51);
        require(reopened.owner.phase() == rp::Phase::Uninitialized,
                "catalog-only recovery creates no program basis");
        require(!old.ordering.mutationsBlocked(),
                "qualified catalog-only noncommit resolves exact held control pin");
        require(reopened.journal.snapshot().records == 2 &&
                    reopened.journal.snapshot().lastIssued == 0,
                "inert records issue no IDs");
    }
    {
        Fixture old(catalog);
        old.start();
        const auto pending = old.confirm(600, false);
        require(pending.status == xr::ManualStatus::Confirming,
                "actual app begins with only chunk");
        unsigned remaining = 1;
        for (const auto &chunk : old.bundleMemory.work.chunks)
            if (chunk.frozen && !chunk.committed)
                ++remaining;
        old.session.readAfterSync = old.session.syncs + remaining * 3;
        auto answer = old.manual->service();
        for (unsigned i = 0; i < 20 && answer.status == xr::ManualStatus::Confirming; ++i)
            answer = old.manual->service();
        require(answer.status == xr::ManualStatus::Unknown,
                "post-final scan failure reported Unknown");
        require(old.ordering.mutationsBlocked(), "uncertain app keeps exact held identity");
        Fixture reopened(catalog, &old.medium, &old.ordering, 51);
        const auto known =
            reopened.owner.status(reopened.program.program.context, pending.operation);
        require(known.status == xr::ManualStatus::Committed &&
                    known.release == xr::ReleaseDisposition::NoRelease,
                "actual matching final resolves old held app exactly");
        require(!old.ordering.mutationsBlocked(), "recovery publishes then releases prepared pin");
    }
    std::cout << checks
              << " own runtime assertions; real SHA/storage, synthetic authority/install/input; NO "
                 "emitted signature positives\n";
}
