#include "CompanionIntegration/DiagnosticStepTransport.h"
#include <algorithm>
#include <cstring>
namespace larp {
namespace diagnostic {
const std::size_t StepLimits::Payload;
namespace {
const std::uint64_t maxTime = 9007199254740991ULL;
std::uint64_t get(const std::uint8_t *p, unsigned n) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i)
        value |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return value;
}
void put(std::uint8_t *p, std::uint64_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
bool route(companion::IdentityAccess &access, const companion::BoundConnection &binding,
           const companion::TrustedAuthority &expected) {
    companion::TrustedAuthority observed;
    return expected.epoch && expected.principal && expected.gameMaster &&
           access.route(binding, observed) && observed == expected;
}
bool valid(const Request &r) {
    if (r.schemaVersion != 1 || !r.deviceId.valid() || !r.bootId || !r.runId.valid() ||
        !r.grantId.valid() || !r.stepId || r.timeoutMs < 1000 || r.timeoutMs > 10000)
        return false;
    return r.operation == Operation::WakeCapture
               ? r.channel == 1
               : r.operation == Operation::LedSingle && r.index == 0 &&
                     r.rgb == std::array<std::uint8_t, 3>{{16, 0, 0}} && r.holdMs == 500;
}
bool same(const Request &a, const Request &b) {
    std::array<std::uint8_t, StepLimits::RequestBytes> x = {}, y = {};
    return encodeStep(1, a, x.data(), x.size()) && encodeStep(1, b, y.data(), y.size()) && x == y;
}
bool replyValid(const Reply &r) {
    if (!valid(r.request) || r.startedDeviceMs > r.endedDeviceMs || r.endedDeviceMs > maxTime ||
        r.endedDeviceMs - r.startedDeviceMs >= r.request.timeoutMs ||
        static_cast<unsigned>(r.outcome) > 3 || static_cast<unsigned>(r.reason) > 7 ||
        r.edgeCount > 64 || r.renderCalls > 2)
        return false;
    if ((r.outcome == Outcome::Completed) != (r.reason == Reason::None))
        return false;
    if (r.request.operation == Operation::LedSingle) {
        return !r.edgeCount && !r.overflow &&
               (r.outcome != Outcome::Completed ||
                (r.renderCalls == 2 && r.restoredOff &&
                 r.endedDeviceMs - r.startedDeviceMs >= r.request.holdMs));
    }
    if (r.renderCalls || r.restoredOff)
        return false;
    std::uint64_t previous = r.startedDeviceMs;
    for (std::size_t i = 0; i < r.edgeCount; ++i) {
        const auto &e = r.edges[i];
        if (e.channel < 1 || e.channel > 4 || e.deviceMs < previous ||
            e.deviceMs > r.endedDeviceMs || e.active != (e.rawHigh == r.activeHigh))
            return false;
        previous = e.deviceMs;
    }
    return true;
}
} // namespace
bool encodeStep(std::uint32_t message, const Request &r, std::uint8_t *out,
                std::size_t capacity) noexcept {
    if (!message || !valid(r) || !out || capacity < StepLimits::RequestBytes)
        return false;
    std::array<std::uint8_t, StepLimits::RequestBytes> p = {};
    p[0] = 'D';
    p[1] = 'G';
    p[2] = 1;
    p[3] = 4;
    put(p.data() + 4, message, 4);
    std::memcpy(p.data() + 8, r.deviceId.bytes.data(), 16);
    put(p.data() + 24, r.bootId, 8);
    std::memcpy(p.data() + 32, r.runId.bytes.data(), 16);
    std::memcpy(p.data() + 48, r.grantId.bytes.data(), 16);
    put(p.data() + 64, r.stepId, 4);
    put(p.data() + 68, r.timeoutMs, 4);
    p[72] = static_cast<std::uint8_t>(r.operation);
    if (r.operation == Operation::WakeCapture)
        p[73] = r.channel;
    else {
        p[74] = r.index;
        std::memcpy(p.data() + 75, r.rgb.data(), 3);
        put(p.data() + 78, r.holdMs, 2);
    }
    std::memcpy(out, p.data(), p.size());
    return true;
}
bool decodeStep(const std::uint8_t *p, std::size_t count, std::uint32_t &message,
                Request &out) noexcept {
    if (!p || count != StepLimits::RequestBytes || p[0] != 'D' || p[1] != 'G' || p[2] != 1 ||
        p[3] != 4 || p[72] > 1)
        return false;
    Request r;
    const auto id = static_cast<std::uint32_t>(get(p + 4, 4));
    std::memcpy(r.deviceId.bytes.data(), p + 8, 16);
    r.bootId = get(p + 24, 8);
    std::memcpy(r.runId.bytes.data(), p + 32, 16);
    std::memcpy(r.grantId.bytes.data(), p + 48, 16);
    r.stepId = static_cast<std::uint32_t>(get(p + 64, 4));
    r.timeoutMs = static_cast<std::uint32_t>(get(p + 68, 4));
    r.operation = static_cast<Operation>(p[72]);
    if (r.operation == Operation::WakeCapture) {
        r.channel = p[73];
        for (unsigned i = 74; i < 80; ++i)
            if (p[i])
                return false;
    } else {
        if (p[73])
            return false;
        r.index = p[74];
        std::memcpy(r.rgb.data(), p + 75, 3);
        r.holdMs = static_cast<std::uint16_t>(get(p + 78, 2));
    }
    if (!id || !valid(r))
        return false;
    message = id;
    out = r;
    return true;
}
bool encodeStepReply(std::uint32_t message, const Reply &r, std::uint8_t *out, std::size_t capacity,
                     std::size_t &written) noexcept {
    written = 0;
    const std::size_t size = StepLimits::ReplyHeader + r.edgeCount * StepLimits::EdgeBytes;
    if (!out || !replyValid(r) || capacity < size || !encodeStep(message, r.request, out, capacity))
        return false;
    put(out + 80, r.startedDeviceMs, 8);
    put(out + 88, r.endedDeviceMs, 8);
    out[96] = static_cast<std::uint8_t>(r.outcome);
    out[97] = static_cast<std::uint8_t>(r.reason);
    out[98] = r.activeHigh;
    out[99] = r.restoredOff;
    out[100] = r.renderCalls;
    out[101] = r.overflow;
    out[102] = static_cast<std::uint8_t>(r.edgeCount);
    out[103] = 1; // Synthetic; voltage unavailable.
    for (std::size_t i = 0; i < r.edgeCount; ++i) {
        auto *p = out + StepLimits::ReplyHeader + i * StepLimits::EdgeBytes;
        put(p, r.edges[i].deviceMs, 8);
        p[8] = r.edges[i].channel;
        p[9] = static_cast<std::uint8_t>(r.edges[i].rawHigh | (r.edges[i].active << 1));
    }
    written = size;
    return true;
}
bool decodeStepReply(const std::uint8_t *p, std::size_t count, std::uint32_t &message,
                     Reply &out) noexcept {
    if (!p || count < StepLimits::ReplyHeader || count > StepLimits::ReplyBytes || p[102] > 64 ||
        count != StepLimits::ReplyHeader + p[102] * StepLimits::EdgeBytes || p[98] > 1 ||
        p[99] > 1 || p[101] > 1 || p[103] != 1)
        return false;
    Reply r;
    std::uint32_t id = 0;
    if (!decodeStep(p, StepLimits::RequestBytes, id, r.request))
        return false;
    r.startedDeviceMs = get(p + 80, 8);
    r.endedDeviceMs = get(p + 88, 8);
    r.outcome = static_cast<Outcome>(p[96]);
    r.reason = static_cast<Reason>(p[97]);
    r.activeHigh = p[98];
    r.restoredOff = p[99];
    r.renderCalls = p[100];
    r.overflow = p[101];
    r.edgeCount = p[102];
    for (std::size_t i = 0; i < r.edgeCount; ++i) {
        const auto *e = p + StepLimits::ReplyHeader + i * StepLimits::EdgeBytes;
        if (e[9] > 3)
            return false;
        r.edges[i].deviceMs = get(e, 8);
        r.edges[i].channel = e[8];
        r.edges[i].rawHigh = e[9] & 1;
        r.edges[i].active = e[9] & 2;
    }
    if (!replyValid(r))
        return false;
    message = id;
    out = r;
    return true;
}
void StepTransport::discard() noexcept {
    Reply ignored;
    control_.takeReply(binding_, ignored);
    active_ = false;
    total_ = offset_ = 0;
}
Admission StepTransport::receive(const std::uint8_t *p, std::size_t n,
                                 const companion::BoundConnection &binding,
                                 const companion::TrustedAuthority &authority,
                                 std::uint64_t receivedAt, std::uint64_t now) noexcept {
    tick(now);
    if (active_)
        return Admission::Busy;
    if (!route(access_, binding, authority))
        return Admission::PermissionDenied;
    Request request;
    std::uint32_t message = 0;
    if (!decodeStep(p, n, message, request))
        return Admission::Invalid;
    if (now > maxTime || receivedAt > now || now - receivedAt >= request.timeoutMs)
        return Admission::Expired;
    const auto result = control_.submitAt(request, binding, receivedAt, now);
    if (result != Admission::Accepted)
        return result;
    binding_ = binding;
    authority_ = authority;
    request_ = request;
    message_ = message;
    deadline_ = receivedAt + request.timeoutMs;
    active_ = true;
    total_ = offset_ = 0;
    tick(now);
    return active_ ? Admission::Accepted : Admission::PermissionDenied;
}
void StepTransport::tick(std::uint64_t now) noexcept {
    control_.tick(now);
    const bool clockOk = now >= lastNow_ && now <= maxTime;
    lastNow_ = now;
    if (!active_)
        return;
    if (!clockOk || now >= deadline_ || !route(access_, binding_, authority_)) {
        discard();
        return;
    }
    if (!total_) {
        Reply reply;
        if (control_.takeReply(binding_, reply) &&
            (!same(request_, reply.request) ||
             !encodeStepReply(message_, reply, bytes_.data(), bytes_.size(), total_))) {
            control_.retire(now);
            discard();
        }
    }
    // A completed result buffered before cancel may not outlive its exact grant,
    // even if another run has meanwhile acquired a new grant on the same link.
    if (total_ && bytes_[96] == static_cast<std::uint8_t>(Outcome::Completed) &&
        !control_.matchesGrant(request_, binding_))
        discard();
}
void StepTransport::disconnect(const companion::BoundConnection &binding,
                               std::uint64_t now) noexcept {
    control_.disconnect(binding, now);
    if (active_ && binding == binding_)
        discard();
    tick(now);
}
bool StepTransport::nextFrame(const companion::BoundConnection &binding, std::uint64_t now,
                              std::uint8_t *out, std::size_t capacity,
                              std::size_t &written) noexcept {
    written = 0;
    tick(now);
    if (!active_ || !total_ || !(binding == binding_) || !out ||
        capacity <= StepLimits::FrameHeader)
        return false;
    const auto count = std::min(total_ - offset_,
                                std::min(capacity - StepLimits::FrameHeader, StepLimits::Payload));
    out[0] = 'D';
    out[1] = 'G';
    out[2] = 1;
    out[3] = 0x84;
    put(out + 4, message_, 4);
    put(out + 8, total_, 2);
    put(out + 10, offset_, 2);
    std::memcpy(out + StepLimits::FrameHeader, bytes_.data() + offset_, count);
    written = StepLimits::FrameHeader + count;
    offset_ += count;
    if (offset_ == total_)
        active_ = false;
    return true;
}
bool ReplyAssembly::begin(std::uint32_t message, const Request &request,
                          const companion::BoundConnection &binding,
                          const companion::TrustedAuthority &authority, std::uint64_t now,
                          std::uint64_t deadline) noexcept {
    retire();
    if (!message || !valid(request) || now >= deadline || deadline > maxTime ||
        deadline - now > request.timeoutMs || !route(access_, binding, authority))
        return false;
    message_ = message;
    request_ = request;
    binding_ = binding;
    authority_ = authority;
    lastNow_ = now;
    deadline_ = deadline;
    active_ = true;
    return true;
}
void ReplyAssembly::retire() noexcept {
    active_ = complete_ = false;
    total_ = received_ = 0;
}
bool ReplyAssembly::live(std::uint64_t now) noexcept {
    if (!active_ || now < lastNow_ || now >= deadline_ || !route(access_, binding_, authority_)) {
        retire();
        return false;
    }
    lastNow_ = now;
    return true;
}
AssemblyResult ReplyAssembly::receive(const std::uint8_t *p, std::size_t n,
                                      const companion::BoundConnection &binding,
                                      const companion::TrustedAuthority &authority,
                                      std::uint64_t now) noexcept {
    if (!active_ || complete_)
        return AssemblyResult::Closed;
    if (!live(now))
        return AssemblyResult::Expired;
    if (!(binding == binding_) || !(authority == authority_) || !p ||
        n <= StepLimits::FrameHeader || n > StepLimits::FrameBytes || p[0] != 'D' || p[1] != 'G' ||
        p[2] != 1 || p[3] != 0x84 || get(p + 4, 4) != message_) {
        retire();
        return AssemblyResult::Rejected;
    }
    const auto total = static_cast<std::size_t>(get(p + 8, 2)),
               offset = static_cast<std::size_t>(get(p + 10, 2)),
               count = n - StepLimits::FrameHeader;
    if (total < StepLimits::ReplyHeader || total > StepLimits::ReplyBytes ||
        (total - StepLimits::ReplyHeader) % StepLimits::EdgeBytes || offset >= total ||
        count > total - offset || (total_ && total_ != total) || offset > received_) {
        retire();
        return AssemblyResult::Rejected;
    }
    if (offset < received_) {
        if (count <= received_ - offset &&
            std::memcmp(bytes_.data() + offset, p + StepLimits::FrameHeader, count) == 0)
            return AssemblyResult::Duplicate;
        retire();
        return AssemblyResult::Rejected;
    }
    total_ = total;
    std::memcpy(bytes_.data() + offset, p + StepLimits::FrameHeader, count);
    received_ += count;
    if (received_ < total_)
        return AssemblyResult::Receiving;
    Reply reply;
    std::uint32_t message = 0;
    if (!decodeStepReply(bytes_.data(), total_, message, reply) || message != message_ ||
        !same(request_, reply.request)) {
        retire();
        return AssemblyResult::Rejected;
    }
    complete_ = true;
    return AssemblyResult::Complete;
}
bool ReplyAssembly::take(std::uint64_t now, Reply &out) noexcept {
    if (!live(now) || !complete_)
        return false;
    std::uint32_t message = 0;
    const bool success = decodeStepReply(bytes_.data(), total_, message, out);
    retire();
    return success;
}
} // namespace diagnostic
} // namespace larp
