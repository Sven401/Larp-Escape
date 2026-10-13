#pragma once
#include "../FreeRtosPeerProofGrantRuntime/FreeRtosPeerProofGrantRuntime.h"
#include "Companion/PeerProofReceiver.h"
namespace larp {
namespace companion {
enum class PeerAuthDispatch : std::uint8_t {
    Empty,
    ProofProcessed,
    GrantObserved,
    ReadPosted,
    Rejected,
    Unavailable
};
// Same existing crypto worker. Four proof copies and four reads, no new task/timer.
class FreeRtosPeerAuthRuntime : public IPeerAuthIngress {
  public:
    FreeRtosPeerAuthRuntime(Esp32GattsSource &source, PeerProofOwner &owner,
                            PeerProofReceiver &receiver, FreeRtosPeerProofGrantRuntime &grant,
                            const IProfileImportClock &clock)
        : source_(source), owner_(owner), receiver_(receiver), grant_(grant), clock_(clock) {}
    FreeRtosPeerAuthRuntime(const FreeRtosPeerAuthRuntime &) = delete;
    FreeRtosPeerAuthRuntime &operator=(const FreeRtosPeerAuthRuntime &) = delete;
    bool beginWorker() noexcept;
    bool workerNow() const noexcept override;
    bool copyProof(const PeerProofWriteRequest &) noexcept override;
    bool copyResultRead(const PeerChallengeReadRequest &) noexcept override;
    // <=4 stale proofs/reads; <=1 current proof OR read OR grant observation.
    PeerAuthDispatch service() noexcept;

  private:
    Esp32GattsSource &source_;
    PeerProofOwner &owner_;
    PeerProofReceiver &receiver_;
    FreeRtosPeerProofGrantRuntime &grant_;
    const IProfileImportClock &clock_;
    QueueHandle_t proofs_ = nullptr, reads_ = nullptr;
    StaticQueue_t proofControl_ = {}, readControl_ = {};
    alignas(PeerProofWriteRequest) std::uint8_t
        proofStorage_[PeerAuthLimits::Depth * sizeof(PeerProofWriteRequest)] = {};
    alignas(PeerChallengeReadRequest) std::uint8_t
        readStorage_[PeerAuthLimits::Depth * sizeof(PeerChallengeReadRequest)] = {};
    TaskHandle_t worker_ = nullptr;
    PeerAuthReadReply latest_, pin_;
    bool attempted_ = false, taking_ = false, haveResult_ = false, havePin_ = false,
         waiting_ = false, terminal_ = false;
    bool current(const PeerProofWriteRequest &) const noexcept;
    bool current(const PeerChallengeReadRequest &) const noexcept;
    bool resultCurrent(const PeerAuthReadReply &, const PeerChallengeReadRequest &) const noexcept;
};
} // namespace companion
} // namespace larp
