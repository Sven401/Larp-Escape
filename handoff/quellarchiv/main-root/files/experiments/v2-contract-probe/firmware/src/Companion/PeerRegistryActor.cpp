#include "Companion/PeerRegistryActor.h"
#include <algorithm>
namespace larp {
namespace companion {
bool validPeerRegistryRoleJob(const PeerRegistryRoleJob &job) noexcept {
    if (!job.original.valid() || !job.sequence ||
        (job.kind != PeerRegistryRoleKind::Close && job.kind != PeerRegistryRoleKind::Install))
        return false;
    bool ended = false;
    for (unsigned i = 0; i < 4; ++i) {
        const auto &role = job.roles[i];
        if (!role.principal) {
            if (role.peer.valid())
                return false;
            ended = true;
            continue;
        }
        if (ended || !role.peer.valid() || job.kind == PeerRegistryRoleKind::Close)
            return false;
        if (i && !std::lexicographical_compare(job.roles[i - 1].peer.bytes.begin(),
                                               job.roles[i - 1].peer.bytes.end(),
                                               role.peer.bytes.begin(), role.peer.bytes.end()))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (role.principal == job.roles[j].principal)
                return false;
    }
    return true;
}
} // namespace companion
} // namespace larp
