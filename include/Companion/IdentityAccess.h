#pragma once
#include "Companion/Identity.h"
namespace larp {
namespace companion {
struct BoundConnection {
    std::uint32_t token = 0;  // Locally minted closed-authority epoch; never reused in this boot.
    std::uint16_t handle = 0; // SDK handle alone is insufficient.
    bool operator==(const BoundConnection &other) const noexcept {
        return token == other.token && handle == other.handle;
    }
};
// Supplied ONLY by a verifier for that exact connection and stable credential
// registry identity. Never construct from advertising/GATT JSON/auth success alone.
// The SDK adapter must prove identity attribution as well as these security flags.
struct VerifiedPeerEvidence {
    Identity peer;
    bool encrypted = false, mitm = false, secureConnections = false, bonded = false;
    bool valid() const noexcept {
        return peer.valid() && encrypted && mitm && secureConnections && bonded;
    }
};
enum class IdentityAccessOutcome : std::uint8_t {
    Initialized,
    Connected,
    Verified,
    Assigned,
    Revoked,
    Disconnected,
    Duplicate,
    Busy,
    StaleConnection,
    InvalidIdentity,
    InvalidAssignment,
    SecurityRejected,
    IdentityConflict,
    Capacity,
    AuthorityUnavailable
};
struct IdentityAccessLimits {
    static const std::size_t Assignments = 4;
};
// Single serialized trusted owner, fixed volatile registry. Persistent credentials,
// device-ID provisioning, local-admin channel and real verification remain separate.
// No client protocol can assign rights through this class.
class IdentityAccess {
  public:
    explicit IdentityAccess(IAuthorityPublisher &authority) : authority_(authority) {}
    IdentityAccess(const IdentityAccess &) = delete;
    IdentityAccess &operator=(const IdentityAccess &) = delete;
    IdentityAccessOutcome begin(const Identity &device) noexcept;
    bool deviceIdentity(Identity &destination) const noexcept;
    IdentityAccessOutcome connect(std::uint16_t handle, BoundConnection &destination) noexcept;
    IdentityAccessOutcome authenticate(const BoundConnection &connection,
                                       const VerifiedPeerEvidence &evidence) noexcept;
    // Retire the exact cached full authority, not connection.token (role epochs
    // advance on the same connection). A replaced producer permanently retires
    // this old owner; it cannot revoke or reconnect over the newer owner.
    IdentityAccessOutcome disconnect(const BoundConnection &connection) noexcept;
    // Explicit trusted local-administrator operations; never an untrusted App write.
    IdentityAccessOutcome assignGameMaster(const Identity &peer, std::uint32_t principal) noexcept;
    IdentityAccessOutcome revokeGameMaster(const Identity &peer) noexcept;
    // Exact live binding AND channel observation. False preserves destination.
    bool route(const BoundConnection &connection, TrustedAuthority &destination) const noexcept;

  private:
    struct Assignment {
        Identity peer;
        std::uint32_t principal = 0;
    };
    IAuthorityPublisher &authority_;
    std::array<Assignment, IdentityAccessLimits::Assignments> assignments_ = {};
    Identity device_, peer_;
    BoundConnection connection_;
    TrustedAuthority current_;
    bool initialized_ = false, fenced_ = false, connected_ = false, verified_ = false,
         failedLink_ = false, authorityRetired_ = false;
    bool matches(const BoundConnection &connection) const noexcept;
    std::uint32_t assignedPrincipal(const Identity &peer) const noexcept;
    bool publish(std::uint32_t principal, bool gm, bool requireOriginal = false) noexcept;
    IdentityAccessOutcome rejectSecurity(IdentityAccessOutcome outcome) noexcept;
};
} // namespace companion
} // namespace larp
