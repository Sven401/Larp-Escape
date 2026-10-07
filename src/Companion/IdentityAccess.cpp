#include "Companion/IdentityAccess.h"
namespace larp {
namespace companion {
IdentityAccessOutcome IdentityAccess::begin(const Identity &device) noexcept {
    if (fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    if (!device.valid() || (initialized_ && !(device == device_))) {
        fenced_ = true;
        authority_.suspendAuthority();
        return IdentityAccessOutcome::InvalidIdentity;
    }
    if (initialized_)
        return IdentityAccessOutcome::Duplicate;
    authority_.suspendAuthority();
    device_ = device;
    initialized_ = true;
    return IdentityAccessOutcome::Initialized;
}
bool IdentityAccess::deviceIdentity(Identity &destination) const noexcept {
    if (!initialized_ || fenced_)
        return false;
    destination = device_;
    return true;
}
bool IdentityAccess::matches(const BoundConnection &connection) const noexcept {
    return initialized_ && !fenced_ && connected_ && connection.token && connection == connection_;
}
std::uint32_t IdentityAccess::assignedPrincipal(const Identity &peer) const noexcept {
    for (const auto &assignment : assignments_)
        if (assignment.principal && assignment.peer == peer)
            return assignment.principal;
    return 0;
}
bool IdentityAccess::publish(std::uint32_t principal, bool gm, bool requireOriginal) noexcept {
    const auto expected = current_;
    current_ = {};
    const auto result =
        requireOriginal ? authority_.advanceAuthorityIfCurrent(expected, principal, gm, current_)
                        : authority_.advanceAuthority(principal, gm, current_);
    if (result != AuthorityUpdate::Published) {
        authority_.suspendAuthority();
        verified_ = false;
        failedLink_ = true;
        return false;
    }
    return true;
}
IdentityAccessOutcome IdentityAccess::connect(std::uint16_t handle,
                                              BoundConnection &destination) noexcept {
    if (!initialized_ || fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    if (connected_)
        return IdentityAccessOutcome::Busy;
    if (!publish(0, false))
        return IdentityAccessOutcome::AuthorityUnavailable;
    connection_.token = current_.epoch;
    connection_.handle = handle;
    connected_ = true;
    verified_ = failedLink_ = false;
    peer_ = {};
    destination = connection_;
    return IdentityAccessOutcome::Connected;
}
IdentityAccessOutcome IdentityAccess::rejectSecurity(IdentityAccessOutcome outcome) noexcept {
    verified_ = false;
    failedLink_ = true;
    peer_ = {};
    return publish(0, false) ? outcome : IdentityAccessOutcome::AuthorityUnavailable;
}
IdentityAccessOutcome IdentityAccess::authenticate(const BoundConnection &connection,
                                                   const VerifiedPeerEvidence &evidence) noexcept {
    if (!matches(connection))
        return IdentityAccessOutcome::StaleConnection;
    if (failedLink_)
        return IdentityAccessOutcome::SecurityRejected;
    if (!evidence.valid())
        return rejectSecurity(IdentityAccessOutcome::SecurityRejected);
    if (verified_) {
        if (evidence.peer == peer_)
            return IdentityAccessOutcome::Duplicate;
        return rejectSecurity(IdentityAccessOutcome::IdentityConflict);
    }
    peer_ = evidence.peer;
    verified_ = true;
    const auto principal = assignedPrincipal(peer_);
    if (!publish(principal, principal != 0, true))
        return IdentityAccessOutcome::AuthorityUnavailable;
    return IdentityAccessOutcome::Verified;
}
IdentityAccessOutcome IdentityAccess::disconnect(const BoundConnection &connection) noexcept {
    if (!matches(connection))
        return IdentityAccessOutcome::StaleConnection;
    connected_ = verified_ = failedLink_ = false;
    peer_ = {};
    connection_ = {};
    return publish(0, false) ? IdentityAccessOutcome::Disconnected
                             : IdentityAccessOutcome::AuthorityUnavailable;
}
IdentityAccessOutcome IdentityAccess::assignGameMaster(const Identity &peer,
                                                       std::uint32_t principal) noexcept {
    if (!initialized_ || fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    if (!peer.valid() || !principal)
        return IdentityAccessOutcome::InvalidAssignment;
    Assignment *empty = nullptr;
    for (auto &assignment : assignments_) {
        if (assignment.principal && assignment.peer == peer)
            return assignment.principal == principal ? IdentityAccessOutcome::Duplicate
                                                     : IdentityAccessOutcome::InvalidAssignment;
        if (assignment.principal == principal)
            return IdentityAccessOutcome::InvalidAssignment;
        if (!assignment.principal && !empty)
            empty = &assignment;
    }
    if (!empty)
        return IdentityAccessOutcome::Capacity;
    empty->peer = peer;
    empty->principal = principal;
    if (connected_ && verified_ && peer == peer_ && !publish(principal, true, true))
        return IdentityAccessOutcome::AuthorityUnavailable;
    return IdentityAccessOutcome::Assigned;
}
IdentityAccessOutcome IdentityAccess::revokeGameMaster(const Identity &peer) noexcept {
    if (!initialized_ || fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    if (!peer.valid())
        return IdentityAccessOutcome::InvalidAssignment;
    for (auto &assignment : assignments_) {
        if (assignment.principal && assignment.peer == peer) {
            assignment = {};
            if (connected_ && verified_ && peer == peer_ && !publish(0, false))
                return IdentityAccessOutcome::AuthorityUnavailable;
            return IdentityAccessOutcome::Revoked;
        }
    }
    return IdentityAccessOutcome::Duplicate;
}
bool IdentityAccess::route(const BoundConnection &connection,
                           TrustedAuthority &destination) const noexcept {
    if (!matches(connection) || !verified_ || failedLink_ || !current_.gameMaster ||
        current_.principal != assignedPrincipal(peer_))
        return false;
    TrustedAuthority actual;
    if (!authority_.readAuthority(actual) || !(actual == current_))
        return false;
    destination = actual;
    return true;
}
} // namespace companion
} // namespace larp
