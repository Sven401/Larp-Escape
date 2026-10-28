#include "Esp32GattsSource.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
constexpr std::uint32_t OutputAgeMilliseconds = 1000;
constexpr std::size_t OutputBytes = 20;
struct OutputReadGuard {
    bool &busy;
    explicit OutputReadGuard(bool &value) : busy(value) {
        busy = true;
    }
    ~OutputReadGuard() {
        busy = false;
    }
};
} // namespace
bool Esp32GattsSource::configureOutputNow(std::uint16_t attribute) noexcept {
    if (!sourceTask() || !owner_ || sawConnection_ || !grantConfigured_ || !authIngress_ ||
        !resultAttribute_ || outputAttribute_ || attribute <= resultAttribute_ ||
        attribute == command_ || attribute == profile_ || attribute == challengeAttribute_ ||
        attribute == issueAttribute_ || attribute == proofAttribute_)
        return false;
    portENTER_CRITICAL(&mutex_);
    outputAttribute_ = attribute;
    portEXIT_CRITICAL(&mutex_);
    return true;
}
bool Esp32GattsSource::outputScopeCurrent(const OutputValue &caller) const noexcept {
    const auto value = caller;
    GattsSourceRegistration registration;
    if (!value.original.valid() || !value.authority.gameMaster || !value.authority.principal ||
        !readRegistration(registration) || registration.token != value.original.registration ||
        registration.interface != value.original.interface)
        return false;
    const auto matches = [&]() {
        portENTER_CRITICAL(&mutex_);
        const auto owned = identitySnapshot_;
        const bool current = outputAttribute_ && peerProofOpen_ && !registryFenced_ &&
                             lifecycle_ == value.lifecycle && owned.current && owned.connected &&
                             owned.proofAllowed && owned.device == value.original.device &&
                             owned.connection == value.original.connection &&
                             owned.authority == value.authority;
        portEXIT_CRITICAL(&mutex_);
        return current;
    };
    TrustedAuthority observed;
    return matches() && channel_.readAuthority(observed) && observed == value.authority &&
           matches();
}
ReplyFrameCopy Esp32GattsSource::copyReplyFrame(const RoutedReplyFrame &caller) noexcept {
    const auto frame = caller; // Freeze caller bytes before any external observation.
    const auto task = xTaskGetCurrentTaskHandle();
    const auto now = clock_.nowMilliseconds();
    if (!task || !frame.principal || !frame.epoch || !frame.count || frame.count > OutputBytes)
        return ReplyFrameCopy::Unavailable;
    TrustedAuthority observed;
    if (!channel_.readAuthority(observed) || !observed.gameMaster ||
        observed.principal != frame.principal || observed.epoch != frame.epoch)
        return ReplyFrameCopy::Unavailable;
    OutputValue value;
    value.frame = frame;
    value.authority = observed;
    value.copiedAt = now;
    portENTER_CRITICAL(&mutex_);
    const auto owned = identitySnapshot_;
    const bool permitted = outputAttribute_ && peerProofOpen_ && !registryFenced_ &&
                           owned.current && owned.connected && owned.proofAllowed &&
                           owned.authority == observed &&
                           (!outputWorker_ || outputWorker_ == task) && task != sourceTask_ &&
                           outputCounter_ != UINT32_MAX;
    if (permitted) {
        value.original.device = owned.device;
        value.original.boot = proofBoot_;
        value.original.connection = owned.connection;
        value.original.registration = registrationToken_;
        value.original.interface = interface_;
        value.lifecycle = lifecycle_;
        if (outputPending_ &&
            (now - output_.copiedAt >= OutputAgeMilliseconds || !(output_.authority == observed) ||
             !(output_.original == value.original) || output_.lifecycle != value.lifecycle))
            outputPending_ = false;
    }
    const bool busy = permitted && outputPending_;
    if (permitted && !busy) {
        outputWorker_ = task;
        value.token = ++outputCounter_;
        output_ = value;
        outputPending_ = true;
    }
    portEXIT_CRITICAL(&mutex_);
    if (!permitted)
        return ReplyFrameCopy::Unavailable;
    if (busy)
        return ReplyFrameCopy::Busy;
    if (clock_.nowMilliseconds() - value.copiedAt < OutputAgeMilliseconds &&
        outputScopeCurrent(value))
        return ReplyFrameCopy::Copied;
    portENTER_CRITICAL(&mutex_);
    if (output_.token == value.token)
        outputPending_ = false;
    portEXIT_CRITICAL(&mutex_);
    return ReplyFrameCopy::Unavailable;
}
void Esp32GattsSource::readOutputNow(std::uint16_t packed, std::uint32_t transaction,
                                     const sdk446::Read &caller) noexcept {
    const auto read = caller;
    if (!sourceTask() || outputReading_)
        return;
    OutputReadGuard guard(outputReading_);
    PeerTransportBinding transport;
    if (!sourceTask() || !read.needResponse || !transportNow(transport) ||
        packed != transport.connection.handle)
        return;
    OutputValue value;
    bool pending;
    portENTER_CRITICAL(&mutex_);
    pending = outputPending_;
    if (pending && (!(output_.original.connection == transport.connection) ||
                    output_.original.registration != transport.registration ||
                    output_.lifecycle != transport.lifecycle ||
                    !(output_.authority == identitySnapshot_.authority))) {
        outputPending_ = false;
        pending = false;
    }
    if (pending) {
        value = output_;
    } else {
        value.original.device = transport.device;
        value.original.boot = proofBoot_;
        value.original.connection = transport.connection;
        value.original.registration = transport.registration;
        value.original.interface = transport.interface;
        value.authority = identitySnapshot_.authority;
        value.lifecycle = transport.lifecycle;
    }
    portEXIT_CRITICAL(&mutex_);
    const auto capturedAt = clock_.nowMilliseconds();
    OriginalPeerSecurity security;
    std::uint8_t status = 8; // No GM: no private output, including an idle-success assertion.
    if (outputScopeCurrent(value) && readOriginalPeerSecurityNow(value.original, security) &&
        outputScopeCurrent(value)) {
        status = read.offset || read.isLong ? 7 : 0;
        if (pending && capturedAt - value.copiedAt >= OutputAgeMilliseconds)
            status = 14; // Expired slot; never report a lost value as a successful empty poll.
    }
    sdk446::Response response = {};
    response.value.handle = read.handle;
    if (!status && pending) {
        response.value.length = value.frame.count;
        std::memcpy(response.value.value, value.frame.bytes.data(), value.frame.count);
    }
    const auto sendingAt = clock_.nowMilliseconds();
    // Same actor at the actual SDK call: fresh original security and authority,
    // not the public delayed GAP callback or a lookup by a recycled raw handle.
    if (!status && (sendingAt - capturedAt >= OutputAgeMilliseconds ||
                    (pending && sendingAt - value.copiedAt >= OutputAgeMilliseconds))) {
        status = 14;
        response.value.length = 0;
    }
    if (!status &&
        (!readOriginalPeerSecurityNow(value.original, security) || !outputScopeCurrent(value))) {
        status = 8;
        response.value.length = 0;
    }
    // SDK metadata ports above may take time. Their work cannot renew either
    // the mailbox age or this original read's absolute response deadline.
    const auto finalAt = clock_.nowMilliseconds();
    if (!status && (finalAt - capturedAt >= OutputAgeMilliseconds ||
                    (pending && finalAt - value.copiedAt >= OutputAgeMilliseconds))) {
        status = 14;
        response.value.length = 0;
    }
    if (!status && !outputScopeCurrent(value)) {
        status = 8;
        response.value.length = 0;
    }
    PeerTransportBinding after;
    if (sourceTask() && transportNow(after) && after == transport) {
        // No local critical section surrounds SDK code. Success is local submission,
        // never a delivery acknowledgement. A lost response is not automatically replayed.
        (void)GATTS_SendRsp(packed, transaction, status, &response);
        if (pending && (status == 0 || status == 8 || status == 14)) {
            portENTER_CRITICAL(&mutex_);
            if (output_.token == value.token)
                outputPending_ = false;
            portEXIT_CRITICAL(&mutex_);
        }
    }
}
} // namespace companion
} // namespace larp
