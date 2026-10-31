#include "DeviceRuntime/RulesBundleJournal.h"
#include "Store.h"
#ifdef RULES_BUNDLE_REVIEW_PSA
#define PACKAGE_MODULE_REVIEW_PSA
#endif
#include "../rules-planner-review/CompilerSupport.h"
#include <cstdlib>
#include <functional>
namespace rb = larp::devices::rules_bundle;
using namespace larp::devices;
namespace {
std::size_t checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
std::string hex(const std::uint8_t *p, std::size_t n) {
    const char *digits = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}
std::vector<std::uint8_t> unhex(const std::string &s) {
    std::vector<std::uint8_t> b;
    check(s.size() % 2 == 0, "hex length");
    for (std::size_t i = 0; i < s.size(); i += 2)
        b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return b;
}
rb::Digest digest(Hash &h, const std::vector<std::uint8_t> &bytes) {
    rb::Digest d{};
    std::size_t n = 0;
    check(h.begin() && h.update(bytes.data(), bytes.size()) && h.finish(d.data(), n) && n == 32 &&
              h.abort(),
          "real SHA256");
    return d;
}
struct Buffers {
    rb::Workspace work;
    std::vector<std::uint8_t> stage, recover;
    std::vector<rb::Descriptor> descriptors;
    std::vector<rb::RecordLink> records;
    Buffers() : stage(32769), recover(32768), descriptors(63), records(63) {
        work.staged.data = stage.data();
        work.staged.capacity = stage.size();
        work.recovered.data = recover.data();
        work.recovered.capacity = recover.size();
        work.bundles.data = descriptors.data();
        work.bundles.capacity = descriptors.size();
        work.records.data = records.data();
        work.records.capacity = records.size();
    }
    void set(const std::vector<std::uint8_t> &b) {
        check(b.size() <= stage.size(), "stage size");
        std::copy(b.begin(), b.end(), stage.begin());
        work.staged.count = b.size();
    }
};
struct Ids final : rb::IIdentitySource {
    unsigned next = 0, calls = 0;
    bool fail = false, zero = false, collision = false;
    std::function<void()> callback;
    bool make(rb::Identity &out) override {
        ++calls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        out.fill(zero ? 0 : static_cast<std::uint8_t>(0x80 + (collision ? next : next++)));
        return !fail;
    }
};
struct Fence final : rb::ICommitFence {
    bool allowed = true;
    unsigned calls = 0, chunkCalls = 0, finalCalls = 0;
    std::function<void()> callback;
    bool allow(const rb::Key &, rb::Phase phase, const ProgressRecord &) override {
        ++calls;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        if (phase == rb::Phase::Final)
            ++finalCalls;
        else
            ++chunkCalls;
        return allowed;
    }
};
// A deliberately provisional storage reducer. It is not an authority/Owner seam.
struct Consumer final : rb::IRecoveryConsumer {
    unsigned begins = 0, catalogs = 0, applications = 0, auxiliaries = 0, prepares = 0, commits = 0,
             aborts = 0;
    int reject = 0;
    std::function<void()> beforePrepare;
    std::vector<std::vector<std::uint8_t>> staged, published;
    bool begin(const ProgressSnapshot &) override {
        ++begins;
        staged.clear();
        return reject != 1;
    }
    bool catalog(const rb::Key &, const std::uint8_t *p, std::size_t n) override {
        ++catalogs;
        staged.emplace_back(p, p + n);
        return reject != 2;
    }
    bool application(const rb::Key &, const ProgressRecord &, const std::uint8_t *p,
                     std::size_t n) override {
        ++applications;
        staged.emplace_back(p, p + n);
        return reject != 3;
    }
    bool auxiliary(const ProgressRecord &, const std::uint8_t *, std::size_t) override {
        ++auxiliaries;
        return reject != 4;
    }
    bool prepare() override {
        ++prepares;
        if (beforePrepare)
            beforePrepare();
        return reject != 5;
    }
    void commit() override {
        ++commits;
        published = staged;
    }
    void abort() override {
        ++aborts;
        staged.clear();
    }
};
ProgressOrigin origin() {
    ProgressOrigin o;
    o.event.fill(0x11);
    o.box.fill(0x22);
    o.epoch.fill(0x33);
    return o;
}
rb::Key key(Hash &h, int kind, const std::vector<std::uint8_t> &bytes) {
    rb::Key k;
    k.kind = static_cast<rb::Kind>(kind);
    k.operation.fill(static_cast<std::uint8_t>(0x40 + kind));
    k.request.fill(static_cast<std::uint8_t>(0x10 + kind));
    k.deployment.fill(0x23);
    k.blob = digest(h, bytes);
    k.catalog = kind == 1 ? k.blob : digest(h, {0});
    k.bytes = static_cast<std::uint32_t>(bytes.size());
    k.chunks = static_cast<std::uint8_t>((bytes.size() + 2047) / 2048);
    return k;
}
rb::HeaderMeta meta() {
    rb::HeaderMeta m;
    m.profileRevision = 17;
    m.trustRevision = 23;
    return m;
}
void execute(rb::Journal &bundle, ProgressJournal &low, Fence &f, const rb::Key &k,
             unsigned issues) {
    auto r = bundle.begin(k, meta(), static_cast<std::uint8_t>(issues), 0);
    check(r.outcome == rb::Outcome::Prepared, "bundle Prepared");
    const auto before = low.snapshot();
    for (unsigned i = 0; i <= k.chunks; ++i) {
        r = bundle.step(f);
        check(r.outcome == (i < k.chunks ? rb::Outcome::Progress : rb::Outcome::Committed),
              "chunk Progress / unique Final Committed");
        check(low.snapshot().lastIssued == before.lastIssued + (i == k.chunks ? issues : 0),
              "chunks never allocate IDs");
    }
    check(!bundle.active() && !bundle.unresolved(), "terminal flags");
}
void dump(const std::string &name, ProgressJournal &low, unsigned first) {
    std::uint8_t bytes[3072];
    for (unsigned i = first; i < low.snapshot().records; ++i) {
        ProgressRecord r;
        check(low.readRecord(static_cast<std::uint8_t>(i), r, bytes, sizeof(bytes)).outcome ==
                  ProgressOutcome::Read,
              "actual record read");
        std::cout << "RECORD " << name << ' ' << i << ' ' << hex(r.operationId.data(), 16) << ' '
                  << hex(r.requestDigest.data(), 32) << ' ' << static_cast<unsigned>(r.kind) << ' '
                  << static_cast<unsigned>(r.issueCount) << ' ' << r.profileRevision << ' '
                  << r.trustRevision << ' ' << hex(bytes, r.payloadBytes) << '\n';
    }
}
void valid(const std::string &name, int kind, const std::vector<std::uint8_t> &bytes) {
    bundle_review::Medium medium;
    bundle_review::Session session(medium);
    ProgressJournal low(session);
    check(low.initialize(origin()).outcome == ProgressOutcome::Initialized, "low initialize");
    Hash h;
    Ids ids;
    Buffers b;
    rb::Journal bundle(low, h, ids, b.work);
    Consumer initial;
    check(bundle.recover(initial).outcome == rb::Outcome::Recovered && initial.commits == 1,
          "empty semantic recover");
    Fence f;
    if (kind == 2) {
        b.set({0});
        execute(bundle, low, f, key(h, 1, {0}), 0);
    }
    const auto first = low.snapshot().records;
    const auto k = key(h, kind, bytes);
    b.set(bytes);
    execute(bundle, low, f, k, kind == 2 ? 8 : 0);
    dump(name, low, first);
    const auto programs = session.programs;
    const auto idCalls = ids.calls;
    check(bundle.begin(k, meta(), kind == 2 ? 8 : 0, 0).outcome == rb::Outcome::Duplicate,
          "exact final duplicate");
    check(session.programs == programs && ids.calls == idCalls, "duplicate no IO/random writes");
    if (kind == 2)
        check(bundle.begin(k, meta(), 7, 0).error == rb::Error::Conflict,
              "duplicate changed issueCount");
    auto m = meta();
    ++m.trustRevision;
    check(bundle.begin(k, m, kind == 2 ? 8 : 0, 0).error == rb::Error::Conflict,
          "duplicate changed metadata");
    session.loseSession();
    bundle_review::Session reopened(medium);
    ProgressJournal recovered(reopened);
    check(recovered.recover(origin()).outcome == ProgressOutcome::Recovered,
          "genuine new backend low recovery");
    Buffers r;
    Ids newIds;
    Hash rh;
    rb::Journal newBundle(recovered, rh, newIds, r.work);
    Consumer sink;
    check(newBundle.recover(sink).outcome == rb::Outcome::Recovered && sink.commits == 1 &&
              sink.aborts == 0,
          "all-state publication");
    check(sink.published.size() == (kind == 2 ? 2u : 1u) && sink.published.back() == bytes,
          "exact recovered original bytes");
    for (int reject : {1, 2, 5}) {
        Consumer rejected;
        rejected.reject = reject;
        check(newBundle.recover(rejected).outcome == rb::Outcome::Rejected &&
                  rejected.commits == 0 && rejected.aborts == 1 && rejected.published.empty(),
              "consumer abort no publication");
        Consumer healthy;
        check(newBundle.recover(healthy).outcome == rb::Outcome::Recovered && healthy.commits == 1,
              "consumer failure healthy reuse");
    }
    if (kind == 2) {
        Consumer rejected;
        rejected.reject = 3;
        rejected.published = {{0x4f}};
        check(newBundle.recover(rejected).outcome == rb::Outcome::Rejected &&
                  rejected.commits == 0 && rejected.aborts == 1 &&
                  rejected.published == std::vector<std::vector<std::uint8_t>>{{0x4f}},
              "application callback abort preserves prior publication");
    }
}

const char *error(rb::Error e) {
    switch (e) {
#define E(x)                                                                                       \
    case rb::Error::x:                                                                             \
        return #x
        E(None);
        E(InvalidArgument);
        E(Profile);
        E(Capacity);
        E(Crypto);
        E(Conflict);
        E(Canonical);
        E(Context);
        E(MissingChunk);
        E(MissingCatalog);
        E(DigestMismatch);
        E(Journal);
        E(Fence);
        E(RecoveryRequired);
        E(Busy);
        E(Consumer);
#undef E
    }
    return "unknown";
}
struct LowFence final : IProgressCommitFence {
    bool allow(const ProgressRecord &) override {
        return true;
    }
};
void malformed(const std::string &name) {
    std::string expected;
    unsigned count;
    check(bool(std::cin >> expected >> count), "malformed header");
    bundle_review::Medium medium;
    bundle_review::Session session(medium);
    ProgressJournal low(session);
    check(low.initialize(origin()).outcome == ProgressOutcome::Initialized,
          "malformed low initialize");
    LowFence f;
    for (unsigned i = 0; i < count; ++i) {
        std::string op, dig, payload;
        unsigned kind, issues, profile, trust;
        check(bool(std::cin >> op >> dig >> kind >> issues >> profile >> trust >> payload),
              "malformed record input");
        const auto operation = unhex(op), request = unhex(dig),
                   bytes = payload == "-" ? std::vector<std::uint8_t>{} : unhex(payload);
        ProgressRequest r;
        std::copy(operation.begin(), operation.end(), r.operationId.begin());
        std::copy(request.begin(), request.end(), r.requestDigest.begin());
        r.kind = static_cast<ProgressKind>(kind);
        r.issueCount = static_cast<std::uint8_t>(issues);
        r.profileRevision = profile;
        r.trustRevision = trust;
        r.expectedRevision = low.snapshot().revision;
        r.expectedLastIssued = low.snapshot().lastIssued;
        r.payload = bytes.data();
        r.payloadBytes = bytes.size();
        check(low.commit(r, f).outcome == ProgressOutcome::Committed,
              "malformed semantic fixture must be real lowlevel valid record");
    }
    session.loseSession();
    bundle_review::Session fresh(medium);
    ProgressJournal reopened(fresh);
    check(reopened.recover(origin()).outcome == ProgressOutcome::Recovered,
          "malformed physical bytes reopen valid");
    Hash h;
    Ids ids;
    Buffers b;
    rb::Journal bundle(reopened, h, ids, b.work);
    Consumer sink;
    const auto r = bundle.recover(sink);
    if (r.outcome != rb::Outcome::Rejected || expected != error(r.error)) {
        std::cerr << "BAD " << name << " expected " << expected << " got "
                  << static_cast<unsigned>(r.outcome) << '/' << error(r.error) << '\n';
        std::exit(2);
    }
    check(sink.commits == 0 && sink.aborts == 1 && sink.begins == 0 && sink.published.empty(),
          "all physical validation before semantic callbacks");
    std::cout << "BADPASS " << name << '\n';
}
#include "Faults.h"
} // namespace
int main() {
    std::string name, encoded;
    int kind;
    while (std::cin >> name >> kind) {
        if (kind == 0)
            malformed(name);
        else {
            check(bool(std::cin >> encoded), "valid input");
            valid(name, kind, unhex(encoded));
        }
    }
    faults();
    std::cout << "CHECKS " << checks << '\n';
}
