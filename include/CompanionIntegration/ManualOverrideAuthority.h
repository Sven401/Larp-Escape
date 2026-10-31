#pragma once
#include "Companion/PeerRegistry.h"
#include "Companion/PeerRegistryActor.h"
#include "DeviceRuntime/EventAuthorityStorage.h"
#include "GameExecution/ManualOverrideOwner.h"

namespace larp {
namespace integration {
struct ManualOriginalIngress {
    companion::PeerProofBinding connection;
    companion::PeerRegistryActorOrigin actor;
};
class IManualOriginalSource {
  public:
    virtual ~IManualOriginalSource() = default;
    // Actual original service/actor liveness. Must compare the complete copied
    // registration/lifecycle/interface/authority and original connection, never
    // relabel an old queued command with a newly observed source/authority.
    virtual bool current(const ManualOriginalIngress &) const = 0;
};
struct ManualAuthorityEvidence {
    ManualOriginalIngress original;
    companion::PeerCredential credential;
    companion::AcceptedPeerProof possession;
    companion::PeerRegistryRecord registry;
    devices::event_authority::Snapshot event;
};
struct ManualAuthorityEntry {
    execution::rules::Context context;
    execution::rules::OverrideAuthority authority;
    ManualAuthorityEvidence evidence;
};
struct ManualAuthorityWorkspace {
    configuration::module_ir::Pool<companion::AcceptedPeerProof> connections;
    configuration::module_ir::Pool<ManualAuthorityEntry> entries;
};
// Concrete policy projection from existing owners, never a signature=>GM or
// client-supplied authorized flag. It STILL requires the actual original source
// adapter and original setup/event installation proof; neither is invented here.
// Exclusive serialized worker; all ports/storage/pools nonreentrant, nonaliasing
// and process-lifetime. The original source/credential is copied before callbacks.
class ManualOverrideAuthority final : public execution::rules::IManualAuthority {
  public:
    ManualOverrideAuthority(companion::PeerRegistryOwner &registry,
                            const companion::IdentityAccess &access,
                            const companion::IPeerProofContext &proof,
                            const IManualOriginalSource &source,
                            devices::event_authority::Storage &event,
                            configuration::IPackageDigest &hash,
                            ManualAuthorityWorkspace &workspace)
        : registry_(registry), access_(access), proof_(proof), source_(source), event_(event),
          hash_(hash), work_(workspace) {}
    // Actual proof owner export, called immediately at the original successful
    // grant before its short challenge deadline/reset. This caches possession
    // only; current GM/event rights are still checked on every capture/fence.
    bool rememberConnection(companion::PeerProofOwner &, const companion::PeerProofBinding &);
    // Called ONLY on a new genuinely attributed ingress. The output is a copied
    // identity bound to that original observation; an old command is not recaptured.
    execution::rules::ManualAuthorityResult capture(const execution::rules::Context &,
                                                    const ManualOriginalIngress &,
                                                    execution::rules::OverrideAuthority &);
    execution::rules::ManualAuthorityResult
    check(const execution::rules::Context &, const execution::rules::OverrideAuthority &) override;
    // Complete original mapping/key/event evidence for the versioned durable
    // authorization codec. Event evidence bytes are copied into exclusive staging.
    bool evidence(const execution::rules::Context &, const execution::rules::OverrideAuthority &,
                  ManualAuthorityEvidence &,
                  configuration::module_ir::Pool<std::uint8_t> &eventEvidence);

  private:
    companion::PeerRegistryOwner &registry_;
    const companion::IdentityAccess &access_;
    const companion::IPeerProofContext &proof_;
    const IManualOriginalSource &source_;
    devices::event_authority::Storage &event_;
    configuration::IPackageDigest &hash_;
    ManualAuthorityWorkspace &work_;
    bool operating_ = false;
    bool valid() const;
    bool registryDigest(const companion::PeerRegistrySnapshot &, companion::PeerRegistryRecord &,
                        execution::rules::Digest &);
    execution::rules::ManualAuthorityResult current(const ManualAuthorityEntry &);
};
} // namespace integration
} // namespace larp
