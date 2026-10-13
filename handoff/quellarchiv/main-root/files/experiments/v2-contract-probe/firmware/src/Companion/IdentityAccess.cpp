#include "Companion/IdentityAccess.h"
namespace larp {
namespace companion {
struct IdentityAccess::Observation {
    IdentityAccess &owner;
    ~Observation() {
        owner.notifyObserver();
    }
};
bool IdentityAccess::attachObserver(IIdentityAccessObserver &observer) noexcept {
    if (observer_ && observer_ != &observer)
        return false;
    observer_ = &observer;
    notifyObserver();
    return true;
}
void IdentityAccess::notifyObserver() noexcept {
    if (!observer_)
        return;
    IdentityAccessSnapshot value;
    value.device = device_;
    value.connection = connection_;
    value.authority = current_;
    value.current = initialized_ && !fenced_ && !authorityRetired_;
    value.connected = value.current && connected_;
    value.proofAllowed = value.connected && !failedLink_ && current_.epoch;
    observer_->copyIdentitySnapshot(value);
}
IdentityAccessOutcome IdentityAccess::begin(const Identity &device) noexcept {
    Observation observation{*this};
    if (authorityRetired_)
        return IdentityAccessOutcome::AuthorityUnavailable;
    if (fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    if (!device.valid() || (initialized_ && !(device == device_))) {
        fenced_ = true;
        notifyObserver();
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
    return initialized_ && !fenced_ && !authorityRetired_ && connected_ && connection.token &&
           connection == connection_;
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
    notifyObserver();
    const bool retiring = connected_ && !principal && !gm && !requireOriginal;
    const auto result =
        retiring          ? authority_.closeAuthorityIfCurrent(expected, current_)
        : requireOriginal ? authority_.advanceAuthorityIfCurrent(expected, principal, gm, current_)
                          : authority_.advanceAuthority(principal, gm, current_);
    if (result != AuthorityUpdate::Published) {
        if (retiring || requireOriginal)
            authorityRetired_ = result != AuthorityUpdate::Suspended;
        else
            authority_.suspendAuthority();
        verified_ = false;
        failedLink_ = true;
        return false;
    }
    return true;
}
AuthorityUpdate IdentityAccess::closeCopiedAuthority(const TrustedAuthority &caller,
                                                     TrustedAuthority &out) const noexcept {
    const auto expected = caller;
    return authority_.closeAuthorityIfCurrent(expected, out);
}
bool IdentityAccess::acknowledgeCopiedClose(const BoundConnection &caller,
                                            const TrustedAuthority &expectedInput,
                                            const TrustedAuthority &closedInput) noexcept {
    const auto original = caller;
    const auto expected = expectedInput;
    const auto closed = closedInput;
    Observation observation{*this};
    if (!matches(original) || !(current_ == expected) || !expected.epoch ||
        closed.epoch <= expected.epoch || closed.principal || closed.gameMaster)
        return false;
    TrustedAuthority observed;
    if (!authority_.readAuthority(observed) || !(observed == closed) || !matches(original) ||
        !(current_ == expected))
        return false;
    current_ = closed;
    verified_ = false;
    failedLink_ = true;
    peer_ = {};
    return true;
}
IdentityAccessOutcome IdentityAccess::connect(std::uint16_t handle,
                                              BoundConnection &destination) noexcept {
    Observation observation{*this};
    if (authorityRetired_)
        return IdentityAccessOutcome::AuthorityUnavailable;
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
    Observation observation{*this};
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
    Observation observation{*this};
    const auto original = connection;
    if (!matches(original))
        return IdentityAccessOutcome::StaleConnection;
    const auto expected = current_;
    // Drop the old local connection before any external port observation.
    connected_ = verified_ = failedLink_ = false;
    peer_ = {};
    connection_ = {};
    current_ = {};
    notifyObserver();
    TrustedAuthority closed;
    const auto result = authority_.closeAuthorityIfCurrent(expected, closed);
    if (result == AuthorityUpdate::Published) {
        current_ = closed;
        return IdentityAccessOutcome::Disconnected;
    }
    // A suspended channel can reconnect with a fresh epoch. A different live
    // producer retires this old owner for its lifetime; begin cannot reset it.
    authorityRetired_ = result != AuthorityUpdate::Suspended;
    return IdentityAccessOutcome::AuthorityUnavailable;
}
IdentityAccessOutcome IdentityAccess::assignGameMaster(const Identity &peer,
                                                       std::uint32_t principal) noexcept {
    Observation observation{*this};
    if (authorityRetired_)
        return IdentityAccessOutcome::AuthorityUnavailable;
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
    Observation observation{*this};
    if (authorityRetired_)
        return IdentityAccessOutcome::AuthorityUnavailable;
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
IdentityAccessOutcome IdentityAccess::replaceGameMastersClosed(
    const std::array<GameMasterAssignment, IdentityAccessLimits::Assignments> &caller,
    TrustedAuthority *closedAck) noexcept {
    Observation observation{*this};
    const auto selected = caller;
    if (authorityRetired_)
        return IdentityAccessOutcome::AuthorityUnavailable;
    if (!initialized_ || fenced_)
        return IdentityAccessOutcome::InvalidIdentity;
    for (unsigned i = 0; i < selected.size(); ++i) {
        const auto &entry = selected[i];
        if (entry.principal ? !entry.peer.valid() : entry.peer.valid())
            return IdentityAccessOutcome::InvalidAssignment;
        for (unsigned j = 0; j < i; ++j)
            if (entry.principal &&
                (entry.peer == selected[j].peer || entry.principal == selected[j].principal))
                return IdentityAccessOutcome::InvalidAssignment;
    }
    const auto expected = current_;
    // Before any external observation, all local route/proof checks close.
    verified_ = false;
    failedLink_ = connected_;
    peer_ = {};
    assignments_ = {};
    current_ = {};
    notifyObserver();
    TrustedAuthority closed;
    const auto result = authority_.closeAuthorityIfCurrent(expected, closed);
    if (result == AuthorityUpdate::Published)
        current_ = closed;
    else if (result != AuthorityUpdate::Suspended) {
        authorityRetired_ = true;
        return IdentityAccessOutcome::AuthorityUnavailable;
    }
    for (unsigned i = 0; i < selected.size(); ++i) {
        assignments_[i].peer = selected[i].peer;
        assignments_[i].principal = selected[i].principal;
    }
    if (closedAck)
        *closedAck = current_;
    return IdentityAccessOutcome::AssignmentsReplaced;
}
bool IdentityAccess::proofConnectionCurrent(const BoundConnection &caller) const noexcept {
    const auto original = caller;
    if (!matches(original) || failedLink_)
        return false;
    const auto expected = current_;
    TrustedAuthority observed;
    return authority_.readAuthority(observed) && observed == expected && matches(original) &&
           !failedLink_ && current_ == expected;
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
