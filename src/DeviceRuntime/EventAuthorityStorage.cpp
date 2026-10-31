#include "DeviceRuntime/EventAuthorityStorage.h"
#include <cstring>
#include <limits>
namespace larp {
namespace devices {
namespace event_authority {
namespace {
constexpr std::size_t EvidenceOffset = 148, CrcOffset = 2196;
struct Operation {
    bool &busy;
    explicit Operation(bool &v) : busy(v) {
        busy = true;
    }
    ~Operation() {
        busy = false;
    }
};
bool zero(const std::uint8_t *p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (p[i])
            return false;
    return true;
}
bool same(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool same(const Digest &a, const Digest &b) {
    return !std::memcmp(a.bytes, b.bytes, 32);
}
void put(std::uint8_t *p, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint64_t get(const std::uint8_t *p, unsigned count) {
    std::uint64_t v = 0;
    for (unsigned i = 0; i < count; ++i)
        v |= std::uint64_t(p[i]) << (8 * i);
    return v;
}
std::uint32_t crc(const Record &r) {
    std::uint32_t value = UINT32_MAX;
    for (std::size_t i = 0; i < CrcOffset; ++i) {
        value ^= r.bytes[i];
        for (unsigned j = 0; j < 8; ++j)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
    }
    return value ^ UINT32_MAX;
}
bool shape(const Snapshot &s, std::size_t count) {
    if (zero(s.device.bytes, 16) || !s.generation || count > MaximumEvidenceBytes)
        return false;
    if (s.phase == Phase::Closed)
        return !count && zero(s.event.bytes, 16) && !s.registryGeneration &&
               zero(s.registryDigest.bytes, 32) && zero(s.originalAuthorizationDigest.bytes, 32);
    return s.phase == Phase::Bound && !zero(s.event.bytes, 16) && s.registryGeneration && count;
}
void encode(const Snapshot &s, const std::uint8_t *evidence, std::size_t count, Record &r) {
    r.bytes.fill(0);
    std::memcpy(r.bytes.data(), "LEVTA001", 8);
    r.bytes[8] = static_cast<std::uint8_t>(s.phase);
    put(r.bytes.data() + 16, s.generation, 8);
    std::memcpy(r.bytes.data() + 24, s.device.bytes, 16);
    std::memcpy(r.bytes.data() + 40, s.event.bytes, 16);
    put(r.bytes.data() + 56, s.registryGeneration, 4);
    std::memcpy(r.bytes.data() + 60, s.registryDigest.bytes, 32);
    std::memcpy(r.bytes.data() + 92, s.originalAuthorizationDigest.bytes, 32);
    put(r.bytes.data() + 124, count, 2);
    if (count)
        std::memcpy(r.bytes.data() + EvidenceOffset, evidence, count);
    put(r.bytes.data() + CrcOffset, crc(r), 4);
}
bool decode(const Record &r, Snapshot &s, std::uint16_t &count) {
    const auto *p = r.bytes.data();
    if (std::memcmp(p, "LEVTA001", 8) || !zero(p + 9, 7) || !zero(p + 126, EvidenceOffset - 126) ||
        get(p + CrcOffset, 4) != crc(r))
        return false;
    Snapshot next;
    next.phase = static_cast<Phase>(p[8]);
    next.generation = get(p + 16, 8);
    std::memcpy(next.device.bytes, p + 24, 16);
    std::memcpy(next.event.bytes, p + 40, 16);
    next.registryGeneration = static_cast<std::uint32_t>(get(p + 56, 4));
    std::memcpy(next.registryDigest.bytes, p + 60, 32);
    std::memcpy(next.originalAuthorizationDigest.bytes, p + 92, 32);
    const auto length = static_cast<std::uint16_t>(get(p + 124, 2));
    if (!shape(next, length) || !zero(p + EvidenceOffset + length, MaximumEvidenceBytes - length))
        return false;
    s = next;
    count = length;
    return true;
}
Result value(Outcome outcome, Error error = Error::None) {
    Result r;
    r.outcome = outcome;
    r.error = error;
    return r;
}
} // namespace
Result Storage::fail(Error error, bool unknown) {
    ready_ = false;
    failed_ = true;
    store_.fence();
    return value(unknown ? Outcome::Unknown : Outcome::Rejected, error);
}
bool Storage::digest(const std::uint8_t *p, std::size_t n, Digest &out) {
    out = {};
    bool ok = hash_.abort();
    if (ok)
        ok = hash_.begin();
    if (ok)
        ok = hash_.update(p, n);
    std::size_t written = 0;
    if (ok)
        ok = hash_.finish(out.bytes, written) && written == 32;
    const bool closed = hash_.abort();
    if (!ok || !closed) {
        out = {};
        return false;
    }
    return true;
}
Result Storage::load(const Identity &requestedDevice) {
    const Identity device = requestedDevice;
    if (operating_)
        return value(Outcome::Rejected, Error::Busy);
    Operation operation(operating_);
    if (failed_ || store_.fenced())
        return value(Outcome::Rejected, Error::Storage);
    ready_ = false;
    present_ = false;
    evidenceBytes_ = 0;
    state_ = {};
    state_.device = device;
    if (zero(device.bytes, 16))
        return fail(Error::Context);
    const auto read = store_.read(work_.current);
    if (read == Read::Missing)
        return value(Outcome::Missing);
    if (read != Read::Present)
        return fail(Error::Storage);
    Snapshot selected;
    std::uint16_t count = 0;
    if (!decode(work_.current, selected, count))
        return fail(Error::Canonical);
    if (!same(selected.device, device))
        return fail(Error::Context);
    present_ = true;
    state_ = selected;
    evidenceBytes_ = count;
    if (selected.phase == Phase::Closed)
        return value(Outcome::Closed);
    Digest actual;
    if (!digest(work_.current.bytes.data() + EvidenceOffset, count, actual))
        return fail(Error::Crypto);
    if (!same(actual, selected.originalAuthorizationDigest))
        return fail(Error::Canonical);
    const auto authorized =
        proof_.verify(selected, work_.current.bytes.data() + EvidenceOffset, count);
    if (authorized == Proof::Unavailable)
        return value(Outcome::Closed, Error::Unavailable);
    if (authorized != Proof::VerifiedOriginal)
        return fail(Error::Authorization);
    // A proof callback cannot silently switch the stored setup/event record.
    if (store_.read(work_.readback) != Read::Present || work_.readback.bytes != work_.current.bytes)
        return fail(Error::Conflict);
    if (store_.fenced())
        return fail(Error::Storage);
    ready_ = true;
    return value(Outcome::Ready);
}
bool Storage::storedCurrent() {
    if (store_.fenced())
        return false;
    const auto read = store_.read(work_.readback);
    if ((present_ && (read != Read::Present || work_.readback.bytes != work_.current.bytes)) ||
        (!present_ && read != Read::Missing)) {
        fail(read == Read::Failure ? Error::Storage : Error::Conflict);
        return false;
    }
    return !store_.fenced();
}
Result Storage::close(std::uint64_t expected) {
    if (operating_)
        return value(Outcome::Rejected, Error::Busy);
    Operation operation(operating_);
    if (failed_ || store_.fenced() || zero(state_.device.bytes, 16))
        return value(Outcome::Rejected, Error::Storage);
    if (expected != state_.generation)
        return value(Outcome::Rejected, Error::Conflict);
    if (present_ && state_.phase == Phase::Closed) {
        if (!storedCurrent())
            return value(Outcome::Rejected, Error::Conflict);
        return value(Outcome::Closed);
    }
    if (expected == std::numeric_limits<std::uint64_t>::max())
        return fail(Error::Overflow);
    ready_ = false;
    Snapshot closed;
    closed.device = state_.device;
    closed.generation = expected + 1;
    encode(closed, nullptr, 0, work_.next);
    const auto written = store_.replace(present_ ? &work_.current : nullptr, work_.next);
    if (written != Write::Confirmed)
        return fail(written == Write::Conflict ? Error::Conflict : Error::Storage,
                    written == Write::Uncertain);
    if (store_.read(work_.readback) != Read::Present || work_.readback.bytes != work_.next.bytes)
        return fail(Error::Storage, true);
    work_.current = work_.readback;
    state_ = closed;
    present_ = true;
    evidenceBytes_ = 0;
    return value(Outcome::Closed);
}
Result Storage::install(std::uint64_t expected, const Snapshot &requested,
                        const std::uint8_t *evidence, std::size_t count) {
    const Snapshot next = requested;
    if (operating_)
        return value(Outcome::Rejected, Error::Busy);
    Operation operation(operating_);
    if (failed_ || store_.fenced() || !present_ || state_.phase != Phase::Closed ||
        expected != state_.generation || expected == std::numeric_limits<std::uint64_t>::max() ||
        next.generation != expected + 1 || !same(next.device, state_.device) ||
        next.phase != Phase::Bound || !evidence || !shape(next, count))
        return value(Outcome::Rejected, Error::Context);
    ready_ = false;
    // Freeze complete evidence before hash/proof/storage callbacks. The pending
    // record itself is private and cannot authorize any manual input.
    encode(next, evidence, count, work_.next);
    Digest actual;
    if (!digest(work_.next.bytes.data() + EvidenceOffset, count, actual))
        return fail(Error::Crypto);
    if (!same(actual, next.originalAuthorizationDigest))
        return fail(Error::Canonical);
    const auto authorization = proof_.verify(next, work_.next.bytes.data() + EvidenceOffset, count);
    if (authorization != Proof::VerifiedOriginal)
        return value(Outcome::Closed, authorization == Proof::Unavailable ? Error::Unavailable
                                                                          : Error::Authorization);
    const auto written = store_.replace(&work_.current, work_.next);
    if (written != Write::Confirmed)
        return fail(written == Write::Conflict ? Error::Conflict : Error::Storage,
                    written == Write::Uncertain);
    if (store_.read(work_.readback) != Read::Present || work_.readback.bytes != work_.next.bytes)
        return fail(Error::Storage, true);
    work_.current = work_.readback;
    state_ = next;
    evidenceBytes_ = static_cast<std::uint16_t>(count);
    ready_ = true;
    return value(Outcome::Ready);
}
bool Storage::snapshot(Snapshot &out) {
    if (operating_)
        return false;
    Operation operation(operating_);
    if (failed_ || store_.fenced() || (!ready_ && state_.phase != Phase::Closed) ||
        zero(state_.device.bytes, 16))
        return false;
    if (!storedCurrent())
        return false;
    out = state_;
    return true;
}
bool Storage::evidence(Pool<std::uint8_t> &destination) {
    if (operating_)
        return false;
    Operation operation(operating_);
    if (failed_ || store_.fenced() || !ready_ || destination.count > destination.capacity ||
        !destination.data || destination.capacity < evidenceBytes_)
        return false;
    const auto first = reinterpret_cast<std::uintptr_t>(destination.data),
               begin = reinterpret_cast<std::uintptr_t>(&work_);
    if (destination.capacity > std::numeric_limits<std::uintptr_t>::max() - first ||
        (first < begin + sizeof(work_) && begin < first + destination.capacity))
        return false;
    if (!storedCurrent())
        return false;
    destination.count = 0;
    std::memcpy(destination.data, work_.current.bytes.data() + EvidenceOffset, evidenceBytes_);
    destination.count = evidenceBytes_;
    return true;
}
} // namespace event_authority
} // namespace devices
} // namespace larp
