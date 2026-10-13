#pragma once
#include "../FreeRtosPeerProofGrantRuntime/FreeRtosPeerProofGrantRuntime.h"
namespace larp {
namespace companion {
enum class PeerChallengeReadDispatch : std::uint8_t { Empty, Posted, Rejected, Unavailable };
// Sole crypto worker owns proof getter and queue consumer. The SDK actor copies
// <=one request; four static entries, zero wait. No new task/timer/SDK startup.
class FreeRtosPeerChallengeReadRuntime : public IPeerChallengeReadIngress {
  public:
    FreeRtosPeerChallengeReadRuntime(Esp32GattsSource &source, PeerProofOwner &owner,
                                     FreeRtosPeerProofGrantRuntime &grant,
                                     const IProfileImportClock &clock)
        : source_(source), owner_(owner), grant_(grant), clock_(clock) {}
    FreeRtosPeerChallengeReadRuntime(const FreeRtosPeerChallengeReadRuntime &) = delete;
    FreeRtosPeerChallengeReadRuntime &operator=(const FreeRtosPeerChallengeReadRuntime &) = delete;
    bool beginWorker() noexcept;
    bool copyRead(const PeerChallengeReadRequest &) noexcept override;
    bool workerNow() const noexcept override;
    // <=four stale items + one current request and <=one SDK response post.
    PeerChallengeReadDispatch service() noexcept;

  private:
    Esp32GattsSource &source_;
    PeerProofOwner &owner_;
    FreeRtosPeerProofGrantRuntime &grant_;
    const IProfileImportClock &clock_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(PeerChallengeReadRequest) std::uint8_t
        storage_[PeerChallengeReadLimits::Depth * sizeof(PeerChallengeReadRequest)] = {};
    TaskHandle_t worker_ = nullptr;
    bool attempted_ = false, taking_ = false;
    bool current(const PeerChallengeReadRequest &) const noexcept;
};
} // namespace companion
} // namespace larp
