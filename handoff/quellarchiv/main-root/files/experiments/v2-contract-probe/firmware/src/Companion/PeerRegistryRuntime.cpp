#include "Companion/PeerRegistryRuntime.h"
namespace larp {
namespace companion {
PeerRegistryRuntimeResult PeerRegistryRuntime::close() noexcept {
    std::array<GameMasterAssignment, IdentityAccessLimits::Assignments> empty = {};
    return access_.replaceGameMastersClosed(empty) == IdentityAccessOutcome::AssignmentsReplaced
               ? PeerRegistryRuntimeResult::Confirmed
               : PeerRegistryRuntimeResult::Failure;
}
PeerRegistryRuntimeResult
PeerRegistryRuntime::replaceClosed(const PeerRegistrySnapshot &caller) noexcept {
    const auto selected = caller;
    Identity device;
    if (!validPeerRegistry(selected) || !access_.deviceIdentity(device) ||
        !(selected.device == device))
        return PeerRegistryRuntimeResult::Failure;
    std::array<GameMasterAssignment, IdentityAccessLimits::Assignments> roles = {};
    unsigned count = 0;
    for (unsigned i = 0; i < selected.count; ++i)
        if (selected.peers[i].gameMaster) {
            roles[count].peer = selected.peers[i].credential.peer;
            roles[count++].principal = selected.peers[i].principal;
        }
    return access_.replaceGameMastersClosed(roles) == IdentityAccessOutcome::AssignmentsReplaced
               ? PeerRegistryRuntimeResult::Confirmed
               : PeerRegistryRuntimeResult::Failure;
}
} // namespace companion
} // namespace larp
