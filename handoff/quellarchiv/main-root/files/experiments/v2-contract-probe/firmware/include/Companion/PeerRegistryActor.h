#pragma once
#include "Companion/IdentityAccess.h"
namespace larp {
namespace companion {
struct PeerRegistryActorLimits {
    static const std::uint32_t DeadlineMilliseconds = 5000;
};
struct PeerRegistryActorOrigin {
    Identity device;
    TrustedAuthority authority;
    std::uint32_t registration = 0, lifecycle = 0;
    std::uint8_t interface = 0xff;
    bool valid() const noexcept {
        return device.valid() && registration && lifecycle && interface && interface != 0xff &&
               (authority.epoch
                    ? (authority.gameMaster ? authority.principal != 0 : authority.principal == 0)
                    : authority == TrustedAuthority{});
    }
    bool operator==(const PeerRegistryActorOrigin &other) const noexcept {
        return device == other.device && authority == other.authority &&
               registration == other.registration && lifecycle == other.lifecycle &&
               interface == other.interface;
    }
};
enum class PeerRegistryRoleKind : std::uint8_t { Close, Install };
struct PeerRegistryRoleJob {
    PeerRegistryActorOrigin original;
    std::array<GameMasterAssignment, 4> roles = {};
    std::uint32_t sequence = 0, receivedAt = 0;
    PeerRegistryRoleKind kind = PeerRegistryRoleKind::Close;
    bool operator==(const PeerRegistryRoleJob &other) const noexcept {
        if (!(original == other.original) || sequence != other.sequence ||
            receivedAt != other.receivedAt || kind != other.kind)
            return false;
        for (unsigned i = 0; i < 4; ++i)
            if (!(roles[i].peer == other.roles[i].peer) ||
                roles[i].principal != other.roles[i].principal)
                return false;
        return true;
    }
};
bool validPeerRegistryRoleJob(const PeerRegistryRoleJob &) noexcept;
enum class PeerRegistryRolePost : std::uint8_t { Posted, Duplicate, Busy, Rejected };
enum class PeerRegistryRoleState : std::uint8_t { Pending, Confirmed, Rejected, Unavailable };
} // namespace companion
} // namespace larp
