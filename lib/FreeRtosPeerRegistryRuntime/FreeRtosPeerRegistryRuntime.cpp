#include "FreeRtosPeerRegistryRuntime.h"
namespace larp {
namespace companion {
namespace {
class RegistryWorkerCall {
  public:
    explicit RegistryWorkerCall(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~RegistryWorkerCall() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
} // namespace
bool FreeRtosPeerRegistryRuntime::begin(const Identity &caller) noexcept {
    const auto device = caller;
    if (attempted_)
        return !fenced_ && device == device_ && worker_ == xTaskGetCurrentTaskHandle();
    attempted_ = true;
    PeerRegistryActorOrigin original;
    if (!device.valid() || !xTaskGetCurrentTaskHandle() ||
        !source_.readPeerRoleOrigin(device, original))
        return false;
    device_ = device;
    worker_ = xTaskGetCurrentTaskHandle();
    closed_ = original;
    return true;
}
bool FreeRtosPeerRegistryRuntime::availableWorker() const noexcept {
    return worker_ && worker_ == xTaskGetCurrentTaskHandle() && !fenced_;
}
void FreeRtosPeerRegistryRuntime::fence() noexcept {
    portENTER_CRITICAL(&mutex_);
    ready_ = false;
    fenced_ = true;
    portEXIT_CRITICAL(&mutex_);
    if (worker_ && worker_ == xTaskGetCurrentTaskHandle()) {
        source_.fencePeerRoles(job_);
        TrustedAuthority ignored;
        // Only the exact captured authority; never suspend a replacement producer.
        authority_.closeAuthorityIfCurrent(closed_.authority, ignored);
    }
}
PeerRegistryRuntimeResult FreeRtosPeerRegistryRuntime::reject() noexcept {
    fence();
    return PeerRegistryRuntimeResult::Failure;
}
PeerRegistryRuntimeResult FreeRtosPeerRegistryRuntime::poll() noexcept {
    if (clock_.nowMilliseconds() - started_ >= PeerRegistryWorkerLimits::OperationMilliseconds)
        return reject();
    TrustedAuthority ack;
    const auto state = source_.peerRoleState(job_, ack);
    if (state == PeerRegistryRoleState::Pending)
        return PeerRegistryRuntimeResult::Pending;
    if (state != PeerRegistryRoleState::Confirmed)
        return reject();
    closed_ = job_.original;
    closed_.authority = ack;
    return PeerRegistryRuntimeResult::Confirmed;
}
PeerRegistryRuntimeResult FreeRtosPeerRegistryRuntime::close() noexcept {
    if (busy_)
        return PeerRegistryRuntimeResult::Pending;
    if (!availableWorker())
        return PeerRegistryRuntimeResult::Failure;
    RegistryWorkerCall call(busy_);
    if (phase_ == Phase::Closing) {
        const auto result = poll();
        if (result == PeerRegistryRuntimeResult::Confirmed)
            phase_ = Phase::Closed;
        return result;
    }
    if (phase_ == Phase::Closed)
        return PeerRegistryRuntimeResult::Confirmed;
    if (phase_ != Phase::Idle || sequence_ == UINT32_MAX)
        return reject();
    portENTER_CRITICAL(&mutex_);
    ready_ = false;
    portEXIT_CRITICAL(&mutex_);
    PeerRegistryActorOrigin original;
    if (!source_.readPeerRoleOrigin(device_, original))
        return reject();
    closed_ = original;
    started_ = clock_.nowMilliseconds();
    job_ = {};
    job_.original = original;
    job_.sequence = ++sequence_;
    job_.receivedAt = started_;
    const auto posted = source_.postPeerRoleJob(job_);
    if (posted != PeerRegistryRolePost::Posted && posted != PeerRegistryRolePost::Duplicate)
        return reject();
    phase_ = Phase::Closing;
    const auto result = poll();
    if (result == PeerRegistryRuntimeResult::Confirmed)
        phase_ = Phase::Closed;
    return result;
}
PeerRegistryRuntimeResult
FreeRtosPeerRegistryRuntime::replaceClosed(const PeerRegistrySnapshot &caller) noexcept {
    const auto selected = caller;
    if (busy_)
        return PeerRegistryRuntimeResult::Pending;
    if (!availableWorker())
        return PeerRegistryRuntimeResult::Failure;
    RegistryWorkerCall call(busy_);
    if (!validPeerRegistry(selected) || !(selected.device == device_) ||
        (phase_ != Phase::Closed && phase_ != Phase::Installing))
        return reject();
    if (phase_ == Phase::Closed) {
        if (sequence_ == UINT32_MAX ||
            clock_.nowMilliseconds() - started_ >= PeerRegistryWorkerLimits::OperationMilliseconds)
            return reject();
        job_ = {};
        job_.original = closed_;
        job_.sequence = ++sequence_;
        job_.receivedAt = clock_.nowMilliseconds();
        job_.kind = PeerRegistryRoleKind::Install;
        unsigned roles = 0;
        for (unsigned i = 0; i < selected.count; ++i)
            if (selected.peers[i].gameMaster) {
                job_.roles[roles].peer = selected.peers[i].credential.peer;
                job_.roles[roles++].principal = selected.peers[i].principal;
            }
        // Staging stays unavailable until the original role installation ACK.
        portENTER_CRITICAL(&mutex_);
        published_ = selected;
        portEXIT_CRITICAL(&mutex_);
        const auto posted = source_.postPeerRoleJob(job_);
        if (posted != PeerRegistryRolePost::Posted && posted != PeerRegistryRolePost::Duplicate)
            return reject();
        phase_ = Phase::Installing;
    } else {
        // Fixed semantic comparison under the mux; no port/crypto/IO or second
        // 408-byte stack copy. selected remains frozen across observations.
        portENTER_CRITICAL(&mutex_);
        const bool unchanged = published_ == selected;
        portEXIT_CRITICAL(&mutex_);
        if (!unchanged)
            return reject();
    }
    const auto result = poll();
    if (result == PeerRegistryRuntimeResult::Confirmed) {
        if (!source_.installedPeerRoles(job_))
            return reject();
        portENTER_CRITICAL(&mutex_);
        publishedJob_ = job_;
        ready_ = true;
        portEXIT_CRITICAL(&mutex_);
        phase_ = Phase::Idle;
    }
    return result;
}
bool FreeRtosPeerRegistryRuntime::readPublished(PeerRegistrySnapshot &destination) const noexcept {
    PeerRegistrySnapshot snapshot;
    PeerRegistryRoleJob installed;
    portENTER_CRITICAL(&mutex_);
    const bool available = ready_ && !fenced_;
    snapshot = published_;
    installed = publishedJob_;
    portEXIT_CRITICAL(&mutex_);
    if (!available || !source_.installedPeerRoles(installed))
        return false;
    portENTER_CRITICAL(&mutex_);
    const bool current = ready_ && !fenced_ && publishedJob_ == installed;
    portEXIT_CRITICAL(&mutex_);
    if (!current)
        return false;
    destination = snapshot;
    return true;
}
bool FreeRtosPeerRegistryRuntime::snapshot(PeerRegistrySnapshot &out) const noexcept {
    return readPublished(out);
}
bool FreeRtosPeerRegistryRuntime::credential(const Identity &caller,
                                             PeerCredential &out) const noexcept {
    const auto peer = caller;
    PeerRegistrySnapshot snapshot;
    if (!peer.valid() || !readPublished(snapshot))
        return false;
    for (unsigned i = 0; i < snapshot.count; ++i)
        if (snapshot.peers[i].credential.peer == peer) {
            out = snapshot.peers[i].credential;
            return true;
        }
    return false;
}
bool FreeRtosPeerRegistryRuntime::credentialCurrent(const PeerCredential &caller) const noexcept {
    const auto selected = caller;
    PeerCredential current;
    return credential(selected.peer, current) && current == selected;
}
} // namespace companion
} // namespace larp
