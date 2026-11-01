#include "CompanionIntegration/RulesSignedEvidence.h"
#include <cstring>
#include <initializer_list>
#include <limits>

namespace larp {
namespace integration {
namespace signed_rules {
namespace {
using progress::EvidenceError;
using progress::EvidenceResult;
using progress::EvidenceScope;
EvidenceResult result(EvidenceScope scope, EvidenceError error = EvidenceError::None) {
    EvidenceResult value;
    value.scope = scope;
    value.error = error;
    return value;
}
EvidenceResult unavailable(EvidenceError error) {
    return result(EvidenceScope::Unavailable, error);
}
EvidenceResult rejected(EvidenceError error) {
    return result(EvidenceScope::Rejected, error);
}
struct Operation {
    bool &flag;
    explicit Operation(bool &value) : flag(value) {
        flag = true;
    }
    ~Operation() {
        flag = false;
    }
};
struct Bounds {
    std::uintptr_t first = 0, last = 0;
};
bool bounds(const void *data, std::size_t count, Bounds &out) {
    if (!count) {
        out = {};
        return true;
    }
    out.first = reinterpret_cast<std::uintptr_t>(data);
    if (!data || count > std::numeric_limits<std::uintptr_t>::max() - out.first)
        return false;
    out.last = out.first + count;
    return true;
}
bool overlaps(Bounds a, Bounds b) {
    return a.last && b.last && a.first < b.last && b.first < a.last;
}
struct Areas {
    Bounds data[48];
    unsigned count = 0;
    bool add(const void *p, std::size_t n) {
        Bounds next;
        if (!bounds(p, n, next) || count == 48)
            return false;
        for (unsigned i = 0; i < count; ++i)
            if (overlaps(next, data[i]))
                return false;
        data[count++] = next;
        return true;
    }
    template <typename T> bool pool(const rules::Pool<T> &p, bool inputCount = true) {
        return (!inputCount || p.count <= p.capacity) &&
               p.capacity <= std::numeric_limits<std::size_t>::max() / sizeof(T) &&
               (!p.capacity || reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0) &&
               add(p.data, p.capacity * sizeof(T));
    }
    bool disjoint(const void *p, std::size_t n) const {
        Bounds view;
        if (!bounds(p, n, view))
            return false;
        for (unsigned i = 0; i < count; ++i)
            if (overlaps(view, data[i]))
                return false;
        return true;
    }
};
template <typename T> bool poolShape(const rules::Pool<T> &p) {
    return p.count <= p.capacity &&
           (!p.capacity ||
            (p.data && reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0)) &&
           p.capacity <= std::numeric_limits<std::size_t>::max() / sizeof(T);
}
template <typename T> bool spanShape(rules::Span<T> p) {
    return (!p.count || (p.data && reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0)) &&
           p.count <= std::numeric_limits<std::size_t>::max() / sizeof(T);
}
template <typename A, typename B> bool bytesEqual(const A &a, const B &b, std::size_t n) {
    return !std::memcmp(a.bytes, b.bytes, n);
}
bool same(const rules::Identity &a, const rules::Identity &b) {
    return bytesEqual(a, b, 16);
}
bool same(const rules::Digest &a, const rules::Digest &b) {
    return bytesEqual(a, b, 32);
}
bool same(const rules::FactId &a, const rules::FactId &b) {
    return same(a.event, b.event) && same(a.origin, b.origin) && same(a.epoch, b.epoch) &&
           a.sequence == b.sequence;
}
bool same(const rules::FactRef &a, const rules::FactRef &b) {
    return same(a.id, b.id) && same(a.digest, b.digest) && same(a.deployment, b.deployment) &&
           same(a.binding, b.binding) && same(a.run, b.run) && same(a.station, b.station) &&
           same(a.subject, b.subject) && a.type == b.type;
}
bool nonzero(const rules::Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
template <typename A, typename B> void copyId(const A &a, B &b) {
    std::memcpy(b.bytes, a.bytes, 16);
}
template <typename A, typename B> void copyDigest(const A &a, B &b) {
    std::memcpy(b.bytes, a.bytes, 32);
}
void copyId(const rules::FactId &a, d0::FactId &b) {
    copyId(a.event, b.event);
    copyId(a.origin, b.origin);
    copyId(a.epoch, b.epoch);
    b.sequence = a.sequence;
}
rules::FactId copyId(const d0::FactId &a) {
    rules::FactId b;
    copyId(a.event, b.event);
    copyId(a.origin, b.origin);
    copyId(a.epoch, b.epoch);
    b.sequence = a.sequence;
    return b;
}
rules::FactRef actualRef(const d0::FrozenFact &checked) {
    rules::FactRef ref;
    const auto &f = checked.fact;
    copyId(f.event, ref.id.event);
    copyId(f.origin, ref.id.origin);
    copyId(f.epoch, ref.id.epoch);
    ref.id.sequence = f.sequence;
    copyDigest(checked.payloadDigest, ref.digest);
    copyDigest(f.package, ref.deployment);
    copyDigest(f.binding, ref.binding);
    copyId(f.run, ref.run);
    copyId(f.station, ref.station);
    copyId(f.subject, ref.subject);
    ref.type = static_cast<rules::FactType>(f.type);
    return ref;
}
class Bytes final : public configuration::IPackageByteSource {
    rules::Span<std::uint8_t> bytes_;
    std::size_t at_ = 0;

  public:
    explicit Bytes(rules::Span<std::uint8_t> bytes) : bytes_(bytes) {}
    configuration::PackageReadStatus readByte(std::uint8_t &out) override {
        if (at_ == bytes_.count)
            return configuration::PackageReadStatus::End;
        out = bytes_.data[at_++];
        return configuration::PackageReadStatus::Byte;
    }
};
struct Writer {
    rules::Pool<std::uint8_t> &out;
    bool ok = true;
    explicit Writer(rules::Pool<std::uint8_t> &p) : out(p) {}
    void raw(const void *p, std::size_t n) {
        if (!ok || n > out.capacity - out.count || (n && !p)) {
            ok = false;
            return;
        }
        if (n)
            std::memcpy(out.data + out.count, p, n);
        out.count += n;
    }
    void byte(std::uint8_t b) {
        raw(&b, 1);
    }
    void head(unsigned major, std::uint64_t n) {
        if (n < 24) {
            byte(static_cast<std::uint8_t>((major << 5) | n));
            return;
        }
        const unsigned length = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        byte(static_cast<std::uint8_t>((major << 5) | (length == 1   ? 24
                                                       : length == 2 ? 25
                                                       : length == 4 ? 26
                                                                     : 27)));
        for (unsigned i = length; i; --i)
            byte(static_cast<std::uint8_t>(n >> (8 * (i - 1))));
    }
    void u(std::uint64_t n) {
        head(0, n);
    }
    void bytes(const void *p, std::size_t n) {
        head(2, n);
        raw(p, n);
    }
    void text(const char *s) {
        const auto n = std::strlen(s);
        head(3, n);
        raw(s, n);
    }
    void clock(const progress::ClockWitness &w) {
        head(4, 5);
        bytes(w.boot.bytes, 16);
        u(w.monotonicBefore);
        u(w.monotonicAfter);
        u(w.activeBefore);
        u(w.activeAfter);
    }
    void id(const rules::FactId &id) {
        head(4, 4);
        bytes(id.event.bytes, 16);
        bytes(id.origin.bytes, 16);
        bytes(id.epoch.bytes, 16);
        u(id.sequence);
    }
};
struct Reader {
    rules::Span<std::uint8_t> source;
    std::size_t at = 0;
    explicit Reader(rules::Span<std::uint8_t> p) : source(p) {}
    bool head(unsigned major, std::uint64_t &n) {
        if (at == source.count)
            return false;
        unsigned b = source.data[at++], ai = b & 31;
        if (b >> 5 != major || ai > 27)
            return false;
        if (ai < 24) {
            n = ai;
            return true;
        }
        const unsigned length = 1u << (ai - 24);
        if (length > source.count - at)
            return false;
        n = 0;
        for (unsigned i = 0; i < length; ++i)
            n = (n << 8) | source.data[at++];
        return !((length == 1 && n < 24) || (length == 2 && n <= 255) ||
                 (length == 4 && n <= 65535) || (length == 8 && n <= UINT32_MAX));
    }
    bool shape(unsigned major, std::uint64_t wanted) {
        std::uint64_t n = 0;
        return head(major, n) && n == wanted;
    }
    bool u(std::uint64_t &n) {
        return head(0, n);
    }
    bool fixed(void *p, std::size_t n) {
        if (!shape(2, n) || n > source.count - at)
            return false;
        std::memcpy(p, source.data + at, n);
        at += n;
        return true;
    }
    bool blob(rules::Span<std::uint8_t> &p) {
        std::uint64_t n = 0;
        if (!head(2, n) || n > source.count - at)
            return false;
        p.data = n ? source.data + at : nullptr;
        p.count = static_cast<std::size_t>(n);
        at += p.count;
        return true;
    }
    bool text(const char *s) {
        const auto n = std::strlen(s);
        if (!shape(3, n) || n > source.count - at || std::memcmp(source.data + at, s, n))
            return false;
        at += n;
        return true;
    }
    bool clock(progress::ClockWitness &w) {
        return shape(4, 5) && fixed(w.boot.bytes, 16) && u(w.monotonicBefore) &&
               u(w.monotonicAfter) && u(w.activeBefore) && u(w.activeAfter);
    }
    bool id(rules::FactId &id) {
        return shape(4, 4) && fixed(id.event.bytes, 16) && fixed(id.origin.bytes, 16) &&
               fixed(id.epoch.bytes, 16) && u(id.sequence);
    }
};
bool clockShape(const rules::State &before, const codec::Application &a,
                const progress::ClockWitness &w) {
    if (before.mode != rules::Mode::Running || !nonzero(w.boot) ||
        w.monotonicAfter < w.monotonicBefore || w.activeBefore != before.fence.activeTime ||
        w.activeAfter != a.plan.expected.activeTime || w.activeAfter < w.activeBefore ||
        !rules::overridePlanningFence(before.fence, a.plan.expected))
        return false;
    return w.activeAfter - w.activeBefore == w.monotonicAfter - w.monotonicBefore;
}
bool programShape(const rules::Program &p, const rules::State &before,
                  const codec::Application &a) {
    return p.ready && rules::sameContext(p.context, before.context) &&
           rules::sameContext(p.context, a.plan.context) &&
           rules::sameContext(p.context, a.plan.receipt.context) &&
           rules::sameContext(p.context, a.plan.receipt.input.context) && nonzero(a.operation) &&
           a.plan.emissions.count <= codec::MaximumOutgoing && poolShape(a.plan.values) &&
           poolShape(a.plan.timers) && poolShape(a.plan.latches) && poolShape(a.plan.emissions) &&
           poolShape(a.plan.intents) && poolShape(a.plan.evidence) && spanShape(before.values) &&
           spanShape(before.timers) && spanShape(before.latches) && spanShape(before.facts);
}
template <typename T> bool protect(const Areas &areas, const rules::Pool<T> &p) {
    return poolShape(p) && areas.disjoint(p.data, p.capacity * sizeof(T));
}
template <typename T> bool protect(const Areas &areas, rules::Span<T> p) {
    return spanShape(p) && areas.disjoint(p.data, p.count * sizeof(T));
}
bool owned(rules::Evidence evidence, const rules::Pool<rules::FactRef> &pool) {
    if (!evidence.refs.count)
        return true;
    const auto base = reinterpret_cast<std::uintptr_t>(pool.data);
    const auto at = reinterpret_cast<std::uintptr_t>(evidence.refs.data);
    if (!pool.data || !evidence.refs.data || at < base || (at - base) % sizeof(rules::FactRef))
        return false;
    const auto index = (at - base) / sizeof(rules::FactRef);
    return index <= pool.count && evidence.refs.count <= pool.count - index;
}
bool protectSources(const Areas &areas, const rules::Program &p, const rules::State &before,
                    const codec::Application &a) {
    if (before.values.count > 32 || before.timers.count > 16 || before.latches.count > 64 ||
        before.facts.count > MaximumProofs || a.plan.values.count > 32 ||
        a.plan.timers.count > 16 || a.plan.latches.count > 64 || a.plan.emissions.count > 8 ||
        a.plan.intents.count > 8 || a.plan.evidence.count > MaximumProofs)
        return false;
    if (!areas.disjoint(&p, sizeof(p)) || !areas.disjoint(&before, sizeof(before)) ||
        !protect(areas, p.strings) || !protect(areas, p.offsets) || !protect(areas, p.nodes) ||
        !protect(areas, p.actions) || !protect(areas, p.rules) || !protect(areas, p.values) ||
        !protect(areas, p.timers) || !protect(areas, p.ports) || !protect(areas, p.subjects) ||
        !protect(areas, p.resources) || !protect(areas, p.epochs) ||
        !protect(areas, p.allowedResources) || !protect(areas, p.triggers) ||
        !protect(areas, a.plan.values) || !protect(areas, a.plan.timers) ||
        !protect(areas, a.plan.latches) || !protect(areas, a.plan.emissions) ||
        !protect(areas, a.plan.intents) || !protect(areas, a.plan.evidence) ||
        !protect(areas, before.values) || !protect(areas, before.timers) ||
        !protect(areas, before.latches) || !protect(areas, before.facts) ||
        !protect(areas, before.ledger) || !protect(areas, before.solved))
        return false;
    for (std::size_t i = 0; i < a.plan.values.count; ++i)
        if (!owned(a.plan.values.data[i].value.evidence, a.plan.evidence))
            return false;
    for (std::size_t i = 0; i < a.plan.timers.count; ++i)
        if (!owned(a.plan.timers.data[i].value.evidence, a.plan.evidence))
            return false;
    for (std::size_t i = 0; i < a.plan.latches.count; ++i)
        if (!owned(a.plan.latches.data[i].evidence, a.plan.evidence))
            return false;
    for (std::size_t i = 0; i < a.plan.emissions.count; ++i)
        if (!owned(a.plan.emissions.data[i].dependencies, a.plan.evidence))
            return false;
    for (std::size_t i = 0; i < a.plan.intents.count; ++i)
        if (!owned(a.plan.intents.data[i].evidence, a.plan.evidence))
            return false;
    for (std::size_t i = 0; i < before.values.count; ++i)
        if (!protect(areas, before.values.data[i].evidence.refs))
            return false;
    for (std::size_t i = 0; i < before.timers.count; ++i)
        if (!protect(areas, before.timers.data[i].evidence.refs))
            return false;
    for (std::size_t i = 0; i < before.latches.count; ++i)
        if (!protect(areas, before.latches.data[i].evidence.refs))
            return false;
    return true;
}
bool workAreas(Areas &areas, Workspace &work) {
    return areas.add(&work, sizeof(work)) && areas.pool(work.annotations, false) &&
           areas.pool(work.catalog, false) && areas.pool(work.origins, false);
}
void clearWork(Workspace &work) {
    if (work.annotations.data && work.annotations.count)
        std::memset(work.annotations.data, 0, work.annotations.count);
    work.annotations.count = work.catalog.count = work.origins.count = 0;
    work.checked = {};
    work.writer = {};
}
struct Engine {
    configuration::IPackageDigest &hash;
    d0::IFactCrypto &crypto;
    IOriginalInstallations &installations;
    IOriginalInputs &inputs;
    Workspace &work;
    EvidenceResult outcome;
    bool extend = true;
    const std::uint64_t lastIssued;
    Engine(configuration::IPackageDigest &h, d0::IFactCrypto &c, IOriginalInstallations &i,
           IOriginalInputs &p, Workspace &w, std::uint64_t issued)
        : hash(h), crypto(c), installations(i), inputs(p), work(w), lastIssued(issued) {}
    bool fail(EvidenceError error) {
        outcome = rejected(error);
        return false;
    }
    bool policy(EvidenceResult r, Use use) {
        const auto expected = use == Use::CurrentPreparation ? EvidenceScope::FrozenOriginal
                                                             : EvidenceScope::VerifiedHistorical;
        if (r.scope != expected || r.error != EvidenceError::None) {
            outcome = r;
            if (outcome.scope != EvidenceScope::Unavailable)
                outcome.scope = EvidenceScope::Rejected;
            return false;
        }
        return true;
    }
    bool digest(const std::uint8_t *p, std::size_t n, rules::Digest &out) {
        out = {};
        bool ok = hash.abort();
        if (ok)
            ok = hash.begin();
        if (ok)
            ok = hash.update(p, n);
        std::size_t written = 0;
        if (ok)
            ok = hash.finish(out.bytes, written) && written == 32;
        const bool closed = hash.abort();
        if (!ok || !closed) {
            out = {};
            return fail(EvidenceError::Crypto);
        }
        return true;
    }
    bool catalog(const rules::Program &p, const rules::Digest &expected) {
        work.catalog.count = 0;
        const auto encoded = codec::encodeCatalog(p, work.catalog);
        if (encoded.scope != codec::Scope::EncodedUnpublished)
            return fail(encoded.error == codec::Error::Capacity ? EvidenceError::Capacity
                                                                : EvidenceError::Context);
        rules::Digest actual;
        return digest(work.catalog.data, work.catalog.count, actual) &&
               (same(actual, expected) || fail(EvidenceError::Digest));
    }
    bool checked(const rules::Program &p, const codec::SignedFact &f,
                 rules::Span<std::uint8_t> install, Use use, bool priorProof = true) {
        if (!spanShape(f.cose) || !f.cose.count || f.cose.count > codec::MaximumFactBytes)
            return fail(EvidenceError::Argument);
        d0::CandidatePublicKey key;
        std::memcpy(key.x, f.publicKey, 32);
        std::memcpy(key.y, f.publicKey + 32, 32);
        Bytes source(f.cose);
        const auto check = d0::checkFact(source, key, hash, crypto, work.checked);
        if (check.scope != d0::Scope::SignatureCheckedUnauthenticated)
            return fail(EvidenceError::Signature);
        if ((work.checked.fact.type != 1 && work.checked.fact.type != 2) ||
            !same(actualRef(work.checked), f.ref))
            return fail(EvidenceError::Context);
        // Newly discovered closure entries have not passed Planner::validFact.
        // An existing cause cannot consume an ID this Final has yet to allocate.
        if (priorProof && same(f.ref.id.event, p.context.event) &&
            same(f.ref.id.origin, p.context.box) && same(f.ref.id.epoch, p.context.sourceEpoch) &&
            f.ref.id.sequence > lastIssued)
            return fail(EvidenceError::Range);
        return policy(installations.fact(p, work.checked, f, install, use), use);
    }
    rules::Id find(const rules::FactId &id) const {
        for (std::size_t i = 0; i < work.origins.count; ++i)
            if (same(work.origins.data[i].ref.id, id))
                return static_cast<rules::Id>(i + 1);
        return 0;
    }
    rules::Id ensure(const rules::FactId &id, const rules::FactRef *expected = nullptr) {
        rules::Id found = find(id);
        if (found) {
            auto &r = work.origins.data[found - 1];
            if (expected) {
                if ((r.expected || r.fetched) && !same(r.ref, *expected)) {
                    fail(EvidenceError::Digest);
                    return 0;
                }
                r.ref = *expected;
                r.expected = true;
            }
            return found;
        }
        if (!extend) {
            fail(EvidenceError::Dependency);
            return 0;
        }
        if (!id.sequence || !nonzero(id.event) || !nonzero(id.origin) || !nonzero(id.epoch)) {
            fail(EvidenceError::Context);
            return 0;
        }
        if (work.origins.count == work.origins.capacity || work.origins.count == MaximumProofs) {
            fail(EvidenceError::Capacity);
            return 0;
        }
        auto &r = work.origins.data[work.origins.count++];
        r = {};
        r.ref.id = id;
        if (expected) {
            r.ref = *expected;
            r.expected = true;
        }
        return static_cast<rules::Id>(work.origins.count);
    }
    bool seed(rules::Evidence evidence) {
        if (!spanShape(evidence.refs) || evidence.refs.count > MaximumProofs)
            return fail(EvidenceError::Argument);
        for (std::size_t i = 0; i < evidence.refs.count; ++i)
            if (!ensure(evidence.refs.data[i].id, &evidence.refs.data[i]))
                return false;
        return true;
    }
    bool seeds(const rules::State &before, const codec::Application &a) {
        rules::Evidence selected;
        selected.refs.data = a.plan.evidence.data;
        selected.refs.count = a.plan.evidence.count;
        if (!seed(selected))
            return false;
        for (std::size_t i = 0; i < before.values.count; ++i)
            if (!seed(before.values.data[i].evidence))
                return false;
        for (std::size_t i = 0; i < before.timers.count; ++i)
            if (!seed(before.timers.data[i].evidence))
                return false;
        for (std::size_t i = 0; i < before.latches.count; ++i)
            if (!seed(before.latches.data[i].evidence))
                return false;
        for (std::size_t i = 0; i < before.facts.count; ++i) {
            const auto &ref = before.facts.data[i].ref;
            if (!ensure(ref.id, &ref))
                return false;
        }
        if (a.plan.receipt.input.kind == rules::InputKind::Fact) {
            const auto &ref = a.plan.receipt.input.fact.ref;
            if (!ensure(ref.id, &ref))
                return false;
        }
        return true;
    }
    bool acyclic() {
        if (!work.origins.count)
            return true;
        for (std::size_t i = 0; i < work.origins.count; ++i)
            work.origins.data[i].color = 0;
        std::size_t done = 0;
        for (std::size_t pass = 0; pass < work.origins.count; ++pass) {
            bool progressMade = false;
            for (std::size_t i = 0; i < work.origins.count; ++i) {
                auto &r = work.origins.data[i];
                if (r.color)
                    continue;
                bool ready = true;
                for (unsigned j = 0; j < r.dependencyCount; ++j) {
                    const auto at = r.dependencies[j];
                    if (!at || at > work.origins.count)
                        return fail(EvidenceError::Dependency);
                    ready &= work.origins.data[at - 1].color != 0;
                }
                if (ready) {
                    r.color = 1;
                    ++done;
                    progressMade = true;
                }
            }
            if (done == work.origins.count)
                return true;
            if (!progressMade)
                break;
        }
        return fail(EvidenceError::Dependency);
    }
    bool append(rules::Pool<std::uint8_t> &pool, rules::Span<std::uint8_t> source,
                rules::Span<std::uint8_t> &copy) {
        if (!spanShape(source) || source.count > pool.capacity - pool.count)
            return fail(EvidenceError::Capacity);
        copy.data = source.count ? pool.data + pool.count : nullptr;
        copy.count = source.count;
        if (source.count)
            std::memcpy(pool.data + pool.count, source.data, source.count);
        pool.count += source.count;
        return true;
    }
    bool original(IOriginalSources &sources, const Areas &areas, const rules::Program &program,
                  codec::Application &a, progress::EvidenceStorage &storage) {
        for (std::size_t i = 0; i < work.origins.count; ++i) {
            auto &origin = work.origins.data[i];
            FactView view;
            const auto read = sources.lookup(origin.ref.id, view);
            if (read != SourceRead::OriginalBytes) {
                outcome = read == SourceRead::Failure
                              ? rejected(EvidenceError::Source)
                              : unavailable(EvidenceError::OriginalInstallUnavailable);
                return false;
            }
            if (!same(origin.ref.id, view.fact.ref.id) ||
                (origin.expected && !same(origin.ref, view.fact.ref)))
                return fail(EvidenceError::Digest);
            if (!spanShape(view.fact.cose) || !spanShape(view.installation) ||
                !view.fact.cose.count || view.fact.cose.count > codec::MaximumFactBytes ||
                !view.installation.count || view.installation.count > codec::MaximumBytes ||
                !areas.disjoint(view.fact.cose.data, view.fact.cose.count) ||
                !areas.disjoint(view.installation.data, view.installation.count))
                return fail(EvidenceError::Argument);
            if (i >= a.proofs.capacity)
                return fail(EvidenceError::Capacity);
            auto &saved = a.proofs.data[i];
            saved = view.fact;
            if (!append(storage.bytes, view.fact.cose, saved.cose) ||
                !append(work.annotations, view.installation, origin.installation))
                return false;
            if (!checked(program, saved, origin.installation, Use::CurrentPreparation))
                return false;
            origin.ref = saved.ref;
            origin.fetched = true;
            origin.dependencyCount = work.checked.fact.dependencyCount;
            for (unsigned j = 0; j < origin.dependencyCount; ++j) {
                const auto id = copyId(work.checked.fact.dependencies[j]);
                origin.dependencies[j] = ensure(id);
                if (!origin.dependencies[j])
                    return false;
            }
            a.proofs.count = i + 1;
        }
        return acyclic();
    }
    bool outgoing(const rules::Program &program, const LocalOriginView &local,
                  rules::Span<std::uint8_t> installation, codec::Application &a,
                  progress::EvidenceStorage &storage) {
        if (a.plan.emissions.count && !local.signer) {
            outcome = unavailable(EvidenceError::OriginKeyUnavailable);
            return false;
        }
        if (a.plan.emissions.count > a.outbox.capacity)
            return fail(EvidenceError::Capacity);
        if (!program.context.bindingGeneration ||
            a.plan.expected.lastIssued > UINT64_MAX - a.plan.emissions.count)
            return fail(EvidenceError::Range);
        for (std::size_t i = 0; i < a.plan.emissions.count; ++i) {
            const auto &emission = a.plan.emissions.data[i];
            if (!emission.subject || emission.subject > program.subjects.count ||
                !spanShape(emission.dependencies.refs) || emission.dependencies.refs.count > 8)
                return fail(EvidenceError::Dependency);
            const auto &subject = program.subjects.data[emission.subject - 1].binding;
            const unsigned type = static_cast<unsigned>(emission.type);
            if ((type != 1 && type != 2) || !(subject.factTypes & (1u << type)) ||
                !same(subject.station, program.context.station) ||
                !same(subject.box, program.context.box) ||
                !same(emission.candidate.event, program.context.event) ||
                !same(emission.candidate.origin, program.context.box) ||
                !same(emission.candidate.epoch, program.context.sourceEpoch) ||
                emission.candidate.sequence != a.plan.expected.lastIssued + i + 1)
                return fail(EvidenceError::Context);
            d0::Fact value;
            copyId(program.context.event, value.event);
            copyId(program.context.run, value.run);
            copyId(program.context.box, value.origin);
            copyId(program.context.station, value.station);
            copyId(program.context.sourceEpoch, value.epoch);
            copyId(subject.subject, value.subject);
            copyDigest(program.context.binding, value.binding);
            copyDigest(program.context.deployment, value.package);
            value.generation = program.context.bindingGeneration;
            value.sequence = emission.candidate.sequence;
            value.type = static_cast<std::uint8_t>(type);
            value.dependencyCount = static_cast<std::uint8_t>(emission.dependencies.refs.count);
            for (unsigned j = 0; j < value.dependencyCount; ++j) {
                const auto &ref = emission.dependencies.refs.data[j];
                const auto at = find(ref.id);
                if (!at || !same(work.origins.data[at - 1].ref, ref))
                    return fail(EvidenceError::Dependency);
                copyId(ref.id, value.dependencies[j]);
            }
            const auto written =
                d0::writeFact(value, hash, *local.signer, crypto, work.writer, work.checked);
            if (written.scope != d0::WriteScope::SignedUnpublished)
                return fail(EvidenceError::Signature);
            if (std::memcmp(written.publicKey.x, local.publicKey.x, 32) ||
                std::memcmp(written.publicKey.y, local.publicKey.y, 32))
                return fail(EvidenceError::OriginKeyUnavailable);
            auto &saved = a.outbox.data[i];
            saved = {};
            saved.ref = actualRef(work.checked);
            std::memcpy(saved.publicKey, written.publicKey.x, 32);
            std::memcpy(saved.publicKey + 32, written.publicKey.y, 32);
            rules::Span<std::uint8_t> bytes;
            bytes.data = work.checked.original;
            bytes.count = work.checked.length;
            if (!append(storage.bytes, bytes, saved.cose) ||
                !policy(installations.fact(program, work.checked, saved, installation,
                                           Use::CurrentPreparation),
                        Use::CurrentPreparation))
                return false;
            a.outbox.count = i + 1;
        }
        return true;
    }
    bool pack(const progress::ClockWitness &clock, rules::Span<std::uint8_t> programInstall,
              rules::Span<std::uint8_t> input, codec::Application &a,
              progress::EvidenceStorage &storage) {
        const auto start = storage.bytes.count;
        Writer out(storage.bytes);
        out.head(5, 5);
        out.u(0);
        out.text("larp.rules.evidence.v1");
        out.u(1);
        out.clock(clock);
        out.u(2);
        out.bytes(programInstall.data, programInstall.count);
        out.u(3);
        out.bytes(input.data, input.count);
        out.u(4);
        out.head(4, work.origins.count);
        for (std::size_t i = 0; i < work.origins.count; ++i) {
            const auto &origin = work.origins.data[i];
            out.head(4, 3);
            out.id(origin.ref.id);
            out.bytes(origin.ref.digest.bytes, 32);
            out.bytes(origin.installation.data, origin.installation.count);
        }
        if (!out.ok)
            return fail(EvidenceError::Capacity);
        a.authorization.data = storage.bytes.data + start;
        a.authorization.count = storage.bytes.count - start;
        return a.authorization.count <= codec::MaximumBytes || fail(EvidenceError::Capacity);
    }
    bool unpack(const codec::Application &a, progress::ClockWitness &clock,
                rules::Span<std::uint8_t> &programInstall, rules::Span<std::uint8_t> &input) {
        if (!spanShape(a.authorization) || !a.authorization.count ||
            a.authorization.count > codec::MaximumBytes)
            return fail(EvidenceError::Canonical);
        Reader in(a.authorization);
        std::uint64_t count = 0;
        if (!in.shape(5, 5) || !in.shape(0, 0) || !in.text("larp.rules.evidence.v1") ||
            !in.shape(0, 1) || !in.clock(clock) || !in.shape(0, 2) || !in.blob(programInstall) ||
            !in.shape(0, 3) || !in.blob(input) || !in.shape(0, 4) || !in.head(4, count))
            return fail(EvidenceError::Canonical);
        if (count != a.proofs.count || count > work.origins.capacity || count > MaximumProofs)
            return fail(EvidenceError::Capacity);
        work.origins.count = static_cast<std::size_t>(count);
        for (std::size_t i = 0; i < work.origins.count; ++i) {
            auto &origin = work.origins.data[i];
            origin = {};
            if (!in.shape(4, 3) || !in.id(origin.ref.id) ||
                !in.fixed(origin.ref.digest.bytes, 32) || !in.blob(origin.installation))
                return fail(EvidenceError::Canonical);
            if (!origin.installation.count)
                return fail(EvidenceError::Canonical);
            if (!same(origin.ref.id, a.proofs.data[i].ref.id) ||
                !same(origin.ref.digest, a.proofs.data[i].ref.digest))
                return fail(EvidenceError::Digest);
            for (std::size_t j = 0; j < i; ++j)
                if (same(origin.ref.id, work.origins.data[j].ref.id))
                    return fail(EvidenceError::Digest);
            origin.ref = a.proofs.data[i].ref;
            origin.fetched = true;
        }
        return (in.at == a.authorization.count && programInstall.count && input.count) ||
               fail(EvidenceError::Canonical);
    }
    bool historicalFacts(const rules::Program &program, const codec::Application &a) {
        for (std::size_t i = 0; i < a.proofs.count; ++i) {
            auto &origin = work.origins.data[i];
            if (!checked(program, a.proofs.data[i], origin.installation, Use::OriginalHistorical))
                return false;
            origin.dependencyCount = work.checked.fact.dependencyCount;
            for (unsigned j = 0; j < origin.dependencyCount; ++j) {
                const auto id = copyId(work.checked.fact.dependencies[j]);
                origin.dependencies[j] = find(id);
                if (!origin.dependencies[j])
                    return fail(EvidenceError::Dependency);
            }
        }
        return acyclic();
    }
    bool historicalOutbox(const rules::Program &program, const codec::Application &a,
                          rules::Span<std::uint8_t> installation) {
        if (a.outbox.count != a.plan.emissions.count || a.outbox.count > 8 ||
            a.plan.expected.lastIssued > UINT64_MAX - a.outbox.count)
            return fail(EvidenceError::Range);
        for (std::size_t i = 0; i < a.outbox.count; ++i) {
            const auto &stored = a.outbox.data[i];
            const auto &emission = a.plan.emissions.data[i];
            if (!checked(program, stored, installation, Use::OriginalHistorical, false))
                return false;
            if (!emission.subject || emission.subject > program.subjects.count ||
                !spanShape(emission.dependencies.refs) || emission.dependencies.refs.count > 8)
                return fail(EvidenceError::Dependency);
            const auto &subject = program.subjects.data[emission.subject - 1].binding;
            const auto &f = work.checked.fact;
            if (!same(stored.ref.id, emission.candidate) ||
                stored.ref.id.sequence != a.plan.expected.lastIssued + i + 1 ||
                f.generation != program.context.bindingGeneration ||
                f.type != static_cast<unsigned>(emission.type) ||
                !bytesEqual(f.subject, subject.subject, 16) ||
                !bytesEqual(f.event, program.context.event, 16) ||
                !bytesEqual(f.run, program.context.run, 16) ||
                !bytesEqual(f.origin, program.context.box, 16) ||
                !bytesEqual(f.station, program.context.station, 16) ||
                !bytesEqual(f.epoch, program.context.sourceEpoch, 16) ||
                !bytesEqual(f.binding, program.context.binding, 32) ||
                !bytesEqual(f.package, program.context.deployment, 32) ||
                f.dependencyCount != emission.dependencies.refs.count)
                return fail(EvidenceError::Context);
            for (unsigned j = 0; j < f.dependencyCount; ++j) {
                const auto &ref = emission.dependencies.refs.data[j];
                const auto at = find(ref.id);
                if (!at || !same(work.origins.data[at - 1].ref, ref) ||
                    !same(copyId(f.dependencies[j]), ref.id))
                    return fail(EvidenceError::Dependency);
            }
        }
        return true;
    }
};
} // namespace

progress::EvidenceResult IOriginalInstallations::program(const rules::Program &,
                                                         rules::Span<std::uint8_t>, Use) {
    return unavailable(EvidenceError::OriginalInstallUnavailable);
}
progress::EvidenceResult IOriginalInstallations::fact(const rules::Program &,
                                                      const d0::FrozenFact &,
                                                      const codec::SignedFact &,
                                                      rules::Span<std::uint8_t>, Use) {
    return unavailable(EvidenceError::OriginalInstallUnavailable);
}
progress::EvidenceResult IOriginalInputs::current(const rules::Program &, const rules::State &,
                                                  const progress::ClockWitness &,
                                                  const codec::Application &,
                                                  rules::Span<std::uint8_t>) {
    return unavailable(EvidenceError::Source);
}
progress::EvidenceResult IOriginalInputs::historical(const rules::Program &, const rules::State &,
                                                     const progress::ClockWitness &,
                                                     const codec::Application &,
                                                     rules::Span<std::uint8_t>) {
    return unavailable(EvidenceError::Source);
}

progress::EvidenceResult SignedPlanEvidence::freeze(const rules::Program &program,
                                                    const rules::State &before,
                                                    const progress::ClockWitness &observation,
                                                    codec::Application &application,
                                                    progress::EvidenceStorage &storage) {
    if (operating_)
        return rejected(EvidenceError::Source);
    Operation operation(operating_);
    const progress::ClockWitness clock = observation;
    Areas areas;
    if (!workAreas(areas, work_) || !areas.add(this, sizeof(*this)) ||
        !areas.add(&application, sizeof(application)) || !areas.add(&storage, sizeof(storage)) ||
        !areas.pool(storage.bytes, false) || !areas.pool(application.proofs, false) ||
        !areas.pool(application.outbox, false) ||
        !protectSources(areas, program, before, application))
        return rejected(EvidenceError::Argument);
    work_.annotations.count = work_.catalog.count = work_.origins.count = 0;
    storage.bytes.count = application.proofs.count = application.outbox.count = 0;
    application.authorization = {};
    auto failed = [&](EvidenceResult r) {
        if (storage.bytes.count)
            std::memset(storage.bytes.data, 0, storage.bytes.count);
        storage.bytes.count = application.proofs.count = application.outbox.count = 0;
        application.authorization = {};
        clearWork(work_);
        return r;
    };
    Engine engine(hash_, crypto_, installations_, inputs_, work_, before.fence.lastIssued);
    if (!programShape(program, before, application) || !application.plan.ready ||
        application.decoded || !clockShape(before, application, clock))
        return failed(rejected(EvidenceError::Clock));
    const auto &admission = application.plan.receipt.input.overrideAdmission;
    const auto expectedScope = application.plan.receipt.input.kind == rules::InputKind::Override
                                   ? rules::OverrideAdmissionScope::Live
                                   : rules::OverrideAdmissionScope::Empty;
    if (admission.scope() != expectedScope ||
        !rules::sameOverrideRecord(admission.record(), application.overrideRecord))
        return failed(rejected(EvidenceError::Authority));
    if (!engine.catalog(program, application.catalog))
        return failed(engine.outcome);
    LocalOriginView local;
    const auto localRead = sources_.local(program, local);
    if (localRead != SourceRead::OriginalBytes)
        return failed(localRead == SourceRead::Failure
                          ? rejected(EvidenceError::Source)
                          : unavailable(EvidenceError::OriginalInstallUnavailable));
    if (!spanShape(local.installation) || !local.installation.count ||
        local.installation.count > codec::MaximumBytes ||
        !areas.disjoint(local.installation.data, local.installation.count))
        return failed(rejected(EvidenceError::Argument));
    rules::Span<std::uint8_t> programInstall;
    if (!engine.append(work_.annotations, local.installation, programInstall) ||
        !engine.policy(installations_.program(program, programInstall, Use::CurrentPreparation),
                       Use::CurrentPreparation))
        return failed(engine.outcome);
    rules::Span<std::uint8_t> offeredInput, input;
    const auto inputRead = sources_.input(program, before, clock, application, offeredInput);
    if (inputRead != SourceRead::OriginalBytes)
        return failed(inputRead == SourceRead::Failure ? rejected(EvidenceError::Source)
                                                       : unavailable(EvidenceError::Source));
    if (!spanShape(offeredInput) || !offeredInput.count ||
        offeredInput.count > codec::MaximumBytes ||
        !areas.disjoint(offeredInput.data, offeredInput.count))
        return failed(rejected(EvidenceError::Argument));
    if (!engine.append(work_.annotations, offeredInput, input) ||
        !engine.policy(inputs_.current(program, before, clock, application, input),
                       Use::CurrentPreparation) ||
        !engine.seeds(before, application) ||
        !engine.original(sources_, areas, program, application, storage) ||
        !engine.outgoing(program, local, programInstall, application, storage) ||
        !engine.pack(clock, programInstall, input, application, storage) ||
        !engine.catalog(program, application.catalog))
        return failed(engine.outcome);
    // Actual serialized size, complete descriptor/evidence representation and
    // immutable plan checks are enforced by the subsequent canonical codec.
    clearWork(work_);
    return result(EvidenceScope::FrozenOriginal);
}

progress::EvidenceResult SignedPlanEvidence::historical(const rules::Program &program,
                                                        const rules::State &before,
                                                        const codec::Application &application,
                                                        const devices::ProgressRecord &final,
                                                        progress::ClockWitness &originalClock) {
    if (operating_)
        return rejected(EvidenceError::Source);
    Operation operation(operating_);
    Areas areas;
    if (!workAreas(areas, work_) || !areas.add(this, sizeof(*this)) ||
        !areas.add(&originalClock, sizeof(originalClock)) ||
        !areas.disjoint(&application, sizeof(application)) ||
        !areas.disjoint(&final, sizeof(final)) ||
        !protectSources(areas, program, before, application) ||
        !protect(areas, application.proofs) || !protect(areas, application.outbox) ||
        !protect(areas, application.authorization))
        return rejected(EvidenceError::Argument);
    for (const auto *pool : {&application.proofs, &application.outbox})
        for (std::size_t i = 0; i < pool->count; ++i)
            if (!protect(areas, pool->data[i].cose))
                return rejected(EvidenceError::Argument);
    work_.annotations.count = work_.catalog.count = work_.origins.count = 0;
    originalClock = {};
    auto failed = [&](EvidenceResult r) {
        clearWork(work_);
        return r;
    };
    Engine engine(hash_, crypto_, installations_, inputs_, work_, before.fence.lastIssued);
    if (!programShape(program, before, application) || !application.decoded ||
        application.plan.ready ||
        application.plan.receipt.input.overrideAdmission.scope() !=
            rules::OverrideAdmissionScope::Empty ||
        !poolShape(application.proofs) || !poolShape(application.outbox) ||
        application.proofs.count > MaximumProofs ||
        final.kind != devices::ProgressKind::Application ||
        final.issueCount != application.plan.emissions.count ||
        std::memcmp(final.operationId.data(), application.operation.bytes, 16) ||
        final.expectedLastIssued != application.plan.expected.lastIssued ||
        final.profileRevision != application.plan.expected.profile ||
        final.trustRevision != application.plan.expected.trust ||
        application.plan.expected.lastIssued > UINT64_MAX - application.plan.emissions.count)
        return failed(rejected(EvidenceError::Range));
    if (application.plan.emissions.count
            ? (final.firstIssued != application.plan.expected.lastIssued + 1 ||
               final.lastIssued !=
                   application.plan.expected.lastIssued + application.plan.emissions.count)
            : (final.firstIssued || final.lastIssued))
        return failed(rejected(EvidenceError::Range));
    if (!engine.catalog(program, application.catalog))
        return failed(engine.outcome);
    progress::ClockWitness clock;
    rules::Span<std::uint8_t> programInstall, input;
    engine.extend = false;
    if (!engine.unpack(application, clock, programInstall, input) ||
        !clockShape(before, application, clock) ||
        !engine.policy(installations_.program(program, programInstall, Use::OriginalHistorical),
                       Use::OriginalHistorical) ||
        !engine.policy(inputs_.historical(program, before, clock, application, input),
                       Use::OriginalHistorical) ||
        !engine.historicalFacts(program, application) || !engine.seeds(before, application) ||
        work_.origins.count != application.proofs.count ||
        !engine.historicalOutbox(program, application, programInstall) ||
        !engine.catalog(program, application.catalog))
        return failed(engine.outcome.error == EvidenceError::None ? rejected(EvidenceError::Clock)
                                                                  : engine.outcome);
    clearWork(work_);
    originalClock = clock;
    return result(EvidenceScope::VerifiedHistorical);
}
} // namespace signed_rules
} // namespace integration
} // namespace larp
