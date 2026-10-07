#include "Companion/PeerRegistryRuntime.h"
namespace larp {
namespace companion {
bool PeerRegistryRuntime::close() noexcept {
    std::array<GameMasterAssignment, IdentityAccessLimits::Assignments> empty = {};
    return access_.replaceGameMastersClosed(empty) == IdentityAccessOutcome::AssignmentsReplaced;
}
bool PeerRegistryRuntime::replaceClosed(const PeerRegistrySnapshot &caller) noexcept {
    const auto selected = caller;
    Identity device;
    if (!validPeerRegistry(selected) || !access_.deviceIdentity(device) ||
        !(selected.device == device))
        return false;
    std::array<GameMasterAssignment, IdentityAccessLimits::Assignments> roles = {};
    unsigned count = 0;
    for (unsigned i = 0; i < selected.count; ++i)
        if (selected.peers[i].gameMaster) {
            roles[count].peer = selected.peers[i].credential.peer;
            roles[count++].principal = selected.peers[i].principal;
        }
    return access_.replaceGameMastersClosed(roles) == IdentityAccessOutcome::AssignmentsReplaced;
}
} // namespace companion
} // namespace larp
