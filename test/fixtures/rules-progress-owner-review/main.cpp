// Prepared actual runtime harness. No positive qualification before Bodyready.
#ifdef RULES_PROGRESS_OWNER_REVIEW_PSA
#define D0_FACT_REVIEW_PSA
#define PACKAGE_MODULE_REVIEW_PSA
#endif
#include "../rules-bundle-review/Store.h"
#include "../rules-planner-review/CompilerSupport.h"
#include "DeviceRuntime/RulesProgressOwner.h"
#include <cstdlib>
#include <memory>
namespace xr = larp::execution::rules;
namespace rp = larp::execution::rules_progress;
namespace ev = larp::devices::rules_progress;
namespace rb = larp::devices::rules_bundle;
static unsigned checks = 0;
void requireCodec(bool ok, const char *text) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL " << text << '\n';
        std::exit(1);
    }
}
std::vector<std::uint8_t> unhex(const std::string &s) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return out;
}
std::string hex(const std::uint8_t *p, std::size_t n) {
    static const char *d = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += d[p[i] >> 4];
        s += d[p[i] & 15];
    }
    return s;
}
xr::Identity id(unsigned n) {
    xr::Identity out;
    out.bytes[14] = static_cast<std::uint8_t>(n >> 8);
    out.bytes[15] = static_cast<std::uint8_t>(n);
    return out;
}
xr::Digest request(unsigned n) {
    xr::Digest out;
    out.bytes[30] = static_cast<std::uint8_t>(n >> 8);
    out.bytes[31] = static_cast<std::uint8_t>(n);
    return out;
}
#include "../rules-progress-independent-review/Signer.h"
#include "Memory.h"
#include "Ports.h"
namespace owner_review {
struct BundleMemory {
    rb::Workspace work;
    std::vector<std::uint8_t> staged, recovered;
    std::vector<rb::Descriptor> descriptors;
    std::vector<rb::RecordLink> links;
    BundleMemory() {
        storage(work.staged, staged, 32768);
        storage(work.recovered, recovered, 32768);
        storage(work.bundles, descriptors, 63);
        storage(work.records, links, 63);
    }
};
struct SignedMemory {
    sr::Workspace work;
    std::vector<std::uint8_t> annotations, catalog;
    std::vector<sr::OriginRecord> origins;
    SignedMemory() {
        storage(work.annotations, annotations, 32768);
        storage(work.catalog, catalog, 32768);
        storage(work.origins, origins, 256);
    }
};
struct World;
struct Engine {
    World &world;
    bundle_review::Session store;
    larp::devices::ProgressJournal low;
    BundleMemory bundleMemory;
    rb::Journal bundle;
    Memory memory;
    ev::Owner owner;
    Engine(World &, bool initialize);
};
struct World {
    ProgramMemory program;
    bundle_review::Medium medium;
    RealHash hash;
    RealCrypto crypto;
    SigningOwner signer;
    Sources sources;
    InstallPolicy installs;
    InputPolicy inputs;
    Controls controls;
    Clock clock;
    Ids ids;
    Authority authority;
    IntentPolicy eligibility;
    Handler handler;
    Metadata metadata;
    ev::SerializedOrdering ordering;
    SignedMemory signedMemory;
    sr::SignedPlanEvidence evidence;
    explicit World(const std::vector<std::uint8_t> &raw)
        : sources(signer), metadata(program.program), ordering(metadata, id(0x5000)),
          evidence(hash, crypto, sources, installs, inputs, signedMemory.work) {
        requireCodec(rp::decodeCatalog(view(raw.data(), raw.size()), program.program).scope ==
                         rp::Scope::DecodedUnauthenticated,
                     "actual preserved compiler catalog");
        clock.boot = id(0x5000);
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
    }
    void enableSynthetic() {
        sources.available = installs.enabled = inputs.enabled = controls.evidence =
            controls.eligibility = authority.enabled = eligibility.enabled = true;
    }
};
Engine::Engine(World &w, bool initialize)
    : world(w), store(w.medium), low(store), bundle(low, w.hash, w.ids, bundleMemory.work),
      owner(w.program.program, low, bundle, bundleMemory.work, w.hash, w.ids, w.clock, w.ordering,
            w.evidence, w.controls, w.controls, w.eligibility, w.handler, memory.work) {
    larp::devices::ProgressOrigin origin;
    const auto &c = w.program.program.context;
    std::memcpy(origin.event.data(), c.event.bytes, 16);
    std::memcpy(origin.box.data(), c.box.bytes, 16);
    std::memcpy(origin.epoch.data(), c.sourceEpoch.bytes, 16);
    requireCodec((initialize ? low.initialize(origin).outcome : low.recover(origin).outcome) ==
                     (initialize ? larp::devices::ProgressOutcome::Initialized
                                 : larp::devices::ProgressOutcome::Recovered),
                 "actual low journal lifecycle");
    w.metadata.owner = &owner;
}
void pump(ev::Owner &owner) {
    for (unsigned i = 0; i < 40; ++i) {
        const auto p = owner.phase();
        if (p != ev::Phase::WritingCatalog && p != ev::Phase::WritingControl &&
            p != ev::Phase::WritingApplication && p != ev::Phase::Dispatching)
            return;
        owner.service();
    }
    requireCodec(false, "bounded public worker continuation");
}
void activate(Engine &e) {
    requireCodec(e.owner.recover().error == ev::Error::None, "actual semantic empty recovery");
    e.owner.activate(id(101), request(101));
    pump(e.owner);
    requireCodec(e.owner.phase() == ev::Phase::Stopped, "actual durable activation stopped");
    e.owner.control(ev::ControlKind::Start, id(102), request(102));
    pump(e.owner);
    requireCodec(e.owner.phase() == ev::Phase::Ready && e.owner.state().mode == xr::Mode::Running,
                 "actual durable start");
}
struct Manual {
    std::vector<xr::ManualReplay> rows;
    std::vector<std::uint8_t> catalog;
    xr::ManualWorkspace work;
    std::unique_ptr<xr::ManualOverrideOwner> owner;
    Manual(World &w, Engine &e) : rows(16), catalog(32768) {
        work.replays.data = rows.data();
        work.replays.capacity = rows.size();
        work.catalogBytes.data = catalog.data();
        work.catalogBytes.capacity = catalog.size();
        owner.reset(new xr::ManualOverrideOwner(w.program.program, e.owner.state(), w.clock.boot,
                                                30, w.hash, w.clock, w.authority, w.ids, e.owner,
                                                work));
    }
    xr::ManualResult replacement(World &w, bool finish = true) {
        xr::Identity rule;
        for (std::size_t i = 0; i < w.program.program.rules.count; ++i)
            if (w.program.program.rules.data[i].ir.policy == 3)
                rule = w.program.program.rules.data[i].stable;
        xr::ManualView v;
        requireCodec(owner->view(rule, w.authority.actor, v).status == xr::ManualStatus::Prepared,
                     "actual manual view");
        xr::ManualPrepareRequest p;
        p.ownerBoot = v.ownerBoot;
        p.phase = id(201);
        p.rule = v.rule;
        p.view = v.digest;
        p.authority = w.authority.actor;
        const auto prepared = owner->prepare(p);
        if (prepared.status == xr::ManualStatus::AlreadyReleased)
            return prepared;
        requireCodec(prepared.status == xr::ManualStatus::Prepared, "actual manual prepare");
        xr::ManualConfirmRequest c;
        c.ownerBoot = w.clock.boot;
        c.token = prepared.token;
        c.phase = id(202);
        c.authority = w.authority.actor;
        auto result = owner->confirm(c);
        for (unsigned i = 0; finish && i < 40 &&
                             (result.status == xr::ManualStatus::Confirming ||
                              result.status == xr::ManualStatus::Deferred);
             ++i)
            result = owner->service();
        return result;
    }
};
void observe(const char *name, Engine &engine) {
    requireCodec(engine.owner.phase() == ev::Phase::Ready ||
                     engine.owner.phase() == ev::Phase::Stopped,
                 "only complete published owner can export original envelope observations");
    for (std::size_t i = 0; i < engine.memory.work.applications.count; ++i) {
        const auto &entry = engine.memory.work.applications.data[i];
        const ev::CatalogEntry *catalog = nullptr;
        for (std::size_t j = 0; j < engine.memory.work.catalogs.count; ++j)
            if (!std::memcmp(engine.memory.work.catalogs.data[j].digest.bytes, entry.catalog.bytes,
                             32))
                catalog = &engine.memory.work.catalogs.data[j];
        requireCodec(catalog && entry.first <= engine.memory.work.history.count &&
                         entry.count <= engine.memory.work.history.count - entry.first,
                     "actual owned saved application/catalog boundaries");
        std::cout << "{\"observation\":\"" << name << "\",\"transaction\":\""
                  << hex(engine.memory.work.history.data + entry.first, entry.count)
                  << "\",\"catalog\":\""
                  << hex(engine.memory.work.history.data + catalog->first, catalog->count)
                  << "\"}\n";
    }
}

void exportFact(Engine &engine, const xr::Identity &operation, unsigned type) {
    std::vector<std::uint8_t> bytes(2048);
    Pool<std::uint8_t> out;
    out.data = bytes.data();
    out.capacity = bytes.size();
    rp::SignedFact fact;
    requireCodec(engine.owner.outbox(operation, 0, fact, out) &&
                     static_cast<unsigned>(fact.ref.type) == type,
                 "actual final-only signed output type");
    const auto &program = engine.world.program.program;
    bool localSubject = false;
    for (std::size_t i = 0; i < program.subjects.count; ++i) {
        const auto &b = program.subjects.data[i].binding;
        localSubject |= !std::memcmp(b.box.bytes, program.context.box.bytes, 16) &&
                        !std::memcmp(b.subject.bytes, fact.ref.subject.bytes, 16);
    }
    requireCodec(localSubject &&
                     !std::memcmp(fact.ref.id.event.bytes, program.context.event.bytes, 16) &&
                     !std::memcmp(fact.ref.id.origin.bytes, program.context.box.bytes, 16) &&
                     !std::memcmp(fact.ref.id.epoch.bytes, program.context.sourceEpoch.bytes, 16) &&
                     fact.ref.id.sequence == engine.owner.state().fence.lastIssued,
                 "actual outbox retains own origin/epoch/subject/assigned sequence binding");

    std::cout << "{\"cose\":\"" << hex(out.data, out.count) << "\",\"xy\":\""
              << hex(fact.publicKey, 64) << "\",\"expectedType\":" << type << "}\n";
}
xr::Input remote(World &world) {
    const auto &program = world.program.program;
    const xr::Subject *subject = nullptr;
    for (std::size_t i = 0; i < program.subjects.count; ++i)
        if (std::memcmp(program.subjects.data[i].binding.box.bytes, program.context.box.bytes,
                        16)) {
            subject = &program.subjects.data[i];
            break;
        }
    requireCodec(subject != nullptr, "actual remote trigger subject from compiled catalog");
    auto &binding = subject->binding;
    world.sources.saved.emplace_back();
    auto &saved = world.sources.saved.back();
    auto &ref = saved.fact.ref;
    ref.id.event = program.context.event;
    ref.id.origin = binding.box;
    ref.id.epoch = binding.epochs.count ? binding.epochs.data[0] : binding.epoch;
    ref.id.sequence = 1;
    ref.deployment = program.context.deployment;
    ref.binding = binding.binding;
    ref.run = program.context.run;
    ref.station = binding.station;
    ref.subject = binding.subject;
    ref.type = xr::FactType::TaskSolved;
    d0::Fact f;
    std::memcpy(f.event.bytes, ref.id.event.bytes, 16);
    std::memcpy(f.origin.bytes, ref.id.origin.bytes, 16);
    std::memcpy(f.epoch.bytes, ref.id.epoch.bytes, 16);
    std::memcpy(f.run.bytes, ref.run.bytes, 16);
    std::memcpy(f.station.bytes, ref.station.bytes, 16);
    std::memcpy(f.subject.bytes, ref.subject.bytes, 16);
    std::memcpy(f.binding.bytes, ref.binding.bytes, 32);
    std::memcpy(f.package.bytes, ref.deployment.bytes, 32);
    f.sequence = 1;
    f.type = 1;
    f.generation = 1;
    d0::FactWriteWorkspace workspace;
    d0::FrozenFact frozen;
    const auto wrote =
        d0::writeFact(f, world.hash, world.signer.get(), world.crypto, workspace, frozen);
    requireCodec(wrote.scope == d0::WriteScope::SignedUnpublished,
                 "actual original remote proof signed/verified");
    saved.bytes.assign(frozen.original, frozen.original + frozen.length);
    std::memcpy(ref.digest.bytes, frozen.payloadDigest.bytes, 32);
    std::memcpy(saved.fact.publicKey, wrote.publicKey.x, 32);
    std::memcpy(saved.fact.publicKey + 32, wrote.publicKey.y, 32);
    saved.fact.cose = view(saved.bytes.data(), saved.bytes.size());
    std::cout << "{\"cose\":\"" << hex(saved.bytes.data(), saved.bytes.size()) << "\",\"xy\":\""
              << hex(saved.fact.publicKey, 64) << "\",\"expectedType\":1}\n";
    xr::Input input;
    input.context = program.context;
    input.kind = xr::InputKind::Fact;
    input.fact.subject = static_cast<Id>(subject - program.subjects.data + 1);
    input.fact.ref = ref;
    input.fact.kind = xr::FactClass::CurrentSolved;
    return input;
}
void onceChains(const std::vector<std::uint8_t> &catalog) {
    for (bool originalFirst : {false, true}) {
        World world(catalog);
        world.enableSynthetic();
        Engine engine(world, true);
        activate(engine);
        Manual manual(world, engine);
        const auto input = remote(world);
        xr::ManualResult replacement;
        if (!originalFirst) {
            replacement = manual.replacement(world);
            requireCodec(replacement.status == xr::ManualStatus::Committed &&
                             replacement.release == xr::ReleaseDisposition::Released,
                         "replacement wins actual shared group");
            exportFact(engine, replacement.operation, 2);
        }
        const auto prior = engine.owner.state().fence.lastIssued;
        const auto admitted = engine.owner.submit(input, id(301), request(301));
        requireCodec(admitted.error == ev::Error::None,
                     "actual original input admitted before continuation");
        pump(engine.owner);
        requireCodec(
            engine.owner.phase() == ev::Phase::Ready && engine.owner.state().latches.count == 1 &&
                engine.owner.state().solved.count == 1 && engine.memory.work.intents.count == 1,
            "original/replacement chain keeps exactly one release and intent");
        requireCodec(engine.owner.state().fence.lastIssued == (originalFirst ? prior + 1 : prior),
                     "late original cannot allocate a second fact");
        if (originalFirst) {
            exportFact(engine, id(301), 1);
            replacement = manual.replacement(world);
            requireCodec(replacement.status == xr::ManualStatus::AlreadyReleased &&
                             replacement.release == xr::ReleaseDisposition::Released,
                         "late replacement refers only to existing original group");
        }

        const auto committedOriginal = engine.owner.status(world.program.program.context, id(301));
        requireCodec(committedOriginal.status == xr::ManualStatus::Committed,
                     "first original uptake is a verified committed receipt before retry");
        bool storedFact = false;
        for (std::size_t i = 0; i < engine.owner.state().facts.count; ++i) {
            const auto &f = engine.owner.state().facts.data[i];
            storedFact |= f.subject == input.fact.subject &&
                          f.ref.id.sequence == input.fact.ref.id.sequence &&
                          !std::memcmp(f.ref.id.origin.bytes, input.fact.ref.id.origin.bytes, 16) &&
                          !std::memcmp(f.ref.digest.bytes, input.fact.ref.digest.bytes, 32);
        }
        xr::Identity stable;
        for (std::size_t i = 0; i < world.program.program.rules.count; ++i)
            if (world.program.program.rules.data[i].ir.policy == 3)
                stable = world.program.program.rules.data[i].stable;
        bool ownedSolved = false;
        for (std::size_t i = 0; i < engine.owner.state().solved.count; ++i) {
            const auto index = engine.owner.state().solved.data[i];
            ownedSolved |=
                index && index <= world.program.program.subjects.count &&
                !std::memcmp(world.program.program.subjects.data[index - 1].binding.box.bytes,
                             world.program.program.context.box.bytes, 16);
        }
        requireCodec(
            storedFact && ownedSolved &&
                !std::memcmp(engine.owner.state().latches.data[0].rule.bytes, stable.bytes, 16),
            "actual source Fact plus own solved subject and stable group latch bound before "
            "duplicate");
        const auto records = engine.low.snapshot().records;
        const auto duplicate = engine.owner.submit(input, id(301), request(301));
        requireCodec(duplicate.journal.outcome == rb::Outcome::Duplicate &&
                         engine.low.snapshot().records == records,
                     "actual original duplicate never rewrites or releases twice");
        const auto appCount = engine.memory.work.applications.count;
        const auto history = engine.memory.work.history.count;
        const auto sources = world.sources.locals + world.sources.inputs + world.sources.lookups;
        const auto metadataCalls = world.metadata.calls;
        const auto programs = engine.store.programs;
        const auto correlated = engine.owner.submit(input, id(302), request(999));
        requireCodec(
            correlated.journal.outcome == rb::Outcome::Duplicate &&
                !std::memcmp(correlated.originalApplication.bytes, id(301).bytes, 16) &&
                correlated.journal.journal.revision == duplicate.journal.journal.revision &&
                correlated.journal.journal.index == duplicate.journal.journal.index &&
                correlated.journal.journal.firstIssued == duplicate.journal.journal.firstIssued &&
                correlated.journal.journal.lastIssued == duplicate.journal.journal.lastIssued &&
                engine.low.snapshot().records == records &&
                engine.memory.work.applications.count == appCount &&
                engine.memory.work.history.count == history,
            "same durable Fact under fresh operation reports original stand without new receipt");
        requireCodec(world.sources.locals + world.sources.inputs + world.sources.lookups ==
                             sources &&
                         world.metadata.calls == metadataCalls &&
                         engine.store.programs == programs && !world.ordering.mutationsBlocked(),
                     "new correlation performs no current source/signing/write/lease");
        const auto notStored = engine.owner.status(world.program.program.context, id(302));
        requireCodec(notStored.status != xr::ManualStatus::Committed &&
                         notStored.status != xr::ManualStatus::Duplicate,
                     "new correlation operation was not admitted as its own receipt");
        requireCodec(engine.owner.submit(input, id(102), request(999)).error == ev::Error::Conflict,
                     "known Control phase cannot be reused as new Fact correlation");
        const auto claim = engine.memory.work.intents.data[0].reservation.claim;
        requireCodec(engine.owner.submit(input, claim, request(999)).error == ev::Error::Conflict,
                     "reserved Claim phase cannot be reused as correlation");
        if (!originalFirst)
            requireCodec(engine.owner.submit(input, replacement.operation, request(999)).error ==
                             ev::Error::Conflict,
                         "another actual application operation remains conflict");
        auto digestConflict = input;
        digestConflict.fact.ref.digest.bytes[0] ^= 1;
        requireCodec(engine.owner.submit(digestConflict, id(304), request(999)).error ==
                         ev::Error::Conflict,
                     "same FactID different digest conflicts under new operation");
        auto metadataConflict = input;
        metadataConflict.fact.ref.binding.bytes[0] ^= 1;
        requireCodec(engine.owner.submit(metadataConflict, id(304), request(999)).error ==
                         ev::Error::Conflict,
                     "same FactID/digest changed immutable metadata conflicts");

        auto changed = input;
        changed.fact.ref.id.sequence++;
        requireCodec(engine.owner.submit(changed, id(301), request(301)).error ==
                             ev::Error::Conflict &&
                         engine.low.snapshot().records == records,
                     "same operation changed original input conflicts without write");
        changed = input;
        changed.kind = xr::InputKind::Local;
        requireCodec(engine.owner.submit(changed, id(301), request(301)).error ==
                         ev::Error::Conflict,
                     "same operation changed input kind conflicts");
        requireCodec(engine.owner.submit(input, id(301), request(302)).error == ev::Error::Conflict,
                     "same operation changed request conflicts");
        observe(originalFirst ? "original-first" : "replacement-first", engine);
    }
}

bool physical(Engine &engine, const xr::Identity &operation) {
    for (unsigned i = 0; i < engine.low.snapshot().records; ++i) {
        larp::devices::ProgressRecord record;
        std::uint8_t bytes[3072];
        if (engine.low.readRecord(static_cast<std::uint8_t>(i), record, bytes, sizeof(bytes))
                .outcome != larp::devices::ProgressOutcome::Read)
            return false;
        if (!std::memcmp(record.operationId.data(), operation.bytes, 16))
            return true;
    }
    return false;
}
void missingAndCapacity(const std::vector<std::uint8_t> &catalog) {
    for (bool evidence : {false, true}) {
        World world(catalog);
        Engine engine(world, true);
        requireCodec(engine.owner.recover().error == ev::Error::None,
                     "initial actual empty reconstruction before missing-control gate");
        world.controls.evidence = evidence;
        requireCodec(
            engine.owner.activate(id(101), request(101)).error != ev::Error::None &&
                engine.low.snapshot().records == 0 && engine.memory.work.catalogs.count == 0,
            "missing Control evidence or separate live preflight closes BEFORE catalog write");
    }
    {
        World world(catalog);
        world.enableSynthetic();
        Engine engine(world, true);
        activate(engine);
        Manual manual(world, engine);
        const auto before = engine.low.snapshot().records;
        world.sources.available = false;
        const auto denied = manual.replacement(world);
        requireCodec(denied.status != xr::ManualStatus::Committed &&
                         denied.status != xr::ManualStatus::Duplicate &&
                         engine.low.snapshot().records == before &&
                         engine.owner.state().latches.count == 0,
                     "missing original plan source cannot publish unsigned/keyless fallback");
        world.sources.available = true;
        const auto input = remote(world);
        engine.owner.submit(input, id(301), request(301));
        pump(engine.owner);
        requireCodec(engine.owner.status(world.program.program.context, id(301)).status ==
                         xr::ManualStatus::Committed,
                     "healthy actual original path reusable after source failure");
        exportFact(engine, id(301), 1);
    }
    {
        World world(catalog);
        world.enableSynthetic();
        Engine engine(world, true);
        activate(engine);
        Manual manual(world, engine);
        const auto before = engine.low.snapshot().records;
        const auto history = engine.memory.work.history.count;
        engine.memory.work.prepared.outbox.capacity = 0;
        const auto deferred = manual.replacement(world, false);
        const auto record = manual.rows[0].record;
        requireCodec((deferred.status == xr::ManualStatus::Deferred ||
                      deferred.status == xr::ManualStatus::Confirming) &&
                         engine.low.snapshot().records == before &&
                         engine.memory.work.history.count == history &&
                         !physical(engine, record.operation) && !world.ordering.mutationsBlocked(),
                     "capacity exhaustion before reservation acquires no record/lease/history");
        engine.memory.work.prepared.outbox.capacity = 8;
        auto done = manual.owner->service();
        for (unsigned i = 0; i < 40 && done.status == xr::ManualStatus::Confirming; ++i)
            done = manual.owner->service();
        requireCodec(done.status == xr::ManualStatus::Committed &&
                         xr::sameOverrideRecord(record, manual.rows[0].record),
                     "capacity restoration retries original accepted immutable operation");
    }
}
void intentEligibility(const std::vector<std::uint8_t> &catalog) {
    World world(catalog);
    world.enableSynthetic();
    Engine engine(world, true);
    activate(engine);
    Manual manual(world, engine);
    const auto app = manual.replacement(world);
    const auto records = engine.low.snapshot().records;
    const auto claim = engine.memory.work.intents.data[0].reservation.claim;
    world.eligibility.enabled = false;
    const auto denied = engine.owner.dispatch(app.operation, 0);
    requireCodec(denied.error != ev::Error::None && engine.low.snapshot().records == records &&
                     !physical(engine, claim) && world.handler.calls == 0 &&
                     !world.ordering.mutationsBlocked(),
                 "claim prewrite denied only with absent actual physical phase and no handler");
    world.eligibility.enabled = true;
    requireCodec(engine.owner.dispatch(app.operation, 0).error == ev::Error::None &&
                     engine.memory.work.intents.data[0].stage == ev::IntentStage::Returned &&
                     engine.memory.work.intents.data[0].resultCommitted && world.handler.calls == 1,
                 "eligible actual Claim -> single handler -> durable Returned result");
    const auto finalRecords = engine.low.snapshot().records;
    const auto calls = world.handler.calls;
    requireCodec(engine.owner.dispatch(app.operation, 0).journal.outcome ==
                         rb::Outcome::Duplicate &&
                     engine.low.snapshot().records == finalRecords && world.handler.calls == calls,
                 "settled intent duplicate never redelivers");
}
void partialWrites(const std::vector<std::uint8_t> &catalog) {
    for (std::size_t budget : {std::size_t(0), std::size_t(1), std::size_t(20)}) {
        World world(catalog);
        world.enableSynthetic();
        Engine engine(world, true);
        activate(engine);
        Manual manual(world, engine);
        const auto before = world.medium.durable;
        engine.store.budget = budget;
        const auto unknown = manual.replacement(world, false);
        requireCodec(unknown.status == xr::ManualStatus::Unknown &&
                         engine.owner.phase() == ev::Phase::Unknown &&
                         world.ordering.mutationsBlocked() &&
                         engine.owner.state().fence.lastIssued == 0 &&
                         engine.owner.state().latches.count == 0 && world.handler.calls == 0,
                     "partial write cannot claim prewrite denial or publish candidate progress");
        const auto image = world.medium.durable;
        const auto programs = engine.store.programs;
        engine.owner.service();
        engine.owner.dispatch(unknown.operation, 0);
        manual.owner->service();
        requireCodec(world.medium.durable == image && engine.store.programs == programs,
                     "Unknown cannot retry through live cache or reset any header");
        engine.store.loseSession();
        bundle_review::Session fresh(world.medium);
        larp::devices::ProgressJournal reopened(fresh);
        larp::devices::ProgressOrigin origin;
        const auto &context = world.program.program.context;
        std::memcpy(origin.event.data(), context.event.bytes, 16);
        std::memcpy(origin.box.data(), context.box.bytes, 16);
        std::memcpy(origin.epoch.data(), context.sourceEpoch.bytes, 16);
        const auto recovered = reopened.recover(origin);
        requireCodec(
            (recovered.outcome == larp::devices::ProgressOutcome::Recovered) == (budget == 0),
            "genuine zero-byte reopen can recover; actually partial durable record closes");
        requireCodec(world.medium.durable == image,
                     "qualified recovery never repairs/resets durable bytes");
        if (budget)
            requireCodec(world.medium.durable != before,
                         "actual partial bytes retained as evidence, never rolled back");
    }
}

void noRelease(const std::vector<std::uint8_t> &catalog) {
    World world(catalog);
    world.enableSynthetic();
    Engine engine(world, true);
    activate(engine);
    Manual manual(world, engine);
    const auto receipt = manual.replacement(world);
    requireCodec(
        receipt.status == xr::ManualStatus::Committed &&
            receipt.release == xr::ReleaseDisposition::NoRelease &&
            engine.owner.state().latches.count == 0 && engine.owner.state().solved.count == 0 &&
            engine.owner.state().fence.lastIssued == 0 && engine.memory.work.intents.count == 0,
        "actual source-compiled false guard durable receipt has no replacement "
        "release/output/intent");
    std::vector<std::uint8_t> bytes(2048);
    Pool<std::uint8_t> output;
    output.data = bytes.data();
    output.capacity = bytes.size();
    rp::SignedFact fact;
    requireCodec(!engine.owner.outbox(receipt.operation, 0, fact, output),
                 "NoRelease has no public T2 outbox");
    const auto input = remote(world);
    engine.owner.submit(input, id(301), request(301));
    pump(engine.owner);
    exportFact(engine, id(301), 1);
    requireCodec(engine.owner.state().latches.count == 1 &&
                     engine.owner.state().solved.count == 1 &&
                     engine.memory.work.intents.count == 1,
                 "later actual original may fulfill still-unreleased group");
    const auto stored = engine.owner.status(world.program.program.context, receipt.operation);
    requireCodec(stored.status == xr::ManualStatus::Committed &&
                     stored.release == xr::ReleaseDisposition::NoRelease,
                 "later original cannot relabel committed NoRelease receipt");
    xr::ManualConfirmRequest c;
    c.ownerBoot = world.clock.boot;
    c.token = manual.rows[0].record.token;
    c.phase = manual.rows[0].record.confirmId;
    c.authority = world.authority.actor;
    requireCodec(manual.owner->confirm(c).release == xr::ReleaseDisposition::NoRelease,
                 "same accepted Confirm remains NoRelease after later original");
    observe("no-release-then-original", engine);
}
void finalFences(const std::vector<std::uint8_t> &catalog) {
    for (unsigned mode = 0; mode < 3; ++mode) {
        World world(catalog);
        world.enableSynthetic();
        Engine engine(world, true);
        activate(engine);
        Manual manual(world, engine);
        if (mode == 2)
            world.sources.callback = [&]() { world.clock.ticks += 31; };
        const auto pending = manual.replacement(world, false);
        const auto original = manual.rows[0].record;
        if (mode == 0)
            world.authority.enabled = false;
        if (mode == 1)
            world.clock.ticks += 31;
        auto result = pending;
        for (unsigned i = 0; i < 40 && (result.status == xr::ManualStatus::Confirming ||
                                        result.status == xr::ManualStatus::Deferred);
             ++i)
            result = manual.owner->service();
        requireCodec(result.status != xr::ManualStatus::Committed &&
                         result.status != xr::ManualStatus::Duplicate &&
                         result.release != xr::ReleaseDisposition::Released,
                     "revoke/absolute TTL/expensive callback cannot reach released Final");
        requireCodec(engine.owner.state().latches.count == 0 &&
                         engine.owner.state().solved.count == 0 &&
                         engine.owner.state().fence.lastIssued == 0 && world.handler.calls == 0,
                     "denied Final publishes no progress/output/handoff");
        requireCodec(xr::sameOverrideRecord(original, manual.rows[0].record),
                     "failed Final never rewrites original accepted Confirm identity/deadline");
        std::vector<std::uint8_t> bytes(2048);
        Pool<std::uint8_t> output;
        output.data = bytes.data();
        output.capacity = bytes.size();
        rp::SignedFact fact;
        requireCodec(!engine.owner.outbox(original.operation, 0, fact, output),
                     "frozen signed candidate remains private before actual Final");
    }
}
void lateReplayFailure(const std::vector<std::uint8_t> &catalog) {
    World world(catalog);
    world.enableSynthetic();
    std::unique_ptr<Engine> engine(new Engine(world, true));
    activate(*engine);
    const auto input = remote(world);
    engine->owner.submit(input, id(301), request(301));
    pump(engine->owner);
    requireCodec(engine->owner.status(world.program.program.context, id(301)).status ==
                     xr::ManualStatus::Committed,
                 "verified prefix originally committed");
    world.sources.available = false;
    world.sources.saved.clear();
    const auto originalSourceCalls =
        world.sources.locals + world.sources.inputs + world.sources.lookups;
    engine->store.loseSession();
    engine.reset(new Engine(world, false));
    requireCodec(engine->owner.recover().error == ev::Error::None,
                 "complete cold original replay before duplicate query");
    requireCodec(
        world.sources.locals + world.sources.inputs + world.sources.lookups == originalSourceCalls,
        "actual original proof buffers destroyed; cold history consults no current sources");
    const auto before = engine->low.snapshot().records;
    const auto priorSources = world.sources.locals + world.sources.inputs + world.sources.lookups;
    const auto old = engine->owner.submit(input, id(301), request(301));
    requireCodec(
        old.journal.outcome == rb::Outcome::Duplicate && engine->low.snapshot().records == before &&
            world.sources.locals + world.sources.inputs + world.sources.lookups == priorSources,
        "cold complete-history duplicate returns original without planning/signing/source calls");
    const auto correlated = engine->owner.submit(input, id(302), request(999));
    requireCodec(correlated.journal.outcome == rb::Outcome::Duplicate &&
                     !std::memcmp(correlated.originalApplication.bytes, id(301).bytes, 16) &&
                     correlated.journal.journal.revision == old.journal.journal.revision &&
                     correlated.journal.journal.index == old.journal.journal.index &&
                     correlated.journal.journal.firstIssued == old.journal.journal.firstIssued &&
                     correlated.journal.journal.lastIssued == old.journal.journal.lastIssued &&
                     engine->low.snapshot().records == before &&
                     world.sources.locals + world.sources.inputs + world.sources.lookups ==
                         priorSources,
                 "cold new correlation/digest resolves only actual original durable application");
    observe("cold-original-before-hostile-tail", *engine);

    const std::uint8_t unsupported[] = {0xa0};
    larp::devices::ProgressRequest bad;
    bad.kind = larp::devices::ProgressKind::Received;
    bad.operationId[15] = 99;
    bad.expectedRevision = engine->low.snapshot().revision;
    bad.expectedLastIssued = engine->low.snapshot().lastIssued;
    bad.payload = unsupported;
    bad.payloadBytes = 1;
    std::size_t written = 0;
    requireCodec(world.hash.begin() && world.hash.update(unsupported, 1) &&
                     world.hash.finish(bad.requestDigest.data(), written) && written == 32 &&
                     world.hash.abort(),
                 "actual raw SHA of deliberate later unsupported semantic record");
    struct Fence final : larp::devices::IProgressCommitFence {
        bool allow(const larp::devices::ProgressRecord &) override {
            return true;
        }
    } fence;
    requireCodec(engine->low.commit(bad, fence).outcome ==
                     larp::devices::ProgressOutcome::Committed,
                 "actual intact later hostile low-level record");
    world.sources.available = false;
    engine->store.loseSession();
    engine.reset(new Engine(world, false));
    requireCodec(engine->owner.recover().error != ev::Error::None &&
                     engine->owner.phase() == ev::Phase::Fault,
                 "late semantic replay failure closes entire recovery");
    const auto status = engine->owner.status(world.program.program.context, id(301));
    requireCodec(status.status != xr::ManualStatus::Committed &&
                     status.status != xr::ManualStatus::Duplicate,
                 "no committed prefix status after failed whole recovery");
    const auto duplicate = engine->owner.submit(input, id(301), request(301));
    requireCodec(duplicate.journal.outcome != rb::Outcome::Duplicate &&
                     duplicate.error != ev::Error::None,
                 "no duplicate prefix submit after failed whole recovery");
    std::vector<std::uint8_t> bytes(2048);
    Pool<std::uint8_t> out;
    out.data = bytes.data();
    out.capacity = bytes.size();
    rp::SignedFact fact;
    requireCodec(!engine->owner.outbox(id(301), 0, fact, out),
                 "no prefix outbox before complete replay publication");
}

void intentResultUnknown(const std::vector<std::uint8_t> &catalog) {
    World world(catalog);
    world.enableSynthetic();
    std::unique_ptr<Engine> engine(new Engine(world, true));
    activate(*engine);
    Manual manual(world, *engine);
    const auto committed = manual.replacement(world);
    requireCodec(committed.status == xr::ManualStatus::Committed &&
                     committed.release == xr::ReleaseDisposition::Released,
                 "actual ManualOwner->Coordinator replacement committed with release");
    requireCodec(engine->memory.work.intents.count == 1 && world.handler.calls == 0,
                 "actual intent staged without handler handoff");
    std::vector<std::uint8_t> cose(2048);
    Pool<std::uint8_t> output;
    output.data = cose.data();
    output.capacity = cose.size();
    rp::SignedFact fact;
    requireCodec(engine->owner.outbox(committed.operation, 0, fact, output) &&
                     fact.ref.type == xr::FactType::LocalOverride,
                 "actual committed T2 outbox only");
    std::cout << "{\"cose\":\"" << hex(output.data, output.count) << "\",\"xy\":\""
              << hex(fact.publicKey, 64) << "\",\"expectedType\":2}\n";
    world.handler.after = [&]() {
        const auto state = engine->low.snapshot();
        larp::devices::ProgressRecord record;
        std::uint8_t raw[3072];
        requireCodec(
            state.records &&
                engine->low.readRecord(state.records - 1, record, raw, sizeof(raw)).outcome ==
                    larp::devices::ProgressOutcome::Read &&
                record.kind == larp::devices::ProgressKind::EffectClaim,
            "actual durable Claim exists before synthetic handler handoff");
        engine->store.budget = 0;
    };
    engine->owner.dispatch(committed.operation, 0);
    requireCodec(world.handler.calls == 1 && engine->owner.phase() == ev::Phase::Unknown &&
                     world.ordering.mutationsBlocked(),
                 "actual Claim->one handler->Result Unknown");
    ev::OrderingSnapshot pin;
    requireCodec(world.ordering.beginRecovery(pin) == ev::OrderingInventory::Held &&
                     pin.lease.binding.kind == ev::CommitKind::Result,
                 "actual coordinator uncertainty retains Result lease");
    world.ordering.abortRecovery(pin);
    const auto locals = world.sources.locals, inputs = world.sources.inputs,
               lookups = world.sources.lookups;
    world.sources.available = false;
    world.clock.boot = id(0x6000);
    world.clock.ticks = 2000;
    engine->store.loseSession();
    engine.reset(new Engine(world, false));
    requireCodec(engine->owner.recover().error == ev::Error::None,
                 "actual new-session semantic recovery of Result Unknown");
    requireCodec(world.sources.locals == locals && world.sources.inputs == inputs &&
                     world.sources.lookups == lookups && world.handler.calls == 1,
                 "cold history uses no current sources or second handoff");
    requireCodec(engine->memory.work.intents.count == 1 &&
                     engine->memory.work.intents.data[0].stage == ev::IntentStage::Unknown,
                 "recovered attempted intent remains Unknown");
    const auto status = engine->owner.status(world.program.program.context, committed.operation);
    requireCodec(status.status == xr::ManualStatus::Committed &&
                     status.release == xr::ReleaseDisposition::Released,
                 "actual original semantic receipt remains after uncertain effect");
    const auto originalClaimFence = engine->memory.work.intents.data[0].originalClaimFence;
    const auto originalClaimBoot = engine->memory.work.intents.data[0].originalClaimBoot;
    engine->owner.control(ev::ControlKind::RestoreStopped, id(103), request(103));
    pump(engine->owner);
    requireCodec(engine->owner.phase() == ev::Phase::Stopped &&
                     engine->owner.state().mode == xr::Mode::Stopped,
                 "actual new lifecycle RestoreStopped between Claim and Result");
    const auto eligibilityCalls = world.eligibility.calls;
    const auto settled = engine->owner.settleUnknown(committed.operation, 0);
    requireCodec(settled.error == ev::Error::None &&
                     engine->memory.work.intents.data[0].resultCommitted &&
                     world.handler.calls == 1 && world.eligibility.calls == eligibilityCalls,
                 "actual durable Unknown settlement creates no new eligibility or handoff");
    larp::devices::ProgressRecord record;
    std::uint8_t raw[3072];
    const auto snapshot = engine->low.snapshot();
    requireCodec(engine->low.readRecord(snapshot.records - 1, record, raw, sizeof(raw)).outcome ==
                     larp::devices::ProgressOutcome::Read,
                 "read actual settled Result record");
    ev::Effect effect;
    requireCodec(ev::decodeEffect(raw, record.payloadBytes, effect).valid &&
                     effect.phase == ev::EffectPhase::Result &&
                     effect.result == ev::HandlerResult::Unknown &&
                     xr::sameFence(effect.expected, originalClaimFence) &&
                     !std::memcmp(effect.ownerBoot.bytes, originalClaimBoot.bytes, 16),
                 "Result retains original ClaimFence/Boot despite newer control fence and boot");
    engine->store.loseSession();
    engine.reset(new Engine(world, false));
    requireCodec(engine->owner.recover().error == ev::Error::None &&
                     engine->memory.work.intents.data[0].resultCommitted &&
                     engine->memory.work.intents.data[0].stage == ev::IntentStage::Unknown &&
                     world.handler.calls == 1,
                 "fresh session retains settled Unknown without repeated handler");
    observe("result-unknown-settled-reopened", *engine);
}
} // namespace owner_review
int main() {
    std::string encoded, noReleaseEncoded;
    std::getline(std::cin, encoded);
    std::getline(std::cin, noReleaseEncoded);
    const auto catalog = unhex(encoded);
    owner_review::intentResultUnknown(catalog);
    owner_review::onceChains(catalog);
    owner_review::lateReplayFailure(catalog);
    owner_review::noRelease(unhex(noReleaseEncoded));
    owner_review::finalFences(catalog);
    owner_review::missingAndCapacity(catalog);
    owner_review::intentEligibility(catalog);
    owner_review::partialWrites(catalog);
    std::cout << "{\"checks\":" << checks
              << ",\"scope\":\"SYNTHETIC_ORIGINAL_AUTHORITY_PORTS_REAL_OWNER_JOURNAL_CRYPTO\"}\n";
}
