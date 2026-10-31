#include "GameExecution/RulesProgressCodec.h"
#include <cstring>
#include <initializer_list>
#include <limits>

namespace larp {
namespace execution {
namespace rules_progress {
namespace {
using namespace rules;
using configuration::module_ir::Location;
struct Range {
    std::uintptr_t first = 0, last = 0;
};
struct Ranges {
    Range values[32];
    unsigned count = 0;
    bool add(const void *p, std::size_t n) {
        if (!n)
            return true;
        const auto first = reinterpret_cast<std::uintptr_t>(p);
        if (!p || n > std::numeric_limits<std::uintptr_t>::max() - first || count == 32)
            return false;
        Range next;
        next.first = first;
        next.last = first + n;
        for (unsigned i = 0; i < count; ++i)
            if (next.first < values[i].last && values[i].first < next.last)
                return false;
        values[count++] = next;
        return true;
    }
    template <typename T> bool pool(const Pool<T> &p, bool inputCount = true) {
        return (!inputCount || p.count <= p.capacity) &&
               p.capacity <= std::numeric_limits<std::size_t>::max() / sizeof(T) &&
               (!p.capacity || reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0) &&
               add(p.data, p.capacity * sizeof(T));
    }
};
bool catalogRanges(Ranges &r, const Program &p) {
    return r.add(&p, sizeof(p)) && r.pool(p.strings) && r.pool(p.offsets) && r.pool(p.nodes) &&
           r.pool(p.actions) && r.pool(p.rules) && r.pool(p.values) && r.pool(p.timers) &&
           r.pool(p.ports) && r.pool(p.subjects) && r.pool(p.resources) && r.pool(p.epochs) &&
           r.pool(p.allowedResources) && r.pool(p.triggers);
}
void clear(Program &p) {
    p.ready = false;
    p.strings.count = p.offsets.count = p.nodes.count = p.actions.count = p.rules.count = 0;
    p.values.count = p.timers.count = p.ports.count = p.subjects.count = p.resources.count = 0;
    p.epochs.count = p.allowedResources.count = p.triggers.count = 0;
}
struct Writer {
    static constexpr bool Reading = false;
    std::uint8_t *bytes;
    std::size_t capacity, pos = 0;
    Error error = Error::None;
    Writer(std::uint8_t *p, std::size_t n) : bytes(p), capacity(n) {}
    bool fail(Error e) {
        if (error == Error::None)
            error = e;
        return false;
    }
    bool raw(const void *p, std::size_t n) {
        if (n > capacity - pos || (n && !p))
            return fail(Error::Capacity);
        if (n)
            std::memcpy(bytes + pos, p, n);
        pos += n;
        return true;
    }
    bool head(unsigned major, std::uint64_t n) {
        std::uint8_t data[9];
        if (n < 24) {
            data[0] = static_cast<std::uint8_t>((major << 5) | n);
            return raw(data, 1);
        }
        const unsigned length = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        data[0] = static_cast<std::uint8_t>((major << 5) | (length == 1   ? 24
                                                            : length == 2 ? 25
                                                            : length == 4 ? 26
                                                                          : 27));
        for (unsigned i = 0; i < length; ++i)
            data[i + 1] = static_cast<std::uint8_t>(n >> (8 * (length - i - 1)));
        return raw(data, length + 1);
    }
    bool shape(unsigned major, std::uint64_t n) {
        return head(major, n);
    }
    template <typename T> bool u(T &n) {
        return head(0, n);
    }
    bool integer(std::int32_t &n) {
        return n < 0 ? head(1, static_cast<std::uint64_t>(-1 - static_cast<std::int64_t>(n)))
                     : head(0, static_cast<std::uint32_t>(n));
    }
    bool boolean(bool &v) {
        const std::uint8_t b = v ? 0xf5 : 0xf4;
        return raw(&b, 1);
    }
    bool fixed(void *p, std::size_t n) {
        return head(2, n) && raw(p, n);
    }
    bool text(const char *s) {
        return head(3, std::strlen(s)) && raw(s, std::strlen(s));
    }
    bool key(unsigned n) {
        return head(0, n);
    }
};
struct Reader {
    static constexpr bool Reading = true;
    const std::uint8_t *bytes;
    std::size_t size, pos = 0;
    Error error = Error::None;
    Reader(const std::uint8_t *p, std::size_t n) : bytes(p), size(n) {}
    bool fail(Error e) {
        if (error == Error::None)
            error = e;
        return false;
    }
    bool head(unsigned major, std::uint64_t &n) {
        if (pos == size)
            return fail(Error::Canonical);
        const unsigned b = bytes[pos++], ai = b & 31;
        if (b >> 5 != major || ai > 27)
            return fail(Error::Canonical);
        if (ai < 24) {
            n = ai;
            return true;
        }
        const unsigned length = 1u << (ai - 24);
        if (length > size - pos)
            return fail(Error::Canonical);
        n = 0;
        for (unsigned i = 0; i < length; ++i)
            n = (n << 8) | bytes[pos++];
        if ((length == 1 && n < 24) || (length == 2 && n <= 255) || (length == 4 && n <= 65535) ||
            (length == 8 && n <= UINT32_MAX))
            return fail(Error::Canonical);
        return true;
    }
    bool shape(unsigned major, std::uint64_t wanted) {
        std::uint64_t n = 0;
        return head(major, n) && (n == wanted || fail(Error::Canonical));
    }
    template <typename T> bool u(T &out) {
        std::uint64_t n = 0;
        if (!head(0, n))
            return false;
        if (n > std::numeric_limits<T>::max())
            return fail(Error::Range);
        out = static_cast<T>(n);
        return true;
    }
    bool integer(std::int32_t &out) {
        if (pos == size)
            return fail(Error::Canonical);
        const unsigned major = bytes[pos] >> 5;
        std::uint64_t n = 0;
        if ((major != 0 && major != 1) || !head(major, n) || n > INT32_MAX)
            return fail(Error::Range);
        out = major ? static_cast<std::int32_t>(-1 - static_cast<std::int64_t>(n))
                    : static_cast<std::int32_t>(n);
        return true;
    }
    bool boolean(bool &out) {
        if (pos == size || (bytes[pos] != 0xf4 && bytes[pos] != 0xf5))
            return fail(Error::Canonical);
        out = bytes[pos++] == 0xf5;
        return true;
    }
    bool fixed(void *p, std::size_t n) {
        if (!shape(2, n) || n > size - pos)
            return fail(Error::Canonical);
        if (n)
            std::memcpy(p, bytes + pos, n);
        pos += n;
        return true;
    }
    bool text(const char *s) {
        const auto n = std::strlen(s);
        if (!shape(3, n) || n > size - pos || std::memcmp(bytes + pos, s, n))
            return fail(Error::Canonical);
        pos += n;
        return true;
    }
    bool key(unsigned n) {
        return shape(0, n);
    }
};
template <typename IO, typename T> bool enumeration(IO &io, T &v, unsigned maximum) {
    unsigned n = static_cast<unsigned>(v);
    if (!io.u(n) || n > maximum)
        return io.fail(Error::Range);
    if (IO::Reading)
        v = static_cast<T>(n);
    return true;
}
template <typename IO> bool code(IO &io, Identity &v) {
    return io.fixed(v.bytes, 16);
}
template <typename IO> bool code(IO &io, Digest &v) {
    return io.fixed(v.bytes, 32);
}
template <typename IO> bool code(IO &io, Context &v) {
    return io.shape(4, 9) && code(io, v.event) && code(io, v.run) && code(io, v.station) &&
           code(io, v.box) && code(io, v.sourceEpoch) && code(io, v.deployment) &&
           code(io, v.binding) && code(io, v.content) && io.u(v.bindingGeneration);
}
template <typename IO> bool code(IO &io, Location &v) {
    return io.shape(4, 6) && io.u(v.offset) && io.u(v.source) && io.u(v.station) && io.u(v.index) &&
           io.u(v.subindex) && io.u(v.role);
}
template <typename IO> bool code(IO &io, configuration::module_ir::Node &v) {
    if (!io.shape(4, 7) || !code(io, v.location) ||
        !enumeration(io, v.op, static_cast<unsigned>(Op::Override)) ||
        !enumeration(io, v.type, static_cast<unsigned>(Type::Solved)) || !io.integer(v.value) ||
        !io.u(v.ref) || !io.shape(4, 8))
        return false;
    for (auto &child : v.children)
        if (!io.u(child))
            return false;
    return io.u(v.count) && (v.count <= 8 || io.fail(Error::Range));
}
template <typename IO> bool code(IO &io, configuration::module_ir::Action &v) {
    return io.shape(4, 5) && code(io, v.location) &&
           enumeration(io, v.op, static_cast<unsigned>(Op::Override)) && io.u(v.target) &&
           io.u(v.resource) && io.u(v.expr);
}
template <typename IO> bool code(IO &io, configuration::module_ir::Rule &v) {
    return io.shape(4, 16) && code(io, v.location) && io.u(v.owner) && io.u(v.id) &&
           io.u(v.guard) && io.u(v.firstAction) && io.u(v.firstTrigger) && io.u(v.timer) &&
           io.u(v.fallback) && io.u(v.group) && io.u(v.generation) && io.u(v.priority) &&
           io.u(v.actionCount) && io.u(v.triggerCount) && io.u(v.trigger) && io.u(v.policy) &&
           io.boolean(v.once);
}
template <typename IO> bool code(IO &io, Rule &v) {
    return io.shape(4, 3) && code(io, v.ir) && io.u(v.name) && code(io, v.stable);
}
template <typename IO> bool code(IO &io, Symbol &v) {
    return io.shape(4, 3) && io.u(v.name) &&
           enumeration(io, v.type, static_cast<unsigned>(Type::Solved)) && io.integer(v.initial);
}
template <typename IO> bool code(IO &io, Port &v) {
    return io.shape(4, 6) && io.u(v.name) &&
           enumeration(io, v.type, static_cast<unsigned>(Type::Solved)) && io.boolean(v.input) &&
           code(io, v.exclusive) && io.u(v.firstResource) && io.u(v.resourceCount);
}
template <typename IO> bool code(IO &io, Resource &v) {
    return io.shape(4, 2) && io.u(v.name) &&
           enumeration(io, v.type, static_cast<unsigned>(Type::Solved));
}
template <typename IO> bool code(IO &io, Id &v) {
    return io.u(v);
}
template <typename IO> bool code(IO &io, std::uint32_t &v) {
    return io.u(v);
}
template <typename IO> bool code(IO &io, FactRef &v);

template <typename T, typename F> bool items(Writer &io, Pool<T> &pool, F transfer) {
    if (!io.head(4, pool.count))
        return false;
    for (std::size_t i = 0; i < pool.count; ++i) {
        T copy = pool.data[i];
        if (!transfer(io, copy))
            return false;
    }
    return true;
}
template <typename T, typename F> bool items(Reader &io, Pool<T> &pool, F transfer) {
    std::uint64_t count = 0;
    if (!io.head(4, count))
        return false;
    if (count > pool.capacity || count > 65534)
        return io.fail(Error::Capacity);
    pool.count = static_cast<std::size_t>(count);
    for (std::size_t i = 0; i < pool.count; ++i) {
        pool.data[i] = T{};
        if (!transfer(io, pool.data[i]))
            return false;
    }
    return true;
}
template <typename IO, typename T> bool items(IO &io, Pool<T> &pool) {
    return items(io, pool, [](IO &port, T &value) { return code(port, value); });
}
bool strings(Writer &io, Pool<char> &pool) {
    return io.head(2, pool.count) && io.raw(pool.data, pool.count);
}
bool strings(Reader &io, Pool<char> &pool) {
    std::uint64_t count = 0;
    if (!io.head(2, count))
        return false;
    if (count > pool.capacity)
        return io.fail(Error::Capacity);
    if (count > io.size - io.pos)
        return io.fail(Error::Canonical);
    pool.count = static_cast<std::size_t>(count);
    if (pool.count)
        std::memcpy(pool.data, io.bytes + io.pos, pool.count);
    io.pos += pool.count;
    return true;
}
template <typename T> bool spanIndex(const Pool<T> &pool, Span<T> span, std::size_t &index) {
    index = 0;
    if (!span.count)
        return true;
    const auto first = reinterpret_cast<std::uintptr_t>(pool.data);
    const auto at = reinterpret_cast<std::uintptr_t>(span.data);
    if (!pool.data || !span.data || at < first || (at - first) % sizeof(T))
        return false;
    index = (at - first) / sizeof(T);
    return index <= pool.count && span.count <= pool.count - index;
}
template <typename T> bool span(Writer &io, const Pool<T> &pool, Span<T> &view) {
    std::size_t start = 0, count = view.count;
    return (spanIndex(pool, view, start) || io.fail(Error::Reference)) && io.shape(4, 2) &&
           io.u(start) && io.u(count);
}
template <typename T> bool span(Reader &io, const Pool<T> &pool, Span<T> &view) {
    std::size_t start = 0, count = 0;
    if (!io.shape(4, 2) || !io.u(start) || !io.u(count))
        return false;
    if (start > pool.count || count > pool.count - start || (!count && start))
        return io.fail(Error::Reference);
    view.data = pool.data ? pool.data + start : nullptr;
    view.count = count;
    return true;
}
template <typename IO> bool subject(IO &io, Subject &v, const Pool<Identity> &epochs) {
    auto &b = v.binding;
    return (!b.name || io.fail(Error::Reference)) && io.shape(4, 9) && io.u(v.name) &&
           code(io, b.subject) && code(io, b.station) && code(io, b.box) && code(io, b.epoch) &&
           code(io, b.binding) && span(io, epochs, b.epochs) && io.u(b.factTypes) &&
           io.boolean(b.epochsComplete);
}
template <typename IO> bool catalog(IO &io, Program &p) {
    // Epochs precede subjects because their spans are exact owned pool slices.
    return io.shape(5, 16) && io.key(0) && io.text("larp.rules.catalog.v1") && io.key(1) &&
           code(io, p.context) && io.key(2) && io.shape(4, 2) && io.u(p.language) &&
           io.boolean(p.manualOverride) && io.key(3) && strings(io, p.strings) && io.key(4) &&
           items(io, p.offsets) && io.key(5) && items(io, p.nodes) && io.key(6) &&
           items(io, p.actions) && io.key(7) && items(io, p.rules) && io.key(8) &&
           items(io, p.values) && io.key(9) && items(io, p.timers) && io.key(10) &&
           items(io, p.ports) && io.key(11) && items(io, p.epochs) && io.key(12) &&
           items(io, p.subjects,
                 [&p](IO &port, Subject &s) { return subject(port, s, p.epochs); }) &&
           io.key(13) && items(io, p.resources) && io.key(14) && items(io, p.allowedResources) &&
           io.key(15) && items(io, p.triggers);
}
template <typename IO> bool code(IO &io, FactId &v) {
    return io.shape(4, 4) && code(io, v.event) && code(io, v.origin) && code(io, v.epoch) &&
           io.u(v.sequence);
}
template <typename IO> bool code(IO &io, FactRef &v) {
    return io.shape(4, 8) && code(io, v.id) && code(io, v.digest) && code(io, v.deployment) &&
           code(io, v.binding) && code(io, v.run) && code(io, v.station) && code(io, v.subject) &&
           enumeration(io, v.type, static_cast<unsigned>(FactType::LocalOverride)) &&
           (v.type == FactType::TaskSolved || v.type == FactType::LocalOverride ||
            io.fail(Error::Range));
}
template <typename IO> bool code(IO &io, Fact &v) {
    return io.shape(4, 3) && io.u(v.subject) && code(io, v.ref) &&
           enumeration(io, v.kind, static_cast<unsigned>(FactClass::Receipt));
}
template <typename IO> bool code(IO &io, rules::Fence &v) {
    return io.shape(4, 10) && io.u(v.progress) && io.u(v.profile) && io.u(v.trust) &&
           io.u(v.facts) && io.u(v.ledger) && io.u(v.resources) && io.u(v.rights) &&
           io.u(v.capacity) && io.u(v.lastIssued) && io.u(v.activeTime);
}
template <typename IO> bool code(IO &io, OverrideAuthority &v) {
    return io.shape(4, 9) && code(io, v.peer) && io.u(v.principal) && io.u(v.roleEpoch) &&
           io.u(v.connectionToken) && io.u(v.registration) && io.u(v.lifecycle) &&
           io.u(v.connectionHandle) && io.u(v.registryRevision) && io.u(v.eventRevision);
}
template <typename IO> bool code(IO &io, OverrideRecord &v) {
    return io.shape(4, 19) && code(io, v.context) && code(io, v.program) && code(io, v.expected) &&
           code(io, v.authority) && code(io, v.ownerBoot) && code(io, v.token) &&
           code(io, v.prepareId) && code(io, v.confirmId) && code(io, v.operation) &&
           code(io, v.rule) && code(io, v.prepareDigest) && code(io, v.confirmDigest) &&
           io.u(v.original) && io.u(v.replacement) && io.u(v.originalGeneration) &&
           io.u(v.replacementGeneration) && io.u(v.preparedAt) && io.u(v.expiresAt) &&
           io.u(v.confirmedAt);
}
template <typename IO> bool input(IO &io, Input &v, OverrideRecord &record) {
    return io.shape(4, 12) && code(io, v.context) &&
           enumeration(io, v.kind, static_cast<unsigned>(InputKind::Override)) &&
           io.u(v.bootGeneration) && io.u(v.sequence) && io.u(v.port) && io.u(v.timer) &&
           io.u(v.armGeneration) && enumeration(io, v.type, static_cast<unsigned>(Type::Solved)) &&
           io.boolean(v.known) && io.integer(v.value) && code(io, v.fact) && code(io, record);
}
template <typename IO> bool evidence(IO &io, Evidence &v, const Pool<FactRef> &refs) {
    return span(io, refs, v.refs);
}
template <typename IO> bool value(IO &io, ValueWrite &v, const Pool<FactRef> &refs) {
    return io.shape(4, 4) && io.u(v.target) &&
           enumeration(io, v.value.type, static_cast<unsigned>(Type::Solved)) &&
           io.integer(v.value.value) && evidence(io, v.value.evidence, refs);
}
template <typename IO> bool timer(IO &io, TimerWrite &v, const Pool<FactRef> &refs) {
    return io.shape(4, 5) && io.u(v.target) && io.boolean(v.value.active) &&
           io.u(v.value.deadline) && io.u(v.value.generation) &&
           evidence(io, v.value.evidence, refs);
}
template <typename IO> bool latch(IO &io, Latch &v, const Pool<FactRef> &refs) {
    return io.shape(4, 3) && code(io, v.rule) && io.u(v.generation) &&
           evidence(io, v.evidence, refs);
}
template <typename IO> bool emission(IO &io, Emission &v, const Pool<FactRef> &refs) {
    return io.shape(4, 6) && io.u(v.subject) && code(io, v.rule) && io.u(v.generation) &&
           code(io, v.candidate) && evidence(io, v.dependencies, refs) &&
           enumeration(io, v.type, static_cast<unsigned>(FactType::LocalOverride)) &&
           (v.type == FactType::TaskSolved || v.type == FactType::LocalOverride ||
            io.fail(Error::Range));
}
template <typename IO> bool intent(IO &io, Intent &v, const Pool<FactRef> &refs) {
    return io.shape(4, 5) && io.u(v.port) && io.u(v.resource) && code(io, v.rule) &&
           code(io, v.exclusive) && evidence(io, v.evidence, refs);
}
template <typename IO> bool plan(IO &io, Plan &p, OverrideRecord &record) {
    return io.shape(4, 12) && code(io, p.context) && code(io, p.expected) && io.shape(4, 2) &&
           code(io, p.receipt.context) && input(io, p.receipt.input, record) &&
           io.u(p.consumedTimer) && items(io, p.evidence) &&
           items(io, p.values,
                 [&p](IO &port, ValueWrite &v) { return value(port, v, p.evidence); }) &&
           items(io, p.timers,
                 [&p](IO &port, TimerWrite &v) { return timer(port, v, p.evidence); }) &&
           items(io, p.latches, [&p](IO &port, Latch &v) { return latch(port, v, p.evidence); }) &&
           items(io, p.emissions,
                 [&p](IO &port, Emission &v) { return emission(port, v, p.evidence); }) &&
           items(io, p.intents,
                 [&p](IO &port, Intent &v) { return intent(port, v, p.evidence); }) &&
           io.boolean(p.admitsFact) && code(io, p.fact);
}
bool blob(Writer &io, Span<std::uint8_t> &view, Pool<std::uint8_t> &, std::size_t maximum) {
    return (view.count <= maximum || io.fail(Error::Profile)) &&
           (view.data || !view.count || io.fail(Error::InvalidArgument)) &&
           io.head(2, view.count) && io.raw(view.data, view.count);
}
bool blob(Reader &io, Span<std::uint8_t> &view, Pool<std::uint8_t> &storage, std::size_t maximum) {
    std::uint64_t count = 0;
    if (!io.head(2, count))
        return false;
    if (count > maximum)
        return io.fail(Error::Profile);
    if (count > io.size - io.pos)
        return io.fail(Error::Canonical);
    if (count > storage.capacity - storage.count)
        return io.fail(Error::Capacity);
    view.count = static_cast<std::size_t>(count);
    view.data = count ? storage.data + storage.count : nullptr;
    if (count)
        std::memcpy(storage.data + storage.count, io.bytes + io.pos, view.count);
    storage.count += view.count;
    io.pos += view.count;
    return true;
}
template <typename IO> bool signedFact(IO &io, SignedFact &v, Pool<std::uint8_t> &storage) {
    return io.shape(4, 3) && code(io, v.ref) && io.fixed(v.publicKey, 64) &&
           blob(io, v.cose, storage, MaximumFactBytes) &&
           (v.cose.count || io.fail(Error::InvalidPlan));
}
template <typename IO> bool application(IO &io, Application &a, Pool<std::uint8_t> &storage) {
    return io.shape(5, 8) && io.key(0) && io.text("larp.rules.application.v1") && io.key(1) &&
           code(io, a.operation) && io.key(2) && code(io, a.request) && io.key(3) &&
           code(io, a.catalog) && io.key(4) && plan(io, a.plan, a.overrideRecord) && io.key(5) &&
           items(io, a.proofs,
                 [&storage](IO &port, SignedFact &v) { return signedFact(port, v, storage); }) &&
           io.key(6) &&
           items(io, a.outbox,
                 [&storage](IO &port, SignedFact &v) { return signedFact(port, v, storage); }) &&
           io.key(7) && blob(io, a.authorization, storage, MaximumBytes);
}
void clear(Application &a, ApplicationStorage *storage = nullptr) {
    a.decoded = false;
    auto &p = a.plan;
    p.ready = p.admitsFact = false;
    p.receipt = {};
    p.fact = {};
    a.overrideRecord = {};
    p.values.count = p.timers.count = p.latches.count = p.emissions.count = 0;
    p.intents.count = p.evidence.count = a.proofs.count = a.outbox.count = 0;
    a.authorization = {};
    if (storage) {
        if (storage->bytes.data && storage->bytes.count)
            std::memset(storage->bytes.data, 0, storage->bytes.count);
        storage->bytes.count = 0;
    }
}
bool applicationRanges(Ranges &r, const Application &a) {
    const auto &p = a.plan;
    return r.add(&a, sizeof(a)) && r.pool(p.values) && r.pool(p.timers) && r.pool(p.latches) &&
           r.pool(p.emissions) && r.pool(p.intents) && r.pool(p.evidence) && r.pool(a.proofs) &&
           r.pool(a.outbox);
}
bool same(const Identity &a, const Identity &b) {
    return std::memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}
bool same(const FactId &a, const FactId &b) {
    return same(a.event, b.event) && same(a.origin, b.origin) && same(a.epoch, b.epoch) &&
           a.sequence == b.sequence;
}
bool same(const Digest &a, const Digest &b) {
    return std::memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}
bool same(const FactRef &a, const FactRef &b) {
    return same(a.id, b.id) && same(a.digest, b.digest) && same(a.deployment, b.deployment) &&
           same(a.binding, b.binding) && same(a.run, b.run) && same(a.station, b.station) &&
           same(a.subject, b.subject) && a.type == b.type;
}
bool nonzero(const Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
// Representation consistency is not a mathematical or historical proof.
bool applicationShape(const Application &a) {
    const auto &p = a.plan;
    if (!nonzero(a.operation) || !sameContext(p.context, p.receipt.context) ||
        !sameContext(p.context, p.receipt.input.context) || p.emissions.count > MaximumOutgoing ||
        a.outbox.count != p.emissions.count ||
        p.expected.lastIssued > std::numeric_limits<std::uint64_t>::max() - p.emissions.count)
        return false;
    const OverrideRecord empty;
    if (p.receipt.input.kind == InputKind::Override) {
        if (!a.authorization.count || !sameContext(a.overrideRecord.context, p.context) ||
            !same(a.overrideRecord.program, a.catalog) ||
            !same(a.overrideRecord.operation, a.operation) ||
            !overridePlanningFence(a.overrideRecord.expected, p.expected))
            return false;
    } else if (!sameOverrideRecord(a.overrideRecord, empty)) {
        return false;
    }
    for (std::size_t i = 0; i < a.proofs.count; ++i) {
        const auto &ref = a.proofs.data[i].ref;
        if (!ref.id.sequence || !nonzero(ref.id.event) || !nonzero(ref.id.origin) ||
            !nonzero(ref.id.epoch))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (same(ref.id, a.proofs.data[j].ref.id))
                return false;
    }
    auto proved = [&](const FactRef &ref) {
        for (std::size_t i = 0; i < a.proofs.count; ++i)
            if (same(ref, a.proofs.data[i].ref))
                return true;
        return false;
    };
    for (std::size_t i = 0; i < p.evidence.count; ++i)
        if (!proved(p.evidence.data[i]))
            return false;
    if (p.receipt.input.kind == InputKind::Fact && !proved(p.receipt.input.fact.ref))
        return false;
    for (std::size_t i = 0; i < a.outbox.count; ++i) {
        const auto &ref = a.outbox.data[i].ref;
        const auto &emission = p.emissions.data[i];
        if (!same(ref.id, emission.candidate) || ref.type != emission.type ||
            ref.id.sequence != p.expected.lastIssued + i + 1 ||
            !same(ref.id.event, p.context.event) || !same(ref.id.origin, p.context.box) ||
            !same(ref.id.epoch, p.context.sourceEpoch) ||
            !same(ref.deployment, p.context.deployment) || !same(ref.binding, p.context.binding) ||
            !same(ref.run, p.context.run) || !same(ref.station, p.context.station))
            return false;
    }
    return true;
}
Result failed(Error error, std::size_t offset = 0) {
    Result r;
    r.error = error;
    r.offset = offset;
    return r;
}
Result succeeded(Scope scope, std::size_t offset) {
    Result r;
    r.scope = scope;
    r.offset = offset;
    return r;
}
} // namespace

Result encodeCatalog(const rules::Program &source, Pool<std::uint8_t> &destination) {
    Ranges ranges;
    // Check aliases before clearing even a count in a possibly aliased wrapper.
    if (!catalogRanges(ranges, source) || !ranges.add(&destination, sizeof(destination)) ||
        !ranges.pool(destination, false))
        return failed(Error::Alias);
    destination.count = 0;
    if (!source.ready)
        return failed(Error::InvalidProgram);
    Program copy = source;
    Writer writer(destination.data,
                  destination.capacity < MaximumBytes ? destination.capacity : MaximumBytes);
    if (!catalog(writer, copy))
        return failed(writer.error, writer.pos);
    destination.count = writer.pos;
    return succeeded(Scope::EncodedUnpublished, writer.pos);
}
Result decodeCatalog(Span<std::uint8_t> source, rules::Program &inactive) {
    Ranges ranges;
    if (!catalogRanges(ranges, inactive) || !ranges.add(source.data, source.count))
        return failed(Error::Alias);
    clear(inactive);
    if (!source.count || source.count > MaximumBytes)
        return failed(Error::Profile);
    Reader reader(source.data, source.count);
    if (!catalog(reader, inactive) || reader.pos != source.count) {
        clear(inactive);
        return failed(reader.error == Error::None ? Error::Canonical : reader.error, reader.pos);
    }
    // The builder computes groups; a storage decoder must instead verify the
    // exact already-stored group values, never silently repair a different blob.
    if (inactive.rules.count > 64) {
        clear(inactive);
        return failed(Error::InvalidProgram, reader.pos);
    }
    Id groups[64] = {};
    for (std::size_t i = 0; i < inactive.rules.count; ++i)
        groups[i] = inactive.rules.data[i].ir.group;
    const auto validation = rules::validateOwnedProgram(inactive);
    if (validation.decision != rules::Decision::Ready) {
        clear(inactive);
        return failed(Error::InvalidProgram, reader.pos);
    }
    for (std::size_t i = 0; i < inactive.rules.count; ++i)
        if (groups[i] != inactive.rules.data[i].ir.group) {
            clear(inactive);
            return failed(Error::InvalidProgram, reader.pos);
        }
    inactive.ready = true;
    return succeeded(Scope::DecodedUnauthenticated, reader.pos);
}
Result encodeApplication(const Application &source, Pool<std::uint8_t> &destination) {
    Ranges ranges;
    if (!applicationRanges(ranges, source) || !ranges.add(&destination, sizeof(destination)) ||
        !ranges.pool(destination, false))
        return failed(Error::Alias);
    // Byte views may share an immutable proof record with each other, but none
    // may overlap the output (or its wrapper). Check them without rejecting
    // legitimate shared source bytes against one another.
    const auto outputStart = reinterpret_cast<std::uintptr_t>(destination.data);
    auto external = [&](Span<std::uint8_t> bytes) {
        const auto at = reinterpret_cast<std::uintptr_t>(bytes.data);
        if ((!bytes.data && bytes.count) ||
            bytes.count > std::numeric_limits<std::uintptr_t>::max() - at)
            return false;
        if (bytes.count && destination.capacity && at < outputStart + destination.capacity &&
            outputStart < at + bytes.count)
            return false;
        const auto wrapper = reinterpret_cast<std::uintptr_t>(&destination);
        return !bytes.count || at >= wrapper + sizeof(destination) || wrapper >= at + bytes.count;
    };
    if (!external(source.authorization))
        return failed(Error::Alias);
    for (const auto *pool : {&source.proofs, &source.outbox})
        for (std::size_t i = 0; i < pool->count; ++i)
            if (!external(pool->data[i].cose))
                return failed(Error::Alias);
    destination.count = 0;
    if (!source.plan.ready || !applicationShape(source))
        return failed(Error::InvalidPlan);
    if (!sameOverrideRecord(source.overrideRecord,
                            source.plan.receipt.input.overrideAdmission.record()))
        return failed(Error::Admission);
    Application copy = source;
    Pool<std::uint8_t> unused;
    Writer writer(destination.data,
                  destination.capacity < MaximumBytes ? destination.capacity : MaximumBytes);
    if (!application(writer, copy, unused))
        return failed(writer.error, writer.pos);
    destination.count = writer.pos;
    return succeeded(Scope::EncodedUnpublished, writer.pos);
}
Result decodeApplication(Span<std::uint8_t> source, Application &inactive,
                         ApplicationStorage &storage) {
    Ranges ranges;
    if (!applicationRanges(ranges, inactive) || !ranges.add(&storage, sizeof(storage)) ||
        !ranges.pool(storage.bytes) || !ranges.add(source.data, source.count))
        return failed(Error::Alias);
    clear(inactive, &storage);
    if (!source.count || source.count > MaximumBytes)
        return failed(Error::Profile);
    Reader reader(source.data, source.count);
    if (!application(reader, inactive, storage.bytes) || reader.pos != source.count ||
        !applicationShape(inactive)) {
        clear(inactive, &storage);
        return failed(reader.error == Error::None ? Error::InvalidPlan : reader.error, reader.pos);
    }
    inactive.decoded = true;
    // Never create a live admission or executable plan from stored bytes.
    return succeeded(Scope::DecodedUnauthenticated, reader.pos);
}
} // namespace rules_progress
} // namespace execution
} // namespace larp
