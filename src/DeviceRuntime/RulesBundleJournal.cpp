#include "DeviceRuntime/RulesBundleJournal.h"
#include "DeviceRuntime/RulesProgressControl.h"
#include <cstring>
#include <limits>

namespace larp {
namespace devices {
namespace rules_bundle {
namespace {
constexpr char Domain[] = "larp.rules.bundle.v1";
struct Operation {
    bool &flag;
    explicit Operation(bool &busy) : flag(busy) {
        flag = true;
    }
    ~Operation() {
        flag = false;
    }
};
Result busy() {
    Result r;
    r.outcome = Outcome::Deferred;
    r.error = Error::Busy;
    return r;
}
bool nonzero(const Identity &id) {
    for (auto b : id)
        if (b)
            return true;
    return false;
}
bool same(const Key &a, const Key &b) {
    return a.kind == b.kind && a.operation == b.operation && a.request == b.request &&
           a.deployment == b.deployment && a.catalog == b.catalog && a.blob == b.blob &&
           a.bytes == b.bytes && a.chunks == b.chunks;
}
bool header(const ProgressRecord &a, const ProgressRecord &b) {
    return a.operationId == b.operationId && a.requestDigest == b.requestDigest &&
           a.kind == b.kind && a.revision == b.revision &&
           a.expectedRevision == b.expectedRevision &&
           a.expectedLastIssued == b.expectedLastIssued && a.profileRevision == b.profileRevision &&
           a.trustRevision == b.trustRevision && a.firstIssued == b.firstIssued &&
           a.lastIssued == b.lastIssued && a.payloadBytes == b.payloadBytes &&
           a.issueCount == b.issueCount && a.index == b.index;
}
struct Bounds {
    std::uintptr_t first = 0, last = 0;
};
bool bounds(const void *p, std::size_t n, Bounds &out) {
    if (!n) {
        out = {};
        return true;
    }
    if (!p)
        return false;
    out.first = reinterpret_cast<std::uintptr_t>(p);
    if (n > std::numeric_limits<std::uintptr_t>::max() - out.first)
        return false;
    out.last = out.first + n;
    return true;
}
template <typename T> bool bounds(const Pool<T> &p, Bounds &out) {
    if (p.count > p.capacity || p.capacity > std::numeric_limits<std::size_t>::max() / sizeof(T) ||
        (p.capacity && reinterpret_cast<std::uintptr_t>(p.data) % alignof(T)))
        return false;
    return bounds(p.data, p.capacity * sizeof(T), out);
}
bool overlaps(Bounds a, Bounds b) {
    return a.last && b.last && a.first < b.last && b.first < a.last;
}
template <typename T> Id append(Pool<T> &p) {
    if (!p.data || p.count >= p.capacity || p.count >= ProgressLimits::Records)
        return 0;
    p.data[p.count] = T{};
    return static_cast<Id>(++p.count);
}
struct Writer {
    std::uint8_t *p;
    std::size_t capacity, pos = 0;
    bool ok = true;
    Writer(std::uint8_t *b, std::size_t n) : p(b), capacity(n) {}
    void byte(std::uint8_t b) {
        if (pos == capacity)
            ok = false;
        else
            p[pos++] = b;
    }
    void head(unsigned major, std::uint64_t n) {
        if (n < 24) {
            byte(static_cast<std::uint8_t>((major << 5) | n));
            return;
        }
        unsigned count = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        byte(static_cast<std::uint8_t>((major << 5) | (count == 1   ? 24
                                                       : count == 2 ? 25
                                                       : count == 4 ? 26
                                                                    : 27)));
        for (unsigned i = count; i; --i)
            byte(static_cast<std::uint8_t>(n >> (8 * (i - 1))));
    }
    void u(std::uint64_t n) {
        head(0, n);
    }
    void bytes(const std::uint8_t *b, std::size_t n) {
        head(2, n);
        for (std::size_t i = 0; i < n; ++i)
            byte(b[i]);
    }
    template <std::size_t N> void bytes(const std::array<std::uint8_t, N> &b) {
        bytes(b.data(), N);
    }
    void text(const char *s) {
        head(3, std::strlen(s));
        for (; *s; ++s)
            byte(static_cast<std::uint8_t>(*s));
    }
    void common(const Key &k, Phase phase) {
        head(5, phase == Phase::Chunk ? 12 : 11);
        u(0);
        text(Domain);
        u(1);
        u(phase == Phase::Chunk ? 1 : 2);
        u(2);
        u(static_cast<unsigned>(k.kind));
        u(3);
        bytes(k.operation);
        u(4);
        bytes(k.request);
        u(5);
        bytes(k.deployment);
        u(6);
        bytes(k.catalog);
        u(7);
        bytes(k.blob);
        u(8);
        u(k.bytes);
        u(9);
        u(k.chunks);
    }
};
struct Reference {
    std::uint8_t index = 0;
    Identity operation{};
    Digest digest{};
};
struct Parsed {
    Key key;
    Phase phase = Phase::Chunk;
    std::uint8_t chunk = 0;
    std::uint16_t offset = 0, length = 0;
    Reference refs[MaximumChunks];
};
struct Reader {
    const std::uint8_t *p;
    std::size_t size, pos = 0;
    Reader(const std::uint8_t *b, std::size_t n) : p(b), size(n) {}
    bool head(unsigned major, std::uint64_t &n) {
        if (pos == size)
            return false;
        const unsigned b = p[pos++], ai = b & 31;
        if (b >> 5 != major || ai > 27)
            return false;
        if (ai < 24) {
            n = ai;
            return true;
        }
        unsigned count = 1u << (ai - 24);
        if (count > size - pos)
            return false;
        n = 0;
        for (unsigned i = 0; i < count; ++i)
            n = (n << 8) | p[pos++];
        return !((count == 1 && n < 24) || (count == 2 && n <= 255) || (count == 4 && n <= 65535) ||
                 (count == 8 && n <= UINT32_MAX));
    }
    bool expect(unsigned major, std::uint64_t wanted) {
        std::uint64_t n = 0;
        return head(major, n) && n == wanted;
    }
    bool u(std::uint64_t &n, std::uint64_t maximum) {
        return head(0, n) && n <= maximum;
    }
    template <std::size_t N> bool bytes(std::array<std::uint8_t, N> &out) {
        if (!expect(2, N) || N > size - pos)
            return false;
        std::memcpy(out.data(), p + pos, N);
        pos += N;
        return true;
    }
    bool text(const char *s) {
        const std::size_t n = std::strlen(s);
        if (!expect(3, n) || n > size - pos || std::memcmp(p + pos, s, n))
            return false;
        pos += n;
        return true;
    }
};
bool parse(const std::uint8_t *bytes, std::size_t count, Parsed &out) {
    Reader r(bytes, count);
    std::uint64_t fields = 0, role = 0, n = 0;
    if (!r.head(5, fields) || !r.expect(0, 0) || !r.text(Domain) || !r.expect(0, 1) ||
        !r.u(role, 2) || !role || fields != (role == 1 ? 12 : 11) || !r.expect(0, 2) ||
        !r.u(n, 2) || !n)
        return false;
    out.phase = role == 1 ? Phase::Chunk : Phase::Final;
    out.key.kind = static_cast<Kind>(n);
    auto &k = out.key;
    if (!r.expect(0, 3) || !r.bytes(k.operation) || !nonzero(k.operation) || !r.expect(0, 4) ||
        !r.bytes(k.request) || !r.expect(0, 5) || !r.bytes(k.deployment) || !r.expect(0, 6) ||
        !r.bytes(k.catalog) || !r.expect(0, 7) || !r.bytes(k.blob) || !r.expect(0, 8) ||
        !r.u(n, MaximumBlobBytes) || !n)
        return false;
    k.bytes = static_cast<std::uint32_t>(n);
    if (!r.expect(0, 9) || !r.u(n, MaximumChunks) || !n ||
        n != (k.bytes + ChunkBytes - 1) / ChunkBytes)
        return false;
    k.chunks = static_cast<std::uint8_t>(n);
    if (k.kind == Kind::Catalog && k.catalog != k.blob)
        return false;
    if (!r.expect(0, 10))
        return false;
    if (out.phase == Phase::Chunk) {
        if (!r.u(n, k.chunks - 1))
            return false;
        out.chunk = static_cast<std::uint8_t>(n);
        if (!r.expect(0, 11) || !r.head(2, n))
            return false;
        const std::size_t wanted =
            out.chunk + 1 == k.chunks ? k.bytes - out.chunk * ChunkBytes : ChunkBytes;
        if (n != wanted || n > count - r.pos)
            return false;
        out.offset = static_cast<std::uint16_t>(r.pos);
        out.length = static_cast<std::uint16_t>(n);
        r.pos += static_cast<std::size_t>(n);
    } else {
        if (!r.expect(4, k.chunks))
            return false;
        for (unsigned i = 0; i < k.chunks; ++i) {
            auto &ref = out.refs[i];
            if (!r.expect(4, 3) || !r.u(n, ProgressLimits::Records - 1))
                return false;
            ref.index = static_cast<std::uint8_t>(n);
            if ((i && ref.index <= out.refs[i - 1].index) || !r.bytes(ref.operation) ||
                !nonzero(ref.operation) || !r.bytes(ref.digest))
                return false;
        }
    }
    return r.pos == count;
}
std::size_t encodeChunk(Workspace &w, const Key &k, unsigned index) {
    Writer out(w.recordBytes.data(), w.recordBytes.size());
    out.common(k, Phase::Chunk);
    out.u(10);
    out.u(index);
    out.u(11);
    const std::size_t offset = index * ChunkBytes, remaining = k.bytes - offset;
    out.bytes(w.staged.data + offset, remaining < ChunkBytes ? remaining : ChunkBytes);
    return out.ok ? out.pos : 0;
}
std::size_t encodeFinal(Workspace &w, const Key &k) {
    Writer out(w.recordBytes.data(), w.recordBytes.size());
    out.common(k, Phase::Final);
    out.u(10);
    out.head(4, k.chunks);
    for (unsigned i = 0; i < k.chunks; ++i) {
        const auto &p = w.chunks[i];
        out.head(4, 3);
        out.u(p.index);
        out.bytes(p.operation);
        out.bytes(p.digest);
    }
    return out.ok ? out.pos : 0;
}
struct Fence final : IProgressCommitFence {
    ICommitFence &port;
    const Key &key;
    Phase phase;
    Fence(ICommitFence &f, const Key &k, Phase p) : port(f), key(k), phase(p) {}
    bool allow(const ProgressRecord &r) override {
        return port.allow(key, phase, r);
    }
};
} // namespace

bool Journal::workspace() const {
    Bounds ranges[7];
    if (!bounds(work_.staged, ranges[0]) || !bounds(work_.recovered, ranges[1]) ||
        !bounds(work_.bundles, ranges[2]) || !bounds(work_.records, ranges[3]) ||
        !bounds(&work_, sizeof(work_), ranges[4]) ||
        !bounds(&journal_, sizeof(journal_), ranges[5]) || !bounds(this, sizeof(*this), ranges[6]))
        return false;
    for (unsigned i = 0; i < 7; ++i)
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(ranges[i], ranges[j]))
                return false;
    return true;
}
bool Journal::hash(const std::uint8_t *bytes, std::size_t count, Digest &out) {
    out.fill(0);
    bool ok = hash_.abort();
    if (ok)
        ok = hash_.begin();
    if (ok)
        ok = hash_.update(bytes, count);
    std::size_t written = 0;
    if (ok)
        ok = hash_.finish(out.data(), written) && written == out.size();
    const bool closed = hash_.abort();
    if (!ok || !closed) {
        out.fill(0);
        return false;
    }
    return true;
}
Result Journal::fail(Error error, bool deferred) {
    result_.error = error;
    result_.outcome = deferred ? Outcome::Deferred : Outcome::Rejected;
    return result_;
}
bool Journal::stored(const Descriptor &d, const std::uint8_t *marker, std::size_t bytes) {
    Digest markerDigest;
    if (!hash(marker, bytes, markerDigest)) {
        fail(Error::Crypto);
        return false;
    }
    if (d.final) {
        bool matched = false;
        for (std::size_t i = 0; i < work_.records.count; ++i) {
            const auto &link = work_.records.data[i];
            if (link.phase == Phase::Final && link.record.index == d.finalIndex &&
                link.actualDigest == markerDigest)
                matched = true;
        }
        if (!matched) {
            fail(Error::DigestMismatch);
            return false;
        }
    }
    Parsed final;
    if (!parse(marker, bytes, final) || final.phase != Phase::Final || !same(final.key, d.key)) {
        fail(Error::Canonical);
        return false;
    }
    std::size_t foundCount = 0;
    const RecordLink *last = nullptr;
    for (std::size_t i = 0; i < work_.records.count; ++i) {
        const auto &link = work_.records.data[i];
        if (!link.bundle || !same(work_.bundles.data[link.bundle - 1].key, d.key))
            continue;
        if (link.phase == Phase::Chunk)
            ++foundCount;
        else if (link.phase == Phase::Final)
            last = &link;
    }
    if (foundCount != d.key.chunks) {
        fail(Error::MissingChunk);
        return false;
    }
    const auto expectedLast = last ? last->record.expectedLastIssued : baseLastIssued_;
    const auto expectedProfile = last ? last->record.profileRevision : meta_.profileRevision;
    const auto expectedTrust = last ? last->record.trustRevision : meta_.trustRevision;
    if (d.key.kind == Kind::Application) {
        bool catalog = false;
        for (std::size_t i = 0; i < work_.bundles.count; ++i) {
            const auto &c = work_.bundles.data[i];
            catalog |= c.final && c.key.kind == Kind::Catalog && c.key.blob == d.key.catalog &&
                       c.key.deployment == d.key.deployment && c.finalIndex < final.refs[0].index;
        }
        if (!catalog) {
            fail(Error::MissingCatalog);
            return false;
        }
    }
    if (!work_.recovered.data || work_.recovered.capacity < d.key.bytes) {
        fail(Error::Profile);
        return false;
    }
    work_.recovered.count = 0;
    for (unsigned i = 0; i < d.key.chunks; ++i) {
        const RecordLink *link = nullptr;
        for (std::size_t j = 0; j < work_.records.count; ++j) {
            const auto &candidate = work_.records.data[j];
            if (candidate.bundle && candidate.phase == Phase::Chunk && candidate.chunk == i &&
                same(work_.bundles.data[candidate.bundle - 1].key, d.key))
                link = &candidate;
        }
        const auto &ref = final.refs[i];
        if (!link || link->record.index != ref.index || link->record.operationId != ref.operation ||
            link->actualDigest != ref.digest || (last && ref.index >= last->record.index)) {
            fail(Error::MissingChunk);
            return false;
        }
        ProgressRecord actual;
        result_.journal = journal_.readRecord(ref.index, actual, work_.recordBytes.data(),
                                              work_.recordBytes.size());
        if (result_.journal.outcome != ProgressOutcome::Read) {
            fail(Error::Journal);
            return false;
        }
        Digest actualDigest;
        Parsed chunk;
        if (!hash(work_.recordBytes.data(), actual.payloadBytes, actualDigest)) {
            fail(Error::Crypto);
            return false;
        }
        if (!header(actual, link->record) || actualDigest != ref.digest ||
            actualDigest != actual.requestDigest ||
            !parse(work_.recordBytes.data(), actual.payloadBytes, chunk) ||
            chunk.phase != Phase::Chunk || chunk.chunk != i || !same(chunk.key, d.key) ||
            actual.kind != ProgressKind::Control || actual.issueCount ||
            actual.expectedLastIssued != expectedLast ||
            actual.profileRevision != expectedProfile || actual.trustRevision != expectedTrust) {
            fail(Error::Conflict);
            return false;
        }
        std::memcpy(work_.recovered.data + i * ChunkBytes, work_.recordBytes.data() + chunk.offset,
                    chunk.length);
        work_.recovered.count += chunk.length;
    }
    Digest actual;
    if (!hash(work_.recovered.data, work_.recovered.count, actual)) {
        fail(Error::Crypto);
        return false;
    }
    if (actual != d.key.blob || work_.recovered.count != d.key.bytes) {
        fail(Error::DigestMismatch);
        return false;
    }
    return true;
}

Result Journal::recover(IRecoveryConsumer &consumer) {
    if (operating_ || active_)
        return busy();
    Operation operation(operating_);
    result_ = {};
    indexed_ = false;
    auto rejected = [&](Error error) {
        consumer.abort();
        return fail(error);
    };
    if (!workspace())
        return rejected(Error::InvalidArgument);
    const auto start = journal_.snapshot();
    if (!start.ready || start.poisoned || start.operating)
        return rejected(Error::RecoveryRequired);
    if (start.records > work_.records.capacity)
        return rejected(Error::Profile);
    work_.records.count = work_.bundles.count = 0;
    work_.recovered.count = 0;
    for (unsigned i = 0; i < start.records; ++i) {
        ProgressRecord r;
        result_.journal = journal_.readRecord(static_cast<std::uint8_t>(i), r,
                                              work_.recordBytes.data(), work_.recordBytes.size());
        if (result_.journal.outcome != ProgressOutcome::Read)
            return rejected(Error::Journal);
        const Id at = append(work_.records);
        if (!at)
            return rejected(Error::Profile);
        auto &link = work_.records.data[at - 1];
        link.record = r;
        if (!hash(work_.recordBytes.data(), r.payloadBytes, link.actualDigest))
            return rejected(Error::Crypto);
        if (r.kind != ProgressKind::Control && r.kind != ProgressKind::Application) {
            if (r.requestDigest != link.actualDigest)
                return rejected(Error::DigestMismatch);
            link.phase = Phase::Auxiliary;
            continue;
        }
        if (r.kind == ProgressKind::Control &&
            rules_progress::controlDomain(work_.recordBytes.data(), r.payloadBytes)) {
            rules_progress::Control control;
            if (!rules_progress::decodeControl(work_.recordBytes.data(), r.payloadBytes, control,
                                               work_.recovered)
                     .valid ||
                r.issueCount || std::memcmp(r.operationId.data(), control.operation.bytes, 16) ||
                r.profileRevision != control.expected.profile ||
                r.trustRevision != control.expected.trust ||
                r.expectedLastIssued != control.expected.lastIssued ||
                r.requestDigest != link.actualDigest)
                return rejected(Error::Canonical);
            bool catalog = false;
            for (std::size_t k = 0; k < work_.bundles.count; ++k) {
                const auto &d = work_.bundles.data[k];
                catalog |=
                    d.final && d.key.kind == Kind::Catalog &&
                    !std::memcmp(d.key.blob.data(), control.catalog.bytes, 32) &&
                    !std::memcmp(d.key.deployment.data(), control.context.deployment.bytes, 32) &&
                    d.finalIndex < r.index;
            }
            if (!catalog)
                return rejected(Error::MissingCatalog);
            link.phase = Phase::Auxiliary;
            continue;
        }
        Parsed parsed;
        if (!parse(work_.recordBytes.data(), r.payloadBytes, parsed))
            return rejected(Error::Canonical);
        if (r.requestDigest != link.actualDigest)
            return rejected(Error::DigestMismatch);
        if ((parsed.phase == Phase::Chunk && (r.kind != ProgressKind::Control || r.issueCount ||
                                              r.operationId == parsed.key.operation)) ||
            (parsed.phase == Phase::Final &&
             (r.operationId != parsed.key.operation ||
              (parsed.key.kind == Kind::Catalog ? (r.kind != ProgressKind::Control || r.issueCount)
                                                : r.kind != ProgressKind::Application))))
            return rejected(Error::Conflict);
        Id bundle = 0;
        for (std::size_t j = 0; j < work_.bundles.count; ++j)
            if (work_.bundles.data[j].key.operation == parsed.key.operation)
                bundle = static_cast<Id>(j + 1);
        if (!bundle) {
            bundle = append(work_.bundles);
            if (!bundle)
                return rejected(Error::Profile);
            work_.bundles.data[bundle - 1].key = parsed.key;
        }
        auto &d = work_.bundles.data[bundle - 1];
        if (!same(d.key, parsed.key) || d.final)
            return rejected(Error::Conflict);
        for (std::size_t j = 0; j + 1 < work_.records.count; ++j)
            if (work_.records.data[j].bundle == bundle) {
                const auto &prior = work_.records.data[j];
                if (prior.record.profileRevision != r.profileRevision ||
                    prior.record.trustRevision != r.trustRevision ||
                    prior.record.expectedLastIssued != r.expectedLastIssued)
                    return rejected(Error::Conflict);
                if (parsed.phase == Phase::Chunk && prior.phase == Phase::Chunk &&
                    prior.chunk == parsed.chunk)
                    return rejected(Error::Conflict);
            }
        link.bundle = bundle;
        link.phase = parsed.phase;
        link.chunk = parsed.chunk;
        if (parsed.phase == Phase::Final) {
            d.final = true;
            d.finalIndex = r.index;
        }
    }
    for (std::size_t i = 0; i < work_.records.count; ++i) {
        const auto &link = work_.records.data[i];
        if (link.phase == Phase::Chunk)
            for (std::size_t j = 0; j < work_.bundles.count; ++j)
                if (link.record.operationId == work_.bundles.data[j].key.operation)
                    return rejected(Error::Conflict);
        if (link.phase != Phase::Final)
            continue;
        ProgressRecord actual;
        result_.journal = journal_.readRecord(link.record.index, actual, work_.recordBytes.data(),
                                              work_.recordBytes.size());
        if (result_.journal.outcome != ProgressOutcome::Read || !header(actual, link.record))
            return rejected(Error::Journal);
        if (!stored(work_.bundles.data[link.bundle - 1], work_.recordBytes.data(),
                    actual.payloadBytes)) {
            consumer.abort();
            return result_;
        }
    }
    if (!consumer.begin(start))
        return rejected(Error::Consumer);
    for (std::size_t i = 0; i < work_.records.count; ++i) {
        const auto &link = work_.records.data[i];
        if (link.phase == Phase::Chunk)
            continue;
        ProgressRecord actual;
        result_.journal = journal_.readRecord(link.record.index, actual, work_.recordBytes.data(),
                                              work_.recordBytes.size());
        if (result_.journal.outcome != ProgressOutcome::Read || !header(actual, link.record))
            return rejected(Error::Journal);
        if (link.phase == Phase::Auxiliary) {
            Digest actualDigest;
            if (!hash(work_.recordBytes.data(), actual.payloadBytes, actualDigest))
                return rejected(Error::Crypto);
            if (actualDigest != link.actualDigest)
                return rejected(Error::DigestMismatch);
            if (!consumer.auxiliary(actual, work_.recordBytes.data(), actual.payloadBytes))
                return rejected(Error::Consumer);
        } else {
            const auto &d = work_.bundles.data[link.bundle - 1];
            if (!stored(d, work_.recordBytes.data(), actual.payloadBytes)) {
                consumer.abort();
                return result_;
            }
            const bool accepted =
                d.key.kind == Kind::Catalog
                    ? consumer.catalog(d.key, work_.recovered.data, work_.recovered.count)
                    : consumer.application(d.key, actual, work_.recovered.data,
                                           work_.recovered.count);
            if (!accepted)
                return rejected(Error::Consumer);
        }
    }
    const auto end = journal_.snapshot();
    if (!end.ready || end.poisoned || end.operating || end.revision != start.revision ||
        end.lastIssued != start.lastIssued || end.records != start.records)
        return rejected(Error::Conflict);
    if (!consumer.prepare())
        return rejected(Error::Consumer);
    const auto prepared = journal_.snapshot();
    if (!prepared.ready || prepared.poisoned || prepared.operating ||
        prepared.revision != start.revision || prepared.lastIssued != start.lastIssued ||
        prepared.records != start.records)
        return rejected(Error::Conflict);
    consumer.commit();
    indexed_ = true;
    indexedRevision_ = start.revision;
    result_.error = Error::None;
    result_.outcome = Outcome::Recovered;
    return result_;
}

Result Journal::begin(const Key &requestedKey, const HeaderMeta &requestedMeta, std::uint8_t issues,
                      std::size_t reserved) {
    const Key key = requestedKey;
    const HeaderMeta meta = requestedMeta;
    if (operating_ || active_)
        return busy();
    Operation operation(operating_);
    result_ = {};
    if (!workspace())
        return fail(Error::InvalidArgument);
    const auto state = journal_.snapshot();
    if (unresolved_ || !indexed_ || !state.ready || state.poisoned || state.operating ||
        state.records != work_.records.count || state.revision != indexedRevision_)
        return fail(Error::RecoveryRequired);
    if (!nonzero(key.operation) || (key.kind != Kind::Catalog && key.kind != Kind::Application) ||
        !key.bytes || key.bytes > MaximumBlobBytes || key.bytes != work_.staged.count ||
        key.chunks != (key.bytes + ChunkBytes - 1) / ChunkBytes || !key.chunks ||
        key.chunks > MaximumChunks || issues > 8 ||
        (key.kind == Kind::Catalog && (issues || key.catalog != key.blob)))
        return fail(Error::InvalidArgument);
    if (!work_.staged.data || !work_.recovered.data || work_.recovered.capacity < key.bytes)
        return fail(Error::Profile, true);
    Digest actual;
    if (!hash(work_.staged.data, work_.staged.count, actual))
        return fail(Error::Crypto);
    if (actual != key.blob)
        return fail(Error::DigestMismatch);
    for (std::size_t i = 0; i < work_.bundles.count; ++i) {
        const auto &d = work_.bundles.data[i];
        if (d.key.operation != key.operation)
            continue;
        if (!same(d.key, key))
            return fail(Error::Conflict);
        for (std::size_t j = 0; j < work_.records.count; ++j) {
            const auto &r = work_.records.data[j];
            if (r.bundle != i + 1)
                continue;
            if (r.record.profileRevision != meta.profileRevision ||
                r.record.trustRevision != meta.trustRevision)
                return fail(Error::Conflict);
            if (r.phase == Phase::Final) {
                if (r.record.issueCount != issues)
                    return fail(Error::Conflict);
                result_.journal.outcome = ProgressOutcome::Duplicate;
                result_.journal.index = r.record.index;
                result_.journal.revision = r.record.revision;
                result_.journal.firstIssued = r.record.firstIssued;
                result_.journal.lastIssued = r.record.lastIssued;
                result_.outcome = Outcome::Duplicate;
                return result_;
            }
        }
        return fail(
            Error::RecoveryRequired); // Inert recovered orphan: never revive an old command.
    }
    for (std::size_t i = 0; i < work_.records.count; ++i)
        if (work_.records.data[i].record.operationId == key.operation)
            return fail(Error::Conflict);
    if (key.kind == Kind::Application) {
        bool catalog = false;
        for (std::size_t i = 0; i < work_.bundles.count; ++i) {
            const auto &d = work_.bundles.data[i];
            catalog |= d.final && d.key.kind == Kind::Catalog && d.key.blob == key.catalog &&
                       d.key.deployment == key.deployment;
        }
        if (!catalog)
            return fail(Error::MissingCatalog);
    }
    const std::size_t needed = static_cast<std::size_t>(key.chunks) + 1;
    if (reserved > ProgressLimits::Records || needed > ProgressLimits::Records - state.records ||
        reserved > ProgressLimits::Records - state.records - needed)
        return fail(Error::Capacity, true);
    if (work_.bundles.count == work_.bundles.capacity ||
        needed > work_.records.capacity - work_.records.count)
        return fail(Error::Profile, true);
    if (state.revision > std::numeric_limits<std::uint64_t>::max() - needed ||
        state.lastIssued > std::numeric_limits<std::uint64_t>::max() - issues)
        return fail(Error::Profile, true);
    key_ = key;
    meta_ = meta;
    baseRecords_ = state.records;
    baseRevision_ = state.revision;
    baseLastIssued_ = state.lastIssued;
    reservedIntentRecords_ = reserved;
    issueCount_ = issues;
    next_ = 0;
    for (unsigned i = 0; i < key.chunks; ++i) {
        auto &p = work_.chunks[i];
        p = {};
        if (!ids_.make(p.operation) || !nonzero(p.operation) || p.operation == key.operation)
            return fail(Error::Conflict);
        for (unsigned j = 0; j < i; ++j)
            if (work_.chunks[j].operation == p.operation)
                return fail(Error::Conflict);
        for (std::size_t j = 0; j < work_.records.count; ++j)
            if (work_.records.data[j].record.operationId == p.operation)
                return fail(Error::Conflict);
        for (std::size_t j = 0; j < work_.bundles.count; ++j)
            if (work_.bundles.data[j].key.operation == p.operation)
                return fail(Error::Conflict);
        p.expectedRevision = baseRevision_ + i;
        p.expectedLastIssued = baseLastIssued_;
        p.profileRevision = meta.profileRevision;
        p.trustRevision = meta.trustRevision;
        p.index = static_cast<std::uint8_t>(baseRecords_ + i);
        const auto bytes = encodeChunk(work_, key_, i);
        if (!bytes || !hash(work_.recordBytes.data(), bytes, p.digest))
            return fail(Error::Crypto);
        p.frozen = true;
    }
    work_.final = {};
    auto &final = work_.final;
    final.operation = key.operation;
    final.index = static_cast<std::uint8_t>(baseRecords_ + key.chunks);
    final.expectedRevision = baseRevision_ + key.chunks;
    final.expectedLastIssued = baseLastIssued_;
    final.profileRevision = meta.profileRevision;
    final.trustRevision = meta.trustRevision;
    const auto bytes = encodeFinal(work_, key_);
    if (!bytes || !hash(work_.recordBytes.data(), bytes, final.digest))
        return fail(Error::Crypto);
    final.frozen = true;
    bundle_ = append(work_.bundles);
    if (!bundle_)
        return fail(Error::Profile, true);
    work_.bundles.data[bundle_ - 1].key = key_;
    active_ = true;
    result_.outcome = Outcome::Prepared;
    return result_;
}

Result Journal::step(ICommitFence &port) {
    if (operating_)
        return busy();
    Operation operation(operating_);
    result_ = {};
    if (!active_ || unresolved_)
        return fail(Error::RecoveryRequired);
    if (!workspace()) {
        active_ = false;
        return fail(Error::InvalidArgument);
    }
    const auto state = journal_.snapshot();
    if (!state.ready || state.poisoned || state.operating ||
        state.records != baseRecords_ + next_ || state.revision != baseRevision_ + next_ ||
        state.lastIssued != baseLastIssued_) {
        active_ = false;
        return fail(Error::Conflict);
    }
    if (!bundle_ || bundle_ > work_.bundles.count ||
        !same(work_.bundles.data[bundle_ - 1].key, key_) || work_.bundles.data[bundle_ - 1].final ||
        next_ > key_.chunks || work_.records.count != state.records) {
        active_ = false;
        return fail(Error::Conflict);
    }
    const std::size_t remaining = key_.chunks - next_ + 1;
    if (remaining > work_.records.capacity - work_.records.count)
        return fail(Error::Profile, true);
    if (remaining > ProgressLimits::Records - state.records ||
        reservedIntentRecords_ > ProgressLimits::Records - state.records - remaining)
        return fail(Error::Capacity, true);
    Digest actual;
    if (work_.staged.count != key_.bytes || !hash(work_.staged.data, work_.staged.count, actual) ||
        actual != key_.blob) {
        active_ = false;
        return fail(Error::DigestMismatch);
    }
    const bool final = next_ == key_.chunks;
    auto &p = final ? work_.final : work_.chunks[next_];
    std::size_t bytes = final ? encodeFinal(work_, key_) : encodeChunk(work_, key_, next_);
    if (final) {
        if (!stored(work_.bundles.data[bundle_ - 1], work_.recordBytes.data(), bytes)) {
            active_ = false;
            return result_;
        }
        bytes = encodeFinal(work_, key_); // stored() used the record scratch for actual reads.
    }
    if (!bytes || !p.frozen || !hash(work_.recordBytes.data(), bytes, actual) ||
        actual != p.digest) {
        active_ = false;
        return fail(Error::DigestMismatch);
    }
    ProgressRequest request;
    request.operationId = p.operation;
    request.requestDigest = p.digest;
    request.kind =
        final && key_.kind == Kind::Application ? ProgressKind::Application : ProgressKind::Control;
    request.expectedRevision = p.expectedRevision;
    request.expectedLastIssued = p.expectedLastIssued;
    request.profileRevision = p.profileRevision;
    request.trustRevision = p.trustRevision;
    request.issueCount = final ? issueCount_ : 0;
    request.payload = work_.recordBytes.data();
    request.payloadBytes = bytes;
    Fence fence(port, key_, final ? Phase::Final : Phase::Chunk);
    result_.journal = journal_.commit(request, fence);
    const auto outcome = result_.journal.outcome;
    const auto afterCommit = journal_.snapshot();
    if (outcome == ProgressOutcome::WriteUncertain || afterCommit.poisoned || !afterCommit.ready ||
        outcome == ProgressOutcome::IoFailure || outcome == ProgressOutcome::Corrupt ||
        outcome == ProgressOutcome::Unsupported) {
        unresolved_ = true;
        active_ = false;
        indexed_ = false;
        result_.outcome = Outcome::Unknown;
        result_.error = Error::Journal;
        return result_;
    }
    if (outcome != ProgressOutcome::Committed && outcome != ProgressOutcome::Duplicate) {
        if (outcome == ProgressOutcome::Full)
            return fail(Error::Capacity, true);
        active_ = false;
        indexed_ = false;
        return fail(outcome == ProgressOutcome::Denied ? Error::Fence : Error::Journal);
    }
    ProgressRecord storedRecord;
    const auto read = journal_.readRecord(p.index, storedRecord, work_.recordBytes.data(),
                                          work_.recordBytes.size());
    if (read.outcome != ProgressOutcome::Read) {
        unresolved_ = true;
        active_ = false;
        indexed_ = false;
        result_.outcome = Outcome::Unknown;
        result_.error = Error::Journal;
        return result_;
    }
    const Id at = append(work_.records);
    if (!at) {
        unresolved_ = true;
        active_ = false;
        indexed_ = false;
        result_.outcome = Outcome::Unknown;
        result_.error = Error::Profile;
        return result_;
    }
    auto &link = work_.records.data[at - 1];
    link.bundle = bundle_;
    link.phase = final ? Phase::Final : Phase::Chunk;
    link.chunk = next_;
    link.record = storedRecord;
    link.actualDigest = p.digest;
    p.committed = true;
    indexedRevision_ = storedRecord.revision;
    if (final) {
        auto &d = work_.bundles.data[bundle_ - 1];
        d.final = true;
        d.finalIndex = p.index;
        active_ = false;
        result_.outcome =
            outcome == ProgressOutcome::Duplicate ? Outcome::Duplicate : Outcome::Committed;
    } else {
        ++next_;
        result_.outcome = Outcome::Progress;
    }
    return result_;
}
Result Journal::cancelPrepared() {
    if (operating_)
        return busy();
    Operation operation(operating_);
    result_ = {};
    if (!active_ || unresolved_ || next_ || !workspace() || bundle_ != work_.bundles.count ||
        !bundle_)
        return fail(Error::RecoveryRequired);
    const auto current = journal_.snapshot();
    if (!current.ready || current.poisoned || current.operating ||
        current.records != baseRecords_ || current.revision != baseRevision_ ||
        current.lastIssued != baseLastIssued_ || work_.records.count != baseRecords_ ||
        work_.bundles.data[bundle_ - 1].final || !same(work_.bundles.data[bundle_ - 1].key, key_))
        return fail(Error::Conflict);
    --work_.bundles.count;
    active_ = false;
    bundle_ = 0;
    result_.outcome = Outcome::Abandoned;
    return result_;
}
Result Journal::auxiliary(const ProgressRequest &provided, IProgressCommitFence &fence,
                          std::size_t reserved) {
    const ProgressRequest requestInput = provided;
    if (operating_ || active_)
        return busy();
    Operation operation(operating_);
    result_ = {};
    if (!workspace())
        return fail(Error::InvalidArgument);
    const auto state = journal_.snapshot();
    if (unresolved_ || !indexed_ || !state.ready || state.poisoned || state.operating ||
        state.records != work_.records.count || state.revision != indexedRevision_)
        return fail(Error::RecoveryRequired);
    if (!nonzero(requestInput.operationId) || requestInput.issueCount || !requestInput.payload ||
        !requestInput.payloadBytes || requestInput.payloadBytes > work_.recordBytes.size() ||
        (requestInput.kind != ProgressKind::Received &&
         requestInput.kind != ProgressKind::EffectClaim &&
         requestInput.kind != ProgressKind::EffectResult &&
         requestInput.kind != ProgressKind::Control))
        return fail(Error::InvalidArgument);
    Bounds input, output;
    if (!bounds(requestInput.payload, requestInput.payloadBytes, input) ||
        !bounds(&work_, sizeof(work_), output) || overlaps(input, output))
        return fail(Error::InvalidArgument);
    std::memcpy(work_.recordBytes.data(), requestInput.payload, requestInput.payloadBytes);
    ProgressRequest request = requestInput;
    request.payload = work_.recordBytes.data();
    if (request.kind == ProgressKind::Control) {
        rules_progress::Control control;
        if (!rules_progress::controlDomain(request.payload, request.payloadBytes) ||
            !rules_progress::decodeControl(request.payload, request.payloadBytes, control,
                                           work_.recovered)
                 .valid ||
            std::memcmp(request.operationId.data(), control.operation.bytes, 16) ||
            request.profileRevision != control.expected.profile ||
            request.trustRevision != control.expected.trust ||
            request.expectedLastIssued != control.expected.lastIssued)
            return fail(Error::Canonical);
        bool catalog = false;
        for (std::size_t i = 0; i < work_.bundles.count; ++i) {
            const auto &d = work_.bundles.data[i];
            catalog |= d.final && d.key.kind == Kind::Catalog &&
                       !std::memcmp(d.key.blob.data(), control.catalog.bytes, 32) &&
                       !std::memcmp(d.key.deployment.data(), control.context.deployment.bytes, 32);
        }
        if (!catalog)
            return fail(Error::MissingCatalog);
    }
    Digest actual;
    if (!hash(request.payload, request.payloadBytes, actual))
        return fail(Error::Crypto);
    if (actual != request.requestDigest)
        return fail(Error::DigestMismatch);
    bool duplicate = false;
    for (std::size_t i = 0; i < work_.records.count; ++i)
        if (work_.records.data[i].record.operationId == request.operationId) {
            if (work_.records.data[i].phase != Phase::Auxiliary)
                return fail(Error::Conflict);
            duplicate = true;
        }
    if (!duplicate) {
        for (std::size_t i = 0; i < work_.bundles.count; ++i)
            if (work_.bundles.data[i].key.operation == request.operationId)
                return fail(Error::Conflict);
        if (reserved >= ProgressLimits::Records - state.records)
            return fail(Error::Capacity, true);
        if (work_.records.count == work_.records.capacity)
            return fail(Error::Profile, true);
        if (request.expectedRevision != state.revision ||
            request.expectedLastIssued != state.lastIssued)
            return fail(Error::Conflict);
    }
    result_.journal = journal_.commit(request, fence);
    const auto outcome = result_.journal.outcome;
    const auto after = journal_.snapshot();
    if (outcome == ProgressOutcome::WriteUncertain || after.poisoned || !after.ready ||
        outcome == ProgressOutcome::IoFailure || outcome == ProgressOutcome::Corrupt ||
        outcome == ProgressOutcome::Unsupported) {
        unresolved_ = true;
        indexed_ = active_ = false;
        result_.outcome = Outcome::Unknown;
        result_.error = Error::Journal;
        return result_;
    }
    if (outcome != ProgressOutcome::Committed && outcome != ProgressOutcome::Duplicate) {
        if (outcome == ProgressOutcome::Full)
            return fail(Error::Capacity, true);
        return fail(outcome == ProgressOutcome::Denied     ? Error::Fence
                    : outcome == ProgressOutcome::Conflict ? Error::Conflict
                                                           : Error::Journal);
    }
    ProgressRecord record;
    const auto read = journal_.readRecord(result_.journal.index, record, work_.recordBytes.data(),
                                          work_.recordBytes.size());
    Digest storedDigest;
    if (read.outcome != ProgressOutcome::Read || record.operationId != request.operationId ||
        record.requestDigest != request.requestDigest || record.kind != request.kind ||
        record.expectedRevision != request.expectedRevision ||
        record.expectedLastIssued != request.expectedLastIssued ||
        record.profileRevision != request.profileRevision ||
        record.trustRevision != request.trustRevision || record.issueCount || record.firstIssued ||
        record.lastIssued || record.payloadBytes != request.payloadBytes ||
        !hash(work_.recordBytes.data(), record.payloadBytes, storedDigest) ||
        storedDigest != actual) {
        unresolved_ = true;
        indexed_ = active_ = false;
        result_.outcome = Outcome::Unknown;
        result_.error = Error::Journal;
        return result_;
    }
    if (outcome == ProgressOutcome::Committed) {
        const Id at = append(work_.records);
        if (!at) {
            unresolved_ = true;
            indexed_ = false;
            result_.outcome = Outcome::Unknown;
            result_.error = Error::Profile;
            return result_;
        }
        auto &link = work_.records.data[at - 1];
        link.phase = Phase::Auxiliary;
        link.record = record;
        link.actualDigest = actual;
        indexedRevision_ = record.revision;
    }
    result_.outcome =
        outcome == ProgressOutcome::Committed ? Outcome::Committed : Outcome::Duplicate;
    return result_;
}
} // namespace rules_bundle
} // namespace devices
} // namespace larp
