#pragma once
#include "GameExecution/RulesPlanner.h"

namespace larp {
namespace execution {
namespace rules {

enum class ManualAuthorityResult : std::uint8_t { Current, Closed, Stale, Unavailable };
class IManualAuthority {
  public:
    virtual ~IManualAuthority() = default;
    // Checks this ORIGINAL copied transport/registry/event authority. It must
    // never substitute a newly observed principal/connection/role for it.
    virtual ManualAuthorityResult check(const Context &, const OverrideAuthority &) = 0;
};
class IManualIdentitySource {
  public:
    virtual ~IManualIdentitySource() = default;
    virtual bool make(Identity &) = 0;
};
class IManualClock {
  public:
    virtual ~IManualClock() = default;
    // Monotonic owner clock; final admission reads it again, never a cached
    // reception timestamp. Failure closes admission; wrap is not permitted.
    virtual bool now(std::uint64_t &) = 0;
};
enum class ManualStatus : std::uint8_t {
    Rejected,
    Deferred,
    Prepared,
    Confirming,
    Committed,
    Duplicate,
    AlreadyReleased,
    Unknown,
    NotCommitted,
    Expired
};
enum class ManualError : std::uint8_t {
    None,
    InvalidArgument,
    Busy,
    Context,
    Authority,
    Unavailable,
    Stale,
    Conflict,
    Capacity,
    Crypto,
    Identity,
    Expired,
    UnknownOperation,
    Transaction,
    RecoveryRequired
};
// Committed/Duplicate describe durability, NOT successful guard firing or
// physical observation. Released on a new replacement requires the actual
// shared latch AND own T2 output; no-fire receipt has NoRelease. AlreadyReleased
// refers to the verified existing group and claims no newly produced T2.
enum class ReleaseDisposition : std::uint8_t { Unresolved, Released, NoRelease };
struct ManualResult {
    ManualStatus status = ManualStatus::Rejected;
    ManualError error = ManualError::None;
    ReleaseDisposition release = ReleaseDisposition::Unresolved;
    Identity token, operation;
    std::uint64_t expiresAt = 0;
};
// Compact client payloads refer to a copied server-side full view. All actor
// attributes are supplied by the original trusted ingress adapter, never parsed
// as rights from the client command. Prepare/Confirm have distinct phase IDs.
struct ManualPrepareRequest {
    Identity ownerBoot, phase, rule;
    Digest view;
    OverrideAuthority authority;
};
struct ManualConfirmRequest {
    Identity ownerBoot, token, phase;
    OverrideAuthority authority;
};
struct ManualView {
    Identity ownerBoot, rule;
    Digest digest;
};
struct ManualReplay {
    OverrideRecord record;
    ReleaseDisposition release = ReleaseDisposition::Unresolved;
    ManualStatus status = ManualStatus::Prepared;
    ManualError error = ManualError::None;
};
struct ManualWorkspace {
    // Complete immutable request identities remain after an active grant is
    // terminal. No eviction, deadline refresh, or reuse of old phase IDs.
    Pool<ManualReplay> replays;
    // Fresh canonical encoding of the ENTIRE owned Program, never source-only
    // hash or a caller-claimed ready flag. Capacity <=32768 semantic profile.
    Pool<std::uint8_t> catalogBytes;
    std::size_t maximumActive = 4;
};
class IManualFinalFence {
  public:
    virtual ~IManualFinalFence() = default;
    // The transaction invokes this immediately before its Application Final,
    // then keeps context/authority/revisions serialized through resolution.
    virtual ManualAuthorityResult check(const OverrideRecord &) = 0;
};
class IManualTransaction {
  public:
    virtual ~IManualTransaction() = default;
    // Must freeze complete Plan, authorization, original signed bytes/keys,
    // candidate range and physical request IDs before the first data write.
    // Chunks alone have no effect. No publication before the single D1 Final.
    // Repeated calls for the same operation reuse identical frozen bytes.
    virtual ManualResult submit(const OverrideAdmission &, IManualFinalFence &) = 0;
    // Unknown is resolved ONLY after actual backend reopen+qualified recovery.
    // This returns the historical result; it does not create a new admission.
    virtual ManualResult status(const Context &, const Identity &operation) = 0;
};
// Pure serialized owner state machine. It does not itself authenticate, sign,
// persist, publish or execute. Concrete Authority and Transaction adapters are
// mandatory; this class is not a standalone product authorization boundary.
// All referenced Program/State/pools/ports remain exclusively owned, immutable
// during callbacks, nonaliasing, and alive for this owner. No callback reentry.
// New ownerBoot after process loss invalidates EVERY former Prepare token.
class ManualOverrideOwner : private IManualFinalFence {
  public:
    ManualOverrideOwner(const Program &program, const State &state, const Identity &ownerBoot,
                        std::uint64_t lifetime, configuration::IPackageDigest &hash,
                        IManualClock &clock, IManualAuthority &authority,
                        IManualIdentitySource &identities, IManualTransaction &transaction,
                        ManualWorkspace &workspace);
    ManualOverrideOwner(const ManualOverrideOwner &) = delete;
    ManualOverrideOwner &operator=(const ManualOverrideOwner &) = delete;
    ManualResult view(const Identity &originalRule, const OverrideAuthority &, ManualView &);
    ManualResult prepare(const ManualPrepareRequest &);
    ManualResult confirm(const ManualConfirmRequest &);
    // Current newly checked authority may read a historical receipt, but never
    // execute an old Confirm or rebind its old grant after restart/reconnection.
    ManualResult status(const Context &, const OverrideAuthority &, const Identity &operation);
    // Serialized worker continuation also retires a revoked/expired pending
    // transaction through its real final fence, without asking an old peer to
    // regain rights or mint a replacement operation.
    ManualResult service();

  private:
    const Program &program_;
    const State &state_;
    Identity boot_;
    std::uint64_t lifetime_ = 0, now_ = 0;
    configuration::IPackageDigest &hash_;
    IManualClock &clock_;
    IManualAuthority &authority_;
    IManualIdentitySource &ids_;
    IManualTransaction &transaction_;
    ManualWorkspace &work_;
    bool operating_ = false;
    Id submitting_ = 0;
    ManualAuthorityResult check(const OverrideRecord &) override;
    ManualAuthorityResult validate(const OverrideRecord &);
    bool fingerprint(Digest &);
    bool unknown() const;
    bool target(const Identity &, Id &original, Id &replacement) const;
    bool target(const OverrideRecord &) const;
    bool released(const OverrideRecord &) const;
    bool digest(const OverrideRecord &, unsigned purpose, Digest &);
    bool validWorkspace() const;
    bool unique(const Identity &) const;
    void retire(std::uint64_t now);
};
} // namespace rules
} // namespace execution
} // namespace larp
