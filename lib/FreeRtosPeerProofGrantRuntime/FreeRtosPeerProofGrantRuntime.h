#pragma once
#include "../FreeRtosPeerRegistryRuntime/FreeRtosPeerRegistryRuntime.h"
#include "Companion/PeerProofGrant.h"
namespace larp {
namespace companion {
// Process lifetime. One crypto worker uses the mutable proof owner and this
// handoff. Context reads use immutable boot/protected Source and registry copies.
class FreeRtosPeerProofGrantRuntime : public IPeerProofContext {
  public:
    FreeRtosPeerProofGrantRuntime(Esp32GattsSource &source,
                                  const FreeRtosPeerRegistryRuntime &registry,
                                  const IProfileImportClock &clock, const BootIdentity &boot)
        : source_(source), registry_(registry), clock_(clock), boot_(boot) {}
    FreeRtosPeerProofGrantRuntime(const FreeRtosPeerProofGrantRuntime &) = delete;
    FreeRtosPeerProofGrantRuntime &operator=(const FreeRtosPeerProofGrantRuntime &) = delete;
    bool beginWorker() noexcept;
    bool workerNow() const noexcept {
        return worker_ && worker_ == xTaskGetCurrentTaskHandle();
    }
    bool originalLive(const PeerProofBinding &) const noexcept override;
    bool credentialCurrent(const PeerCredential &) const noexcept override;
    // The proof owner is read only by its sole crypto worker. Never accept
    // untrusted JSON flags or an arbitrary client-created verified proof.
    PeerProofGrantState submitVerified(PeerProofOwner &, const PeerProofBinding &) noexcept;
    PeerProofGrantState service(TrustedAuthority &) noexcept;
    void cancel() noexcept;
    bool job(PeerProofGrantJob &) const noexcept;

  private:
    Esp32GattsSource &source_;
    const FreeRtosPeerRegistryRuntime &registry_;
    const IProfileImportClock &clock_;
    const BootIdentity boot_;
    TaskHandle_t worker_ = nullptr;
    PeerProofGrantJob job_;
    std::uint32_t sequence_ = 0;
    bool attempted_ = false, busy_ = false, hasJob_ = false;
};
} // namespace companion
} // namespace larp
