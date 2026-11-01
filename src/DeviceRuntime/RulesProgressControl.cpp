#include "DeviceRuntime/RulesProgressControl.h"
#include <cstring>
#include <limits>
namespace larp {
namespace devices {
namespace rules_progress {
namespace {
constexpr char Domain[] = "larp.rules.control.v1";
struct Range {
    std::uintptr_t first = 0, last = 0;
};
bool range(const void *p, std::size_t n, Range &r) {
    if (!n) {
        r = {};
        return true;
    }
    if (!p)
        return false;
    r.first = reinterpret_cast<std::uintptr_t>(p);
    if (n > std::numeric_limits<std::uintptr_t>::max() - r.first)
        return false;
    r.last = r.first + n;
    return true;
}
bool overlap(Range a, Range b) {
    return a.last && b.last && a.first < b.last && b.first < a.last;
}
bool disjoint(Range *r, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        for (unsigned j = 0; j < i; ++j)
            if (overlap(r[i], r[j]))
                return false;
    return true;
}
bool nonzero(const rules::Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
struct Writer {
    std::uint8_t *out;
    std::size_t capacity, pos = 0;
    ControlError error = ControlError::None;
    Writer(std::uint8_t *p, std::size_t n) : out(p), capacity(n) {}
    bool byte(std::uint8_t b) {
        if (pos == capacity) {
            error = ControlError::Capacity;
            return false;
        }
        out[pos++] = b;
        return true;
    }
    bool head(unsigned major, std::uint64_t n) {
        if (n < 24)
            return byte(static_cast<std::uint8_t>((major << 5) | n));
        unsigned count = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        if (!byte(static_cast<std::uint8_t>((major << 5) | (count == 1   ? 24
                                                            : count == 2 ? 25
                                                            : count == 4 ? 26
                                                                         : 27))))
            return false;
        for (unsigned i = count; i; --i)
            if (!byte(static_cast<std::uint8_t>(n >> (8 * (i - 1)))))
                return false;
        return true;
    }
    bool shape(unsigned major, std::uint64_t n) {
        return head(major, n);
    }
    template <typename T> bool u(T &n) {
        return head(0, static_cast<std::uint64_t>(n));
    }
    bool key(unsigned n) {
        return head(0, n);
    }
    bool raw(const std::uint8_t *p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            if (!byte(p[i]))
                return false;
        return true;
    }
    template <std::size_t N> bool bytes(std::uint8_t (&p)[N]) {
        return head(2, N) && raw(p, N);
    }
    bool text() {
        return head(3, sizeof(Domain) - 1) &&
               raw(reinterpret_cast<const std::uint8_t *>(Domain), sizeof(Domain) - 1);
    }
    bool blob(rules::Span<std::uint8_t> &p, rules::Pool<std::uint8_t> &) {
        return head(2, p.count) && raw(p.data, p.count);
    }
};
struct Reader {
    const std::uint8_t *in;
    std::size_t size, pos = 0;
    ControlError error = ControlError::Canonical;
    Reader(const std::uint8_t *p, std::size_t n) : in(p), size(n) {}
    bool head(unsigned major, std::uint64_t &n) {
        if (pos == size)
            return false;
        const unsigned b = in[pos++], ai = b & 31;
        if ((b >> 5) != major || ai > 27)
            return false;
        if (ai < 24) {
            n = ai;
            return true;
        }
        const unsigned count = 1u << (ai - 24);
        if (count > size - pos)
            return false;
        n = 0;
        for (unsigned i = 0; i < count; ++i)
            n = (n << 8) | in[pos++];
        return !((count == 1 && n < 24) || (count == 2 && n <= 255) || (count == 4 && n <= 65535) ||
                 (count == 8 && n <= UINT32_MAX));
    }
    bool shape(unsigned major, std::uint64_t wanted) {
        std::uint64_t n = 0;
        return head(major, n) && n == wanted;
    }
    template <typename T> bool u(T &v) {
        std::uint64_t n = 0;
        if (!head(0, n) || n > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
            return false;
        v = static_cast<T>(n);
        return true;
    }
    bool key(unsigned n) {
        return shape(0, n);
    }
    template <std::size_t N> bool bytes(std::uint8_t (&p)[N]) {
        if (!shape(2, N) || N > size - pos)
            return false;
        std::memcpy(p, in + pos, N);
        pos += N;
        return true;
    }
    bool text() {
        if (!shape(3, sizeof(Domain) - 1) || sizeof(Domain) - 1 > size - pos ||
            std::memcmp(in + pos, Domain, sizeof(Domain) - 1))
            return false;
        pos += sizeof(Domain) - 1;
        return true;
    }
    bool blob(rules::Span<std::uint8_t> &p, rules::Pool<std::uint8_t> &owned) {
        std::uint64_t count = 0;
        if (!head(2, count) || count > MaximumControlEvidence || count > size - pos)
            return false;
        if (count > owned.capacity || (!owned.data && count)) {
            error = ControlError::Capacity;
            return false;
        }
        if (count)
            std::memcpy(owned.data, in + pos, static_cast<std::size_t>(count));
        owned.count = static_cast<std::size_t>(count);
        p.data = owned.data;
        p.count = owned.count;
        pos += owned.count;
        return true;
    }
};
template <class IO> bool context(IO &io, rules::Context &c) {
    return io.shape(4, 9) && io.bytes(c.event.bytes) && io.bytes(c.run.bytes) &&
           io.bytes(c.station.bytes) && io.bytes(c.box.bytes) && io.bytes(c.sourceEpoch.bytes) &&
           io.bytes(c.deployment.bytes) && io.bytes(c.binding.bytes) && io.bytes(c.content.bytes) &&
           io.u(c.bindingGeneration);
}
template <class IO> bool fence(IO &io, rules::Fence &f) {
    return io.shape(4, 10) && io.u(f.progress) && io.u(f.profile) && io.u(f.trust) &&
           io.u(f.facts) && io.u(f.ledger) && io.u(f.resources) && io.u(f.rights) &&
           io.u(f.capacity) && io.u(f.lastIssued) && io.u(f.activeTime);
}
template <class IO> bool clock(IO &io, ClockWitness &c) {
    return io.shape(4, 5) && io.bytes(c.boot.bytes) && io.u(c.monotonicBefore) &&
           io.u(c.monotonicAfter) && io.u(c.activeBefore) && io.u(c.activeAfter);
}
template <class IO> bool code(IO &io, Control &c, rules::Pool<std::uint8_t> &owned) {
    std::uint8_t kind = static_cast<std::uint8_t>(c.kind),
                 before = static_cast<std::uint8_t>(c.before),
                 after = static_cast<std::uint8_t>(c.after);
    if (!(io.shape(5, 12) && io.key(0) && io.text() && io.key(1) && io.bytes(c.operation.bytes) &&
          io.key(2) && io.bytes(c.request.bytes) && io.key(3) && io.bytes(c.catalog.bytes) &&
          io.key(4) && context(io, c.context) && io.key(5) && fence(io, c.expected) && io.key(6) &&
          fence(io, c.targetFence) && io.key(7) && io.u(kind) && io.key(8) && io.u(before) &&
          io.key(9) && io.u(after) && io.key(10) && clock(io, c.clock) && io.key(11) &&
          io.blob(c.authorization, owned)))
        return false;
    c.kind = static_cast<ControlKind>(kind);
    c.before = static_cast<rules::Mode>(before);
    c.after = static_cast<rules::Mode>(after);
    return true;
}
bool sane(const Control &c) {
    if (!nonzero(c.operation) || !nonzero(c.context.event) || !nonzero(c.context.run) ||
        !nonzero(c.context.station) || !nonzero(c.context.box) || !nonzero(c.context.sourceEpoch) ||
        !nonzero(c.clock.boot) || !c.authorization.count ||
        c.authorization.count > MaximumControlEvidence || !c.authorization.data ||
        c.clock.activeBefore != c.expected.activeTime ||
        c.clock.monotonicAfter < c.clock.monotonicBefore ||
        c.clock.activeAfter < c.clock.activeBefore)
        return false;
    if (c.before == rules::Mode::Running) {
        if (c.clock.activeAfter - c.clock.activeBefore !=
            c.clock.monotonicAfter - c.clock.monotonicBefore)
            return false;
    } else if (c.clock.activeAfter != c.clock.activeBefore)
        return false;
    if (c.kind == ControlKind::Activation) {
        if (c.expected.progress == UINT64_MAX ||
            c.targetFence.progress != c.expected.progress + 1 ||
            c.targetFence.lastIssued != c.expected.lastIssued ||
            c.targetFence.activeTime != c.expected.activeTime ||
            c.clock.activeAfter != c.clock.activeBefore || c.before != rules::Mode::Stopped ||
            c.after != rules::Mode::Stopped)
            return false;
        return c.targetFence.profile >= c.expected.profile &&
               c.targetFence.trust >= c.expected.trust && c.targetFence.facts >= c.expected.facts &&
               c.targetFence.ledger >= c.expected.ledger &&
               c.targetFence.resources >= c.expected.resources &&
               c.targetFence.rights >= c.expected.rights &&
               c.targetFence.capacity >= c.expected.capacity;
    }
    rules::Fence target = c.expected;
    target.activeTime = c.clock.activeAfter;
    if (c.kind != ControlKind::Checkpoint) {
        if (target.progress == UINT64_MAX)
            return false;
        ++target.progress;
    }
    if (!rules::sameFence(target, c.targetFence))
        return false;
    switch (c.kind) {
    case ControlKind::Start:
        return c.before == rules::Mode::Stopped && c.after == rules::Mode::Running;
    case ControlKind::Pause:
        return c.before == rules::Mode::Running && c.after == rules::Mode::Paused;
    case ControlKind::Resume:
        return c.before == rules::Mode::Paused && c.after == rules::Mode::Running;
    case ControlKind::Stop:
        return (c.before == rules::Mode::Running || c.before == rules::Mode::Paused) &&
               c.after == rules::Mode::Stopped;
    case ControlKind::Checkpoint:
        return c.before == rules::Mode::Running && c.after == rules::Mode::Running;
    case ControlKind::Activation:
        return false; // Never a mode shortcut.
    case ControlKind::RestoreStopped:
        return c.before == rules::Mode::RecoveryRequired && c.after == rules::Mode::Stopped;
    }
    return false;
}
ControlResult result(bool valid, ControlError error, std::size_t offset = 0) {
    ControlResult r;
    r.valid = valid;
    r.error = error;
    r.offset = offset;
    return r;
}
} // namespace
bool controlDomain(const std::uint8_t *p, std::size_t n) {
    if (!p)
        return false;
    Reader in(p, n);
    return in.shape(5, 12) && in.key(0) && in.text();
}
ControlResult encodeControl(const Control &source, rules::Pool<std::uint8_t> &destination) {
    Range r[4];
    if (destination.count > destination.capacity || !range(&source, sizeof(source), r[0]) ||
        !range(source.authorization.data, source.authorization.count, r[1]) ||
        !range(&destination, sizeof(destination), r[2]) ||
        !range(destination.data, destination.capacity, r[3]) || overlap(r[0], r[2]) ||
        overlap(r[0], r[3]) || overlap(r[1], r[2]) || overlap(r[1], r[3]) || overlap(r[2], r[3]))
        return result(false, ControlError::Alias);
    destination.count = 0;
    if (!sane(source))
        return result(false, ControlError::Value);
    Control copy = source;
    rules::Pool<std::uint8_t> unused;
    Writer writer(destination.data, destination.capacity < ProgressLimits::PayloadBytes
                                        ? destination.capacity
                                        : ProgressLimits::PayloadBytes);
    if (!code(writer, copy, unused))
        return result(false, writer.error, writer.pos);
    destination.count = writer.pos;
    return result(true, ControlError::None, writer.pos);
}
ControlResult decodeControl(const std::uint8_t *source, std::size_t count, Control &destination,
                            rules::Pool<std::uint8_t> &owned) {
    Range r[4];
    if (owned.count > owned.capacity || !range(source, count, r[0]) ||
        !range(&destination, sizeof(destination), r[1]) || !range(&owned, sizeof(owned), r[2]) ||
        !range(owned.data, owned.capacity, r[3]) || !disjoint(r, 4))
        return result(false, ControlError::Alias);
    destination = {};
    owned.count = 0;
    if (!count || count > ProgressLimits::PayloadBytes)
        return result(false, ControlError::Value);
    Reader reader(source, count);
    if (!code(reader, destination, owned) || reader.pos != count || !sane(destination)) {
        destination = {};
        owned.count = 0;
        return result(false, reader.error, reader.pos);
    }
    return result(true, ControlError::None, reader.pos);
}
} // namespace rules_progress
} // namespace devices
} // namespace larp
