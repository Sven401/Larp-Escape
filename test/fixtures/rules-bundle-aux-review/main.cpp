#include "../rules-bundle-review/Store.h"
#include "../rules-planner-review/CompilerSupport.h"
#include "DeviceRuntime/RulesBundleJournal.h"
#include "DeviceRuntime/RulesProgressControl.h"
#include "GameExecution/RulesProgressCodec.h"
#include <cstdlib>
namespace rb = larp::devices::rules_bundle;
namespace rp = larp::devices::rules_progress;
namespace xr = larp::execution::rules;
namespace codec = larp::execution::rules_progress;
using namespace larp::devices;
namespace {
unsigned checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
template <typename T> void pool(larp::configuration::module_ir::Pool<T> &p, std::vector<T> &v) {
    p.data = v.data();
    p.capacity = v.size();
    p.count = 0;
}
struct Memory {
    rb::Workspace w;
    std::vector<std::uint8_t> stage, recovered;
    std::vector<rb::Descriptor> descriptors;
    std::vector<rb::RecordLink> links;
    Memory() : stage(32768), recovered(32768), descriptors(63), links(63) {
        pool(w.staged, stage);
        pool(w.recovered, recovered);
        pool(w.bundles, descriptors);
        pool(w.records, links);
    }
};
struct Ids final : rb::IIdentitySource {
    unsigned next = 100;
    bool make(rb::Identity &id) override {
        id.fill(0);
        id[15] = static_cast<std::uint8_t>(next++);
        return true;
    }
};
// Synthetic eligibility only. No event, GM, hardware or setup authorization.
struct Fence final : IProgressCommitFence, rb::ICommitFence {
    bool accepted = true;
    unsigned calls = 0;
    bool allow(const ProgressRecord &) override {
        ++calls;
        return accepted;
    }
    bool allow(const rb::Key &, rb::Phase, const ProgressRecord &) override {
        ++calls;
        return accepted;
    }
};
// Provisional REPRESENTATION reducer; never installs or activates a program.
struct Consumer final : rb::IRecoveryConsumer {
    unsigned begins = 0, catalogs = 0, controls = 0, commits = 0, aborts = 0;
    bool rejectAux = false;
    bool begin(const ProgressSnapshot &) override {
        ++begins;
        return true;
    }
    bool catalog(const rb::Key &, const std::uint8_t *p, std::size_t n) override {
        xr::Program inactive;
        xr::Span<std::uint8_t> source;
        source.data = p;
        source.count = n;
        ++catalogs;
        return codec::decodeCatalog(source, inactive).scope == codec::Scope::DecodedUnauthenticated;
    }
    bool application(const rb::Key &, const ProgressRecord &, const std::uint8_t *,
                     std::size_t) override {
        return false;
    }
    bool auxiliary(const ProgressRecord &record, const std::uint8_t *p, std::size_t n) override {
        ++controls;
        if (record.kind != ProgressKind::Control)
            return !rejectAux;
        rp::Control decoded;
        std::uint8_t bytes[2048];
        xr::Pool<std::uint8_t> output;
        output.data = bytes;
        output.capacity = sizeof(bytes);
        return rp::decodeControl(p, n, decoded, output).valid && !rejectAux;
    }
    bool prepare() override {
        return true;
    }
    void commit() override {
        ++commits;
    }
    void abort() override {
        ++aborts;
    }
};
rb::Digest digest(Hash &hash, const std::uint8_t *p, std::size_t n) {
    rb::Digest out{};
    std::size_t written = 0;
    require(hash.abort() && hash.begin() && hash.update(p, n) && hash.finish(out.data(), written) &&
                written == 32 && hash.abort(),
            "real BCrypt SHA256");
    return out;
}
struct Fixture {
    bundle_review::Medium medium;
    bundle_review::Session store;
    ProgressJournal low;
    Hash hash;
    Ids ids;
    Memory memory;
    rb::Journal bundle;
    Fence fence;
    ProgressOrigin origin;
    rb::Key catalog;
    Fixture(bool addCatalog = true) : store(medium), low(store), bundle(low, hash, ids, memory.w) {
        origin.event[15] = 1;
        origin.box[15] = 2;
        origin.epoch[15] = 3;
        require(low.initialize(origin).outcome == ProgressOutcome::Initialized,
                "initialize actual journal");
        Consumer c;
        require(bundle.recover(c).outcome == rb::Outcome::Recovered, "initial empty recovery");
        xr::Program p;
        p.ready = true;
        p.context.event.bytes[15] = 1;
        p.context.box.bytes[15] = 2;
        p.context.sourceEpoch.bytes[15] = 3;
        p.context.run.bytes[15] = 4;
        p.context.station.bytes[15] = 5;
        p.context.deployment.bytes[31] = 12;
        require(codec::encodeCatalog(p, memory.w.staged).scope == codec::Scope::EncodedUnpublished,
                "actual empty canonical catalog codec");
        catalog.kind = rb::Kind::Catalog;
        catalog.operation[15] = 10;
        catalog.request[15] = 11;
        catalog.deployment[31] = 12;
        catalog.blob = digest(hash, memory.stage.data(), memory.w.staged.count);
        catalog.catalog = catalog.blob;
        catalog.bytes = static_cast<std::uint32_t>(memory.w.staged.count);
        catalog.chunks = static_cast<std::uint8_t>((catalog.bytes + 2047) / 2048);
        if (addCatalog) {
            require(bundle.begin(catalog, rb::HeaderMeta{}, 0, 0).outcome == rb::Outcome::Prepared,
                    "prepare real catalog bundle");
            require(bundle.step(fence).outcome == rb::Outcome::Progress, "real catalog chunk");
            require(bundle.step(fence).outcome == rb::Outcome::Committed, "real catalog Final");
        }
    }
    ProgressRequest control(unsigned fault, std::vector<std::uint8_t> &bytes) {
        rp::Control c;
        c.operation.bytes[15] = 20;
        c.request.bytes[31] = 21;
        std::memcpy(c.catalog.bytes, catalog.blob.data(), 32);
        std::memcpy(c.context.deployment.bytes, catalog.deployment.data(), 32);
        c.context.event.bytes[15] = 1;
        c.context.box.bytes[15] = 2;
        c.context.sourceEpoch.bytes[15] = 3;
        c.context.run.bytes[15] = 4;
        c.context.station.bytes[15] = 5;
        c.expected.progress = 7;
        c.expected.profile = 17;
        c.expected.trust = 23;
        c.expected.activeTime = 10;
        c.targetFence = c.expected;
        c.targetFence.activeTime = 15;
        c.kind = rp::ControlKind::Checkpoint;
        c.before = c.after = xr::Mode::Running;
        c.clock.boot.bytes[15] = 9;
        c.clock.monotonicBefore = 100;
        c.clock.monotonicAfter = 105;
        c.clock.activeBefore = 10;
        c.clock.activeAfter = 15;
        const std::uint8_t synthetic[] = {0xa0};
        c.authorization.data = synthetic;
        c.authorization.count = 1;
        if (fault == 1)
            c.catalog.bytes[0] ^= 1;
        if (fault == 2)
            c.context.deployment.bytes[0] ^= 1;
        if (fault == 7)
            c.expected.lastIssued = c.targetFence.lastIssued = 1;
        bytes.resize(3072);
        xr::Pool<std::uint8_t> out;
        pool(out, bytes);
        require(rp::encodeControl(c, out).valid, "actual Control codec");
        bytes.resize(out.count);
        const auto state = low.snapshot();
        ProgressRequest request;
        request.operationId[15] = fault == 3 ? 22 : 20;
        request.kind = ProgressKind::Control;
        request.expectedRevision = state.revision;
        request.expectedLastIssued = state.lastIssued;
        request.profileRevision = fault == 4 ? 18 : 17;
        request.trustRevision = fault == 6 ? 24 : 23;
        request.payload = bytes.data();
        request.payloadBytes = bytes.size();
        request.requestDigest = digest(hash, bytes.data(), bytes.size());
        if (fault == 5)
            request.requestDigest[0] ^= 1;
        return request;
    }
    void reopen(bool accepted, unsigned expectedControls) {
        store.loseSession();
        bundle_review::Session fresh(medium);
        ProgressJournal journal(fresh);
        require(journal.recover(origin).outcome == ProgressOutcome::Recovered,
                "genuine new session low-level recovery");
        Memory m;
        Ids i;
        rb::Journal b(journal, hash, i, m.w);
        Consumer c;
        const auto r = b.recover(c);
        require((r.outcome == rb::Outcome::Recovered) == accepted, "new session bundle outcome");
        require(c.commits == (accepted ? 1u : 0u), "no publication after rejected recovery");
        if (accepted)
            require(c.controls == expectedControls, "actual recovered control count");
        else {
            require(c.aborts == 1, "invalid recovery aborts consumer");
            require(c.begins == 0, "invalid raw records reject before provisional callbacks");
        }
    }
};
void controlCases() {
    {
        Fixture f;
        std::vector<std::uint8_t> bytes;
        auto r = f.control(0, bytes);
        require(f.bundle.auxiliary(r, f.fence, 61).error == rb::Error::Capacity,
                "full AFTER-record reserve enforced");
        require(f.bundle.auxiliary(r, f.fence, 60).outcome == rb::Outcome::Committed,
                "valid control commits with exact reserve");
        const auto calls = f.fence.calls;
        require(f.bundle.auxiliary(r, f.fence, 60).outcome == rb::Outcome::Duplicate,
                "immutable physical duplicate");
        require(f.fence.calls == calls && f.low.snapshot().lastIssued == 0,
                "duplicate no new eligibility and no ID allocation");
        f.reopen(true, 1);
    }
    for (unsigned fault = 0; fault <= 7; ++fault) {
        Fixture f(fault != 0);
        std::vector<std::uint8_t> bytes;
        const auto r = f.control(fault, bytes);
        const auto count = f.low.snapshot().records;
        require(f.bundle.auxiliary(r, f.fence, 0).outcome == rb::Outcome::Rejected,
                "bad control rejects before write");
        require(f.low.snapshot().records == count, "bad control no physical record");
        // Bypass high-level writer deliberately to audit actual hostile persisted bytes.
        require(f.low.commit(r, f.fence).outcome == ProgressOutcome::Committed,
                "hostile raw record injected through actual low journal");
        f.reopen(false, 0);
    }
    {
        Fixture f;
        std::vector<std::uint8_t> bytes;
        auto r = f.control(0, bytes);
        f.store.readAfterSync = f.store.syncs + 2;
        require(f.bundle.auxiliary(r, f.fence, 0).outcome == rb::Outcome::Unknown &&
                    f.bundle.unresolved(),
                "postwrite scan IO is Unknown");
        require(f.bundle.cancelPrepared().outcome != rb::Outcome::Abandoned,
                "Unknown cannot cancel");
        f.reopen(true, 1);
    }
    {
        Fixture f;
        std::vector<std::uint8_t> bytes;
        auto r = f.control(0, bytes);
        f.fence.accepted = false;
        require(f.bundle.auxiliary(r, f.fence, 0).error == rb::Error::Fence &&
                    f.low.snapshot().records == 2,
                "synthetic denied fence writes nothing");
    }
}
void rawAuxiliaryMismatch(ProgressKind kind) {
    Fixture hostile;
    const std::uint8_t synthetic[] = {0xa0};
    ProgressRequest bad;
    bad.operationId[15] = 40;
    bad.kind = kind;
    bad.expectedRevision = hostile.low.snapshot().revision;
    bad.payload = synthetic;
    bad.payloadBytes = sizeof(synthetic);
    bad.requestDigest = digest(hostile.hash, synthetic, sizeof(synthetic));
    bad.requestDigest[0] ^= 1;
    require(hostile.low.commit(bad, hostile.fence).outcome == ProgressOutcome::Committed,
            "inject actual opaque low-level auxiliary with wrong wrapper RawSHA");
    hostile.reopen(false, 0);
}
void otherAuxiliaryCases() {
    for (auto kind :
         {ProgressKind::Received, ProgressKind::EffectClaim, ProgressKind::EffectResult}) {
        Fixture f;
        const std::uint8_t synthetic[] = {0xa0};
        ProgressRequest request;
        request.operationId[15] = 40;
        request.kind = kind;
        request.expectedRevision = f.low.snapshot().revision;
        request.expectedLastIssued = f.low.snapshot().lastIssued;
        request.payload = synthetic;
        request.payloadBytes = sizeof(synthetic);
        request.requestDigest = digest(f.hash, synthetic, sizeof(synthetic));
        auto bad = request;
        bad.requestDigest[0] ^= 1;
        require(f.bundle.auxiliary(bad, f.fence, 0).error == rb::Error::DigestMismatch,
                "noncontrol auxiliary writer requires actual raw digest");
        bad = request;
        bad.issueCount = 1;
        require(f.bundle.auxiliary(bad, f.fence, 0).error == rb::Error::InvalidArgument,
                "auxiliary cannot allocate new Fact IDs");
        require(f.bundle.auxiliary(request, f.fence, 0).outcome == rb::Outcome::Committed &&
                    f.low.snapshot().lastIssued == 0,
                "actual noncontrol auxiliary transport commits without ID allocation");
        f.reopen(true, 1); // Synthetic consumer accepts opaque bytes; NO effect/ingress semantics.
        rawAuxiliaryMismatch(kind);
    }
    Fixture f;
    ProgressRecord chunk;
    std::uint8_t bytes[3072];
    require(f.low.readRecord(0, chunk, bytes, sizeof(bytes)).outcome == ProgressOutcome::Read,
            "read actual catalog bundle fragment");
    ProgressRequest request;
    request.operationId[15] = 41;
    request.kind = ProgressKind::Control;
    request.expectedRevision = f.low.snapshot().revision;
    request.payload = bytes;
    request.payloadBytes = chunk.payloadBytes;
    request.requestDigest = digest(f.hash, bytes, chunk.payloadBytes);
    require(f.bundle.auxiliary(request, f.fence, 0).error == rb::Error::Canonical,
            "bundle fragments forbidden through auxiliary");
    request.kind = ProgressKind::Application;
    require(f.bundle.auxiliary(request, f.fence, 0).error == rb::Error::InvalidArgument,
            "Application forbidden through auxiliary");
}
void cancelCases() {
    {
        Fixture f;
        auto k = f.catalog;
        k.operation[15] = 30;
        require(f.bundle.begin(k, rb::HeaderMeta{}, 0, 0).outcome == rb::Outcome::Prepared,
                "prepare cancel candidate");
        require(f.bundle.cancelPrepared().outcome == rb::Outcome::Abandoned && !f.bundle.active(),
                "prewrite cancellation");
        require(f.low.snapshot().records == 2 && f.memory.w.bundles.count == 1,
                "cancel removes only private descriptor");
        require(f.bundle.cancelPrepared().outcome == rb::Outcome::Rejected,
                "double cancel rejects");
        require(f.bundle.begin(k, rb::HeaderMeta{}, 0, 0).outcome == rb::Outcome::Prepared,
                "cancelled beforewrite can prepare afresh");
        require(f.bundle.step(f.fence).outcome == rb::Outcome::Progress, "candidate actual chunk");
        require(f.bundle.cancelPrepared().outcome == rb::Outcome::Rejected && f.bundle.active(),
                "written chunk cannot cancel");
        f.reopen(true, 0);
    }
    {
        Fixture f;
        auto k = f.catalog;
        k.operation[15] = 31;
        require(f.bundle.begin(k, rb::HeaderMeta{}, 0, 0).outcome == rb::Outcome::Prepared,
                "unknown candidate prepare");
        f.store.budget = 0;
        require(f.bundle.step(f.fence).outcome == rb::Outcome::Unknown,
                "uncertain zero-write cannot infer noncommit");
        require(f.bundle.cancelPrepared().outcome == rb::Outcome::Rejected,
                "uncertain candidate cannot cancel");
        f.reopen(true, 0);
    }
}
} // namespace
int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--rawsha") {
        const int kind = std::atoi(argv[2]);
        require(kind == 1 || kind == 3 || kind == 4, "recognized noncontrol Auxkind");
        rawAuxiliaryMismatch(static_cast<ProgressKind>(kind));
    } else {
        require(argc == 1, "no unknown audit arguments");
        controlCases();
        otherAuxiliaryCases();
        cancelCases();
    }
    std::cout << checks
              << " focused actual Bundle/Control/Aux assertions; synthetic consumer/fences only\n";
}
