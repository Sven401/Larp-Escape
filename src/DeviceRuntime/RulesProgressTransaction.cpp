#include "DeviceRuntime/RulesProgressTransaction.h"
#include <cstring>
#include <limits>
namespace larp {
namespace devices {
namespace rules_progress {
namespace {
constexpr char Domain[] = "larp.rules.transaction.v1";
constexpr std::size_t MaximumBytes = execution::rules_progress::MaximumBytes;
struct Range {
    std::uintptr_t first = 0, last = 0;
};
bool range(const void *p, std::size_t count, std::size_t size, Range &r) {
    if (count > std::numeric_limits<std::size_t>::max() / size)
        return false;
    const auto bytes = count * size;
    if (!bytes) {
        r = {};
        return true;
    }
    if (!p)
        return false;
    r.first = reinterpret_cast<std::uintptr_t>(p);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - r.first)
        return false;
    r.last = r.first + bytes;
    return true;
}
bool overlaps(Range a, Range b) {
    return a.last && b.last && a.first < b.last && b.first < a.last;
}
bool nonzero(const rules::Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
bool equal(const rules::Identity &a, const rules::Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool sane(const Transaction &t) {
    if (!nonzero(t.operation) || !t.application.data || !t.application.count ||
        t.application.count > MaximumBytes || t.intents.count > t.intents.capacity ||
        t.intents.count > MaximumIntentReservations || (!t.intents.data && t.intents.count))
        return false;
    for (std::size_t i = 0; i < t.intents.count; ++i) {
        const auto &r = t.intents.data[i];
        if (r.index != i || !nonzero(r.claim) || !nonzero(r.result) || equal(r.claim, r.result) ||
            equal(r.claim, t.operation) || equal(r.result, t.operation))
            return false;
        for (std::size_t j = 0; j < i; ++j) {
            const auto &other = t.intents.data[j];
            if (equal(r.claim, other.claim) || equal(r.claim, other.result) ||
                equal(r.result, other.claim) || equal(r.result, other.result))
                return false;
        }
    }
    return true;
}
struct Writer {
    std::uint8_t *p;
    std::size_t size, pos = 0;
    TransactionError error = TransactionError::Capacity;
    Writer(std::uint8_t *b, std::size_t n) : p(b), size(n) {}
    bool byte(std::uint8_t b) {
        if (pos == size)
            return false;
        p[pos++] = b;
        return true;
    }
    bool head(unsigned major, std::uint64_t n) {
        if (n < 24)
            return byte(static_cast<std::uint8_t>((major << 5) | n));
        const unsigned count = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
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
    bool raw(const std::uint8_t *b, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            if (!byte(b[i]))
                return false;
        return true;
    }
    bool shape(unsigned major, std::uint64_t n) {
        return head(major, n);
    }
    bool key(unsigned n) {
        return head(0, n);
    }
    template <std::size_t N> bool bytes(std::uint8_t (&v)[N]) {
        return head(2, N) && raw(v, N);
    }
    bool text() {
        return head(3, sizeof(Domain) - 1) &&
               raw(reinterpret_cast<const std::uint8_t *>(Domain), sizeof(Domain) - 1);
    }
    bool index(std::uint16_t &n) {
        return head(0, n);
    }
    bool app(rules::Span<std::uint8_t> &span, rules::Pool<std::uint8_t> &) {
        return head(2, span.count) && raw(span.data, span.count);
    }
    bool count(rules::Pool<IntentReservation> &p) {
        return head(4, p.count);
    }
};
struct Reader {
    const std::uint8_t *p;
    std::size_t size, pos = 0;
    TransactionError error = TransactionError::Canonical;
    Reader(const std::uint8_t *b, std::size_t n) : p(b), size(n) {}
    bool head(unsigned major, std::uint64_t &n) {
        if (pos == size)
            return false;
        const unsigned b = p[pos++], ai = b & 31;
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
            n = (n << 8) | p[pos++];
        return !((count == 1 && n < 24) || (count == 2 && n <= 255) || (count == 4 && n <= 65535) ||
                 (count == 8 && n <= UINT32_MAX));
    }
    bool shape(unsigned major, std::uint64_t n) {
        std::uint64_t actual = 0;
        return head(major, actual) && actual == n;
    }
    bool key(unsigned n) {
        return shape(0, n);
    }
    template <std::size_t N> bool bytes(std::uint8_t (&v)[N]) {
        if (!shape(2, N) || N > size - pos)
            return false;
        std::memcpy(v, p + pos, N);
        pos += N;
        return true;
    }
    bool text() {
        if (!shape(3, sizeof(Domain) - 1) || sizeof(Domain) - 1 > size - pos ||
            std::memcmp(p + pos, Domain, sizeof(Domain) - 1))
            return false;
        pos += sizeof(Domain) - 1;
        return true;
    }
    bool index(std::uint16_t &n) {
        std::uint64_t v = 0;
        if (!head(0, v) || v >= MaximumIntentReservations)
            return false;
        n = static_cast<std::uint16_t>(v);
        return true;
    }
    bool app(rules::Span<std::uint8_t> &span, rules::Pool<std::uint8_t> &owned) {
        std::uint64_t n = 0;
        if (!head(2, n) || !n || n > MaximumBytes || n > size - pos)
            return false;
        if (n > owned.capacity || !owned.data) {
            error = TransactionError::Capacity;
            return false;
        }
        std::memcpy(owned.data, p + pos, static_cast<std::size_t>(n));
        owned.count = static_cast<std::size_t>(n);
        pos += owned.count;
        span.data = owned.data;
        span.count = owned.count;
        return true;
    }
    bool count(rules::Pool<IntentReservation> &pool) {
        std::uint64_t n = 0;
        if (!head(4, n) || n > MaximumIntentReservations)
            return false;
        if (n > pool.capacity || (n && !pool.data)) {
            error = TransactionError::Capacity;
            return false;
        }
        pool.count = static_cast<std::size_t>(n);
        return true;
    }
};
template <class IO> bool code(IO &io, Transaction &t, rules::Pool<std::uint8_t> &owned) {
    if (!(io.shape(5, 6) && io.key(0) && io.text() && io.key(1) && io.bytes(t.operation.bytes) &&
          io.key(2) && io.bytes(t.request.bytes) && io.key(3) && io.bytes(t.catalog.bytes) &&
          io.key(4) && io.app(t.application, owned) && io.key(5) && io.count(t.intents)))
        return false;
    for (std::size_t i = 0; i < t.intents.count; ++i) {
        auto &r = t.intents.data[i];
        if (!(io.shape(4, 3) && io.index(r.index) && io.bytes(r.claim.bytes) &&
              io.bytes(r.result.bytes)))
            return false;
    }
    return true;
}
TransactionResult value(bool valid, TransactionError error, std::size_t offset = 0) {
    TransactionResult r;
    r.valid = valid;
    r.error = error;
    r.offset = offset;
    return r;
}
} // namespace
TransactionResult encodeTransaction(const Transaction &source,
                                    rules::Pool<std::uint8_t> &destination) {
    Range src[3], dst[2];
    if (destination.count > destination.capacity || !range(&source, 1, sizeof(source), src[0]) ||
        !range(source.application.data, source.application.count, 1, src[1]) ||
        !range(source.intents.data, source.intents.capacity, sizeof(IntentReservation), src[2]) ||
        !range(&destination, 1, sizeof(destination), dst[0]) ||
        !range(destination.data, destination.capacity, 1, dst[1]) || overlaps(dst[0], dst[1]))
        return value(false, TransactionError::Alias);
    for (auto s : src)
        for (auto d : dst)
            if (overlaps(s, d))
                return value(false, TransactionError::Alias);
    destination.count = 0;
    if (!sane(source))
        return value(false, TransactionError::Value);
    Transaction copy = source;
    rules::Pool<std::uint8_t> unused;
    Writer writer(destination.data,
                  destination.capacity < MaximumBytes ? destination.capacity : MaximumBytes);
    if (!code(writer, copy, unused))
        return value(false, writer.error, writer.pos);
    destination.count = writer.pos;
    return value(true, TransactionError::None, writer.pos);
}
TransactionResult decodeTransaction(const std::uint8_t *source, std::size_t count,
                                    Transaction &destination, rules::Pool<std::uint8_t> &owned) {
    Range r[5];
    if (destination.intents.count > destination.intents.capacity || owned.count > owned.capacity ||
        !range(source, count, 1, r[0]) || !range(&destination, 1, sizeof(destination), r[1]) ||
        !range(&owned, 1, sizeof(owned), r[2]) ||
        !range(destination.intents.data, destination.intents.capacity, sizeof(IntentReservation),
               r[3]) ||
        !range(owned.data, owned.capacity, 1, r[4]))
        return value(false, TransactionError::Alias);
    for (unsigned i = 0; i < 5; ++i)
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(r[i], r[j]))
                return value(false, TransactionError::Alias);
    const auto storage = destination.intents;
    destination = {};
    destination.intents = storage;
    destination.intents.count = 0;
    owned.count = 0;
    if (!count || count > MaximumBytes)
        return value(false, TransactionError::Value);
    Reader reader(source, count);
    if (!code(reader, destination, owned) || reader.pos != count || !sane(destination)) {
        destination = {};
        destination.intents = storage;
        destination.intents.count = 0;
        owned.count = 0;
        return value(false, reader.error, reader.pos);
    }
    return value(true, TransactionError::None, reader.pos);
}
} // namespace rules_progress
} // namespace devices
} // namespace larp
