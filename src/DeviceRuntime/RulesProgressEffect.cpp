#include "DeviceRuntime/RulesProgressEffect.h"
#include <cstring>
#include <limits>

namespace larp {
namespace devices {
namespace rules_progress {
namespace {
constexpr char Domain[] = "larp.rules.effect.v1";
struct Bounds {
    std::uintptr_t first = 0, last = 0;
};
bool bounds(const void *p, std::size_t n, Bounds &r) {
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
bool overlap(Bounds a, Bounds b) {
    return a.last && b.last && a.first < b.last && b.first < a.last;
}
bool nonzero(const rules::Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
bool same(const rules::Identity &a, const rules::Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool shape(const Effect &e) {
    return nonzero(e.application) && nonzero(e.ownerBoot) &&
           e.intentIndex < MaximumIntentReservations &&
           ((e.phase == EffectPhase::Claim && e.result == HandlerResult::None) ||
            (e.phase == EffectPhase::Result &&
             (e.result == HandlerResult::Returned || e.result == HandlerResult::Failed ||
              e.result == HandlerResult::Unknown)));
}
struct Writer {
    std::uint8_t *p;
    std::size_t capacity, pos = 0;
    Writer(std::uint8_t *out, std::size_t n) : p(out), capacity(n) {}
    bool byte(std::uint8_t b) {
        if (pos == capacity)
            return false;
        p[pos++] = b;
        return true;
    }
    bool head(unsigned major, std::uint64_t n) {
        if (n < 24)
            return byte(static_cast<std::uint8_t>((major << 5) | n));
        const unsigned length = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        if (!byte(static_cast<std::uint8_t>((major << 5) | (length == 1   ? 24
                                                            : length == 2 ? 25
                                                            : length == 4 ? 26
                                                                          : 27))))
            return false;
        for (unsigned i = length; i; --i)
            if (!byte(static_cast<std::uint8_t>(n >> (8 * (i - 1)))))
                return false;
        return true;
    }
    bool exact(unsigned major, std::uint64_t n) {
        return head(major, n);
    }
    bool key(unsigned n) {
        return head(0, n);
    }
    template <typename T> bool u(T &n) {
        return head(0, static_cast<std::uint64_t>(n));
    }
    bool raw(const std::uint8_t *bytes, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            if (!byte(bytes[i]))
                return false;
        return true;
    }
    template <std::size_t N> bool bytes(std::uint8_t (&value)[N]) {
        return head(2, N) && raw(value, N);
    }
    bool text() {
        return head(3, sizeof(Domain) - 1) &&
               raw(reinterpret_cast<const std::uint8_t *>(Domain), sizeof(Domain) - 1);
    }
};
struct Reader {
    const std::uint8_t *p;
    std::size_t size, pos = 0;
    Reader(const std::uint8_t *bytes, std::size_t n) : p(bytes), size(n) {}
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
        const unsigned length = 1u << (ai - 24);
        if (length > size - pos)
            return false;
        n = 0;
        for (unsigned i = 0; i < length; ++i)
            n = (n << 8) | p[pos++];
        return !((length == 1 && n < 24) || (length == 2 && n <= 255) ||
                 (length == 4 && n <= 65535) || (length == 8 && n <= UINT32_MAX));
    }
    bool exact(unsigned major, std::uint64_t wanted) {
        std::uint64_t n = 0;
        return head(major, n) && n == wanted;
    }
    bool key(unsigned n) {
        return exact(0, n);
    }
    template <typename T> bool u(T &out) {
        std::uint64_t n = 0;
        if (!head(0, n) || n > std::numeric_limits<T>::max())
            return false;
        out = static_cast<T>(n);
        return true;
    }
    template <std::size_t N> bool bytes(std::uint8_t (&out)[N]) {
        if (!exact(2, N) || N > size - pos)
            return false;
        std::memcpy(out, p + pos, N);
        pos += N;
        return true;
    }
    bool text() {
        const auto n = sizeof(Domain) - 1;
        if (!exact(3, n) || n > size - pos || std::memcmp(p + pos, Domain, n))
            return false;
        pos += n;
        return true;
    }
};
template <typename IO> bool fence(IO &io, rules::Fence &f) {
    return io.exact(4, 10) && io.u(f.progress) && io.u(f.profile) && io.u(f.trust) &&
           io.u(f.facts) && io.u(f.ledger) && io.u(f.resources) && io.u(f.rights) &&
           io.u(f.capacity) && io.u(f.lastIssued) && io.u(f.activeTime);
}
template <typename IO> bool transfer(IO &io, Effect &e) {
    unsigned phase = static_cast<unsigned>(e.phase), result = static_cast<unsigned>(e.result);
    const bool ok = io.exact(5, 8) && io.key(0) && io.text() && io.key(1) &&
                    io.bytes(e.application.bytes) && io.key(2) && io.bytes(e.catalog.bytes) &&
                    io.key(3) && io.u(e.intentIndex) && io.key(4) && io.bytes(e.ownerBoot.bytes) &&
                    io.key(5) && fence(io, e.expected) && io.key(6) && io.u(phase) && io.key(7) &&
                    io.u(result);
    if (!ok || phase > 2 || result > 3)
        return false;
    e.phase = static_cast<EffectPhase>(phase);
    e.result = static_cast<HandlerResult>(result);
    return true;
}
EffectResult rejected(EffectError error, std::size_t offset = 0) {
    EffectResult r;
    r.error = error;
    r.offset = offset;
    return r;
}
EffectResult success(std::size_t offset = 0) {
    EffectResult r;
    r.valid = true;
    r.offset = offset;
    return r;
}
} // namespace

bool effectDomain(const std::uint8_t *bytes, std::size_t count) {
    if (!bytes || !count || count > ProgressLimits::PayloadBytes)
        return false;
    Reader r(bytes, count);
    return r.exact(5, 8) && r.key(0) && r.text();
}
EffectResult encodeEffect(const Effect &source, rules::Pool<std::uint8_t> &destination) {
    Bounds input, wrapper, output;
    if (!bounds(&source, sizeof(source), input) ||
        !bounds(&destination, sizeof(destination), wrapper) ||
        !bounds(destination.data, destination.capacity, output) || overlap(input, wrapper) ||
        overlap(input, output) || overlap(wrapper, output))
        return rejected(EffectError::Alias);
    destination.count = 0;
    if (!shape(source))
        return rejected(EffectError::Value);
    Effect copy = source;
    Writer writer(destination.data, destination.capacity < ProgressLimits::PayloadBytes
                                        ? destination.capacity
                                        : ProgressLimits::PayloadBytes);
    if (!transfer(writer, copy))
        return rejected(EffectError::Capacity, writer.pos);
    destination.count = writer.pos;
    return success(writer.pos);
}
EffectResult decodeEffect(const std::uint8_t *bytes, std::size_t count, Effect &destination) {
    Bounds input, output;
    if (!bounds(bytes, count, input) || !bounds(&destination, sizeof(destination), output) ||
        overlap(input, output))
        return rejected(EffectError::Alias);
    destination = {};
    if (!bytes || !count || count > ProgressLimits::PayloadBytes)
        return rejected(EffectError::Argument);
    Effect staged;
    Reader reader(bytes, count);
    if (!transfer(reader, staged) || reader.pos != count)
        return rejected(EffectError::Canonical, reader.pos);
    if (!shape(staged))
        return rejected(EffectError::Value, reader.pos);
    destination = staged;
    return success(reader.pos);
}
EffectResult validateEffectRecord(const Effect &e, const ProgressRecord &record,
                                  const IntentReservation &reservation) {
    if (!shape(e))
        return rejected(EffectError::Value);
    if (reservation.index != e.intentIndex || !nonzero(reservation.claim) ||
        !nonzero(reservation.result) || same(reservation.claim, reservation.result) ||
        same(reservation.claim, e.application) || same(reservation.result, e.application) ||
        record.issueCount || record.firstIssued || record.lastIssued)
        return rejected(EffectError::Header);
    const auto kind =
        e.phase == EffectPhase::Claim ? ProgressKind::EffectClaim : ProgressKind::EffectResult;
    const auto &id = e.phase == EffectPhase::Claim ? reservation.claim : reservation.result;
    if (record.kind != kind || std::memcmp(record.operationId.data(), id.bytes, 16))
        return rejected(EffectError::Header);
    return success();
}
} // namespace rules_progress
} // namespace devices
} // namespace larp
