#pragma once
#include "../GattsWriteAdmission/Esp32GattsSource.h"
#include "Companion/PeerRegistry.h"
namespace larp {
namespace companion {
struct PeerRegistryWorkerLimits {
    static const std::uint32_t OperationMilliseconds = 30000;
};
// Process lifetime: one IO worker polls, BTU applies copied roles, crypto workers
// read only the protected public snapshot. No NVS/crypto/waits under a mux.
class FreeRtosPeerRegistryRuntime : public IPeerRegistryRuntime {
  public:
    FreeRtosPeerRegistryRuntime(Esp32GattsSource &source, IAuthorityPublisher &authority,
                                const IProfileImportClock &clock)
        : source_(source), authority_(authority), clock_(clock) {}
    FreeRtosPeerRegistryRuntime(const FreeRtosPeerRegistryRuntime &) = delete;
    FreeRtosPeerRegistryRuntime &operator=(const FreeRtosPeerRegistryRuntime &) = delete;
    bool begin(const Identity &) noexcept;
    PeerRegistryRuntimeResult close() noexcept override;
    PeerRegistryRuntimeResult replaceClosed(const PeerRegistrySnapshot &) noexcept override;
    void fence() noexcept override;
    bool snapshot(PeerRegistrySnapshot &) const noexcept;
    bool credential(const Identity &, PeerCredential &) const noexcept;
    bool credentialCurrent(const PeerCredential &) const noexcept;

  private:
    Esp32GattsSource &source_;
    IAuthorityPublisher &authority_;
    const IProfileImportClock &clock_;
    TaskHandle_t worker_ = nullptr;
    Identity device_;
    PeerRegistryActorOrigin closed_;
    PeerRegistryRoleJob job_, publishedJob_;
    PeerRegistrySnapshot published_;
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    std::uint32_t sequence_ = 0, started_ = 0;
    enum class Phase : std::uint8_t { Idle, Closing, Closed, Installing };
    Phase phase_ = Phase::Idle;
    bool attempted_ = false, busy_ = false, ready_ = false, fenced_ = false;
    bool availableWorker() const noexcept;
    PeerRegistryRuntimeResult poll() noexcept;
    PeerRegistryRuntimeResult reject() noexcept;
    bool readPublished(PeerRegistrySnapshot &) const noexcept;
};
} // namespace companion
} // namespace larp
