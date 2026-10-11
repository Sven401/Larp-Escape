#include "CompanionIntegration/DiagnosticControl.h"
namespace larp {
namespace diagnostic {
namespace {
const std::uint64_t maxTime = 9007199254740991ULL;
}
bool Uuid::valid() const noexcept {
    return (bytes[6] & 0xf0) == 0x40 && (bytes[8] & 0xc0) == 0x80;
}
Control::Control(companion::IdentityAccess &access, IPort &port, const companion::Identity &device,
                 std::uint64_t bootId) noexcept
    : access_(access), port_(port), device_(device), bootId_(bootId) {}
Control::~Control() {
    close(Reason::Cancelled, lastNow_);
}
bool Control::current() const noexcept {
    companion::TrustedAuthority observed;
    return granted_ && access_.route(binding_, observed) && observed == authority_;
}
bool Control::clock(std::uint64_t now) noexcept {
    if (clockFault_)
        return false;
    if (now > maxTime || now < lastNow_) {
        clockFault_ = true;
        close(Reason::OwnerError, lastNow_);
        return false;
    }
    lastNow_ = now;
    return true;
}
bool Control::off() noexcept {
    if (!ledAttempted_)
        return true;
    ledAttempted_ = false;
    ++result_.renderCalls;
    result_.restoredOff = port_.restoreOff();
    if (!result_.restoredOff)
        cleanupFault_ = true;
    return result_.restoredOff;
}
void Control::finish(Outcome outcome, Reason reason, std::uint64_t now) noexcept {
    if (!pending_)
        return;
    if (!off()) {
        outcome = Outcome::Failed;
        reason = Reason::OwnerError;
    }
    result_.outcome = outcome;
    result_.reason = reason;
    result_.endedDeviceMs = now;
    replyBinding_ = binding_;
    pending_ = false;
    replyReady_ = true;
}
void Control::close(Reason reason, std::uint64_t now) noexcept {
    finish(reason == Reason::OwnerError ? Outcome::Failed : Outcome::Cancelled, reason, now);
    off();
    granted_ = false;
    // Unknown light state keeps the components fenced against game start. A
    // product recovery policy must prove safe off before releasing this fault.
    if (owned_ && !cleanupFault_) {
        port_.release();
        owned_ = false;
    }
}
Admission Control::authorize(const Uuid &run, const companion::BoundConnection &binding,
                             bool consent, std::uint32_t ttl, std::uint64_t now,
                             Grant &destination) noexcept {
    tick(now);
    if (clockFault_ || cleanupFault_)
        return Admission::OwnerError;
    companion::TrustedAuthority authority;
    if (!consent || !access_.route(binding, authority))
        return Admission::PermissionDenied;
    if (granted_ || replyReady_)
        return Admission::Busy;
    if (!run.valid() || !device_.valid() || !bootId_ || !ttl || ttl > 60000 || now > maxTime - ttl)
        return Admission::Invalid;
    for (std::size_t i = 0; i < used_; ++i)
        if (runs_[i] == run)
            return Admission::Consumed;
    if (used_ == Runs)
        return Admission::Capacity;
    Uuid id;
    bool activeHigh = true;
    if (!port_.acquire(id, activeHigh))
        return Admission::Busy;
    owned_ = true;
    bool valid = id.valid();
    for (std::size_t i = 0; i < used_; ++i)
        if (grants_[i] == id)
            valid = false;
    // Check again after the port call: fake reentry/changed authority cannot grant.
    companion::TrustedAuthority observed;
    if (!valid || !access_.route(binding, observed) || !(observed == authority)) {
        port_.release();
        owned_ = false;
        return valid ? Admission::PermissionDenied : Admission::OwnerError;
    }
    binding_ = binding;
    authority_ = authority;
    grant_.runId = run;
    grant_.grantId = id;
    grant_.issuedDeviceMs = now;
    grant_.expiresDeviceMs = now + ttl;
    runs_[used_] = run;
    grants_[used_++] = id;
    result_.activeHigh = activeHigh;
    lastStep_ = 0;
    granted_ = true;
    destination = grant_;
    return Admission::Accepted;
}
Admission Control::submit(const Request &request, const companion::BoundConnection &binding,
                          std::uint64_t now) noexcept {
    return submitAt(request, binding, now, now);
}
Admission Control::submitAt(const Request &request, const companion::BoundConnection &binding,
                            std::uint64_t receivedAt, std::uint64_t now) noexcept {
    tick(now);
    if (clockFault_ || cleanupFault_)
        return Admission::OwnerError;
    if (!current() || !(binding == binding_))
        return Admission::PermissionDenied;
    if (pending_ || replyReady_)
        return Admission::Busy;
    if (request.schemaVersion != 1 || !request.stepId || request.timeoutMs < 1000 ||
        request.timeoutMs > 10000 || !(request.runId == grant_.runId) ||
        !(request.grantId == grant_.grantId) || !(request.deviceId == device_) ||
        request.bootId != bootId_)
        return Admission::Invalid;
    if (request.stepId <= lastStep_)
        return Admission::Consumed;
    if (receivedAt < grant_.issuedDeviceMs || receivedAt > now ||
        now - receivedAt >= request.timeoutMs ||
        request.timeoutMs > grant_.expiresDeviceMs - receivedAt)
        return Admission::Expired;
    if (request.operation == Operation::WakeCapture) {
        if (request.channel != 1)
            return Admission::Invalid;
    } else if (request.operation == Operation::LedSingle) {
        if (request.index || request.rgb != std::array<std::uint8_t, 3>{{16, 0, 0}} ||
            request.holdMs != 500)
            return Admission::Invalid;
        if (request.timeoutMs - (now - receivedAt) <= request.holdMs)
            return Admission::Expired;
    } else
        return Admission::Invalid;
    const bool polarity = result_.activeHigh;
    result_ = {};
    result_.activeHigh = polarity;
    result_.request = request;
    result_.startedDeviceMs = now;
    deadline_ = receivedAt + request.timeoutMs;
    lastStep_ = request.stepId;
    pending_ = true;
    if (request.operation == Operation::LedSingle) {
        ledAttempted_ = true;
        if (!port_.armLedOffTimer(request.holdMs)) {
            cleanupFault_ = true;
            finish(Outcome::Failed, Reason::OwnerError, now);
            return Admission::Accepted;
        }
        result_.renderCalls = 1;
        if (!port_.render(0, request.rgb))
            finish(Outcome::Failed, Reason::OwnerError, now);
    }
    return Admission::Accepted;
}
void Control::tick(std::uint64_t now) noexcept {
    if (!clock(now))
        return;
    if (!granted_)
        return;
    if (!current()) {
        close(Reason::PermissionDenied, now);
        return;
    }
    if (now >= grant_.expiresDeviceMs) {
        close(Reason::Expired, now);
        return;
    }
    if (!pending_)
        return;
    if (now >= deadline_) {
        finish(Outcome::Rejected, Reason::Expired, now);
        return;
    }
    if (result_.request.operation == Operation::LedSingle && now >= result_.startedDeviceMs + 500) {
        if (!port_.timedOffConfirmed()) {
            cleanupFault_ = true;
            finish(Outcome::Failed, Reason::OwnerError, now);
        } else {
            finish(Outcome::Completed, Reason::None, now);
        }
    }
}
void Control::edge(std::uint8_t channel, bool rawHigh, std::uint64_t now) noexcept {
    tick(now);
    if (!pending_ || result_.request.operation != Operation::WakeCapture)
        return;
    if (channel < 1 || channel > 4) {
        finish(Outcome::Failed, Reason::OwnerError, now);
        return;
    }
    if (result_.edgeCount == result_.edges.size()) {
        result_.overflow = true;
        return;
    }
    auto &edge = result_.edges[result_.edgeCount++];
    edge.channel = channel;
    edge.rawHigh = rawHigh;
    edge.active = rawHigh == result_.activeHigh;
    edge.deviceMs = now;
}
void Control::finishWake(std::uint64_t now) noexcept {
    tick(now);
    if (pending_ && result_.request.operation == Operation::WakeCapture)
        finish(Outcome::Completed, Reason::None, now);
}
void Control::cancel(const companion::BoundConnection &binding, std::uint64_t now) noexcept {
    tick(now);
    if (granted_ && binding == binding_)
        close(Reason::Cancelled, now);
}
void Control::disconnect(const companion::BoundConnection &binding, std::uint64_t now) noexcept {
    // Lifecycle owner calls this for the original generation, even after access closes.
    if (clock(now) && granted_ && binding == binding_)
        close(Reason::Cancelled, now);
}
void Control::retire(std::uint64_t now) noexcept {
    if (clock(now))
        close(Reason::Cancelled, now);
}
bool Control::takeReply(const companion::BoundConnection &binding, Reply &destination) noexcept {
    if (!replyReady_ || !(binding == replyBinding_))
        return false;
    destination = result_;
    replyReady_ = false;
    return true;
}
} // namespace diagnostic
} // namespace larp
