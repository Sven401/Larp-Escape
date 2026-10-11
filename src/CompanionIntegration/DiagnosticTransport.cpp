#include "CompanionIntegration/DiagnosticTransport.h"
#include <cstring>
namespace larp {
namespace diagnostic {
namespace {
std::uint64_t read(const std::uint8_t *p, unsigned n) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i)
        value |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return value;
}
void write(std::uint8_t *p, std::uint64_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
bool zero(const Uuid &id) {
    return id.bytes == Uuid{}.bytes;
}
bool timely(std::uint64_t received, std::uint64_t now) {
    return now <= 9007199254740991ULL && received <= now &&
           now - received < TransportLimits::QueueMs;
}
} // namespace
bool decodeControlRequest(const std::uint8_t *p, std::size_t n, ControlRequest &out) noexcept {
    if (!p || n != TransportLimits::RequestBytes || p[0] != 'D' || p[1] != 'G' || p[2] != 1 ||
        p[3] < 1 || p[3] > 3 || p[68] > 1)
        return false;
    ControlRequest r;
    r.command = static_cast<ControlCommand>(p[3]);
    r.message = static_cast<std::uint32_t>(read(p + 4, 4));
    std::memcpy(r.device.bytes.data(), p + 8, 16);
    r.boot = read(p + 24, 8);
    std::memcpy(r.run.bytes.data(), p + 32, 16);
    std::memcpy(r.grant.bytes.data(), p + 48, 16);
    r.ttlMs = static_cast<std::uint32_t>(read(p + 64, 4));
    r.consent = p[68] != 0;
    if (!r.message || !r.device.valid() || !r.boot)
        return false;
    if (r.command == ControlCommand::Capabilities) {
        if (!zero(r.run) || !zero(r.grant) || r.ttlMs || r.consent)
            return false;
    } else if (r.command == ControlCommand::Grant) {
        if (!r.run.valid() || !zero(r.grant) || !r.ttlMs || r.ttlMs > 60000 || !r.consent)
            return false;
    } else if (!r.run.valid() || !r.grant.valid() || r.ttlMs || r.consent)
        return false;
    out = r;
    return true;
}
bool ControlTransport::current(const companion::BoundConnection &binding,
                               const companion::TrustedAuthority &expected) const noexcept {
    companion::TrustedAuthority observed;
    companion::Identity actual;
    return expected.epoch && expected.principal && expected.gameMaster &&
           access_.deviceIdentity(actual) && actual == device_ &&
           access_.route(binding, observed) && observed == expected;
}
void ControlTransport::tick(std::uint64_t now) noexcept {
    control_.tick(now);
    if (!control_.granted())
        grantKnown_ = false;
    if (ready_ && (!timely(replyAt_, now) || !current(binding_, authority_) ||
                   (request_.command == ControlCommand::Grant && result_ == Admission::Accepted &&
                    (!grantKnown_ || now >= responseGrant_.expiresDeviceMs))))
        ready_ = false;
}
TransportResult ControlTransport::receive(const std::uint8_t *bytes, std::size_t count,
                                          const companion::BoundConnection &binding,
                                          const companion::TrustedAuthority &captured,
                                          std::uint64_t receivedAt, std::uint64_t now) noexcept {
    tick(now);
    ControlRequest request;
    if (!decodeControlRequest(bytes, count, request) || !(request.device == device_) ||
        request.boot != boot_)
        return TransportResult::Invalid;
    if (!current(binding, captured))
        return TransportResult::Unauthorized;
    if (!timely(receivedAt, now))
        return TransportResult::Expired;
    // A valid original cancel supersedes a queued grant reply, so backpressure
    // cannot keep a grant alive. Wrong run/grant never cancels another session.
    const bool cancel = request.command == ControlCommand::Cancel && grantKnown_ &&
                        binding == grantBinding_ && request.run == grant_.runId &&
                        request.grant == grant_.grantId;
    if (ready_ && !cancel)
        return TransportResult::Busy;
    Admission result = Admission::Accepted;
    Grant response;
    if (request.command == ControlCommand::Grant) {
        result =
            control_.authorize(request.run, binding, request.consent, request.ttlMs, now, response);
        if (result == Admission::Accepted) {
            grant_ = response;
            grantBinding_ = binding;
            grantKnown_ = true;
        }
    } else if (request.command == ControlCommand::Cancel) {
        result = cancel ? Admission::Accepted : Admission::Invalid;
        if (cancel) {
            control_.cancel(binding, now);
            grantKnown_ = false;
        }
    }
    // No reply can be adopted by replacement authority after a port callback.
    if (!current(binding, captured)) {
        tick(now);
        ready_ = false;
        return TransportResult::Unauthorized;
    }
    request_ = request;
    responseGrant_ = response;
    binding_ = binding;
    authority_ = captured;
    result_ = result;
    replyAt_ = receivedAt;
    ready_ = true;
    return TransportResult::Ready;
}
bool ControlTransport::takeReply(const companion::BoundConnection &binding, std::uint64_t now,
                                 std::uint8_t *out, std::size_t capacity,
                                 std::size_t &written) noexcept {
    written = 0;
    tick(now);
    if (!ready_ || !(binding == binding_) || !out || capacity < TransportLimits::ReplyBytes)
        return false;
    std::array<std::uint8_t, TransportLimits::ReplyBytes> p = {};
    p[0] = 'D';
    p[1] = 'G';
    p[2] = 1;
    p[3] = static_cast<std::uint8_t>(request_.command) | 0x80;
    write(p.data() + 4, request_.message, 4);
    std::memcpy(p.data() + 8, device_.bytes.data(), 16);
    write(p.data() + 24, boot_, 8);
    std::memcpy(p.data() + 32, request_.run.bytes.data(), 16);
    const Uuid id =
        request_.command == ControlCommand::Grant ? responseGrant_.grantId : request_.grant;
    std::memcpy(p.data() + 48, id.bytes.data(), 16);
    p[64] = static_cast<std::uint8_t>(result_);
    p[65] = 3; // Synthetic owner, control-only. NEVER a physical hardware capability.
    p[66] = 1; // Logical diagnostic schema.
    write(p.data() + 68, responseGrant_.issuedDeviceMs, 8);
    write(p.data() + 76, responseGrant_.expiresDeviceMs, 8);
    write(p.data() + 84, 60000, 4);
    std::memcpy(out, p.data(), p.size());
    written = p.size();
    ready_ = false;
    return true;
}
void ControlTransport::disconnect(const companion::BoundConnection &binding,
                                  std::uint64_t now) noexcept {
    control_.disconnect(binding, now);
    if (binding == binding_)
        ready_ = false;
    tick(now);
}
} // namespace diagnostic
} // namespace larp
