#pragma once
#include "Companion/PeerRegistry.h"
namespace larp {
namespace companion {
// Same serialized original IdentityAccess actor, never a registry IO worker
// reaching across the SDK actor. Production needs an attributed copy/ack port.
class PeerRegistryRuntime : public IPeerRegistryRuntime {
  public:
    explicit PeerRegistryRuntime(IdentityAccess &access) : access_(access) {}
    PeerRegistryRuntimeResult close() noexcept override;
    PeerRegistryRuntimeResult replaceClosed(const PeerRegistrySnapshot &) noexcept override;
    void fence() noexcept override {
        (void)close();
    }

  private:
    IdentityAccess &access_;
};
} // namespace companion
} // namespace larp
