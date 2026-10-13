#pragma once
#include "../FreeRtosPeerChallengeReadRuntime/FreeRtosPeerChallengeReadRuntime.h"
namespace larp {
namespace companion {
enum class PeerChallengeIssueDispatch : std::uint8_t { Empty, Processed, Unavailable };
struct PeerChallengeIssueResult {
    PeerChallengeIssueDispatch dispatch = PeerChallengeIssueDispatch::Empty;
    PeerProofOutcome outcome = PeerProofOutcome::InvalidCredential;
};
// One SDK producer, same existing crypto worker as grant/read. No new task/timer.
// Write Commands have no ATT response; only an original challenge read proves issue.
class FreeRtosPeerChallengeIssueRuntime : public IPeerChallengeIssueIngress {
  public:
    FreeRtosPeerChallengeIssueRuntime(Esp32GattsSource &source, PeerProofOwner &owner,
                                      FreeRtosPeerProofGrantRuntime &grant,
                                      const FreeRtosPeerRegistryRuntime &registry,
                                      const IProfileImportClock &clock)
        : source_(source), owner_(owner), grant_(grant), registry_(registry), clock_(clock) {}
    FreeRtosPeerChallengeIssueRuntime(const FreeRtosPeerChallengeIssueRuntime &) = delete;
    FreeRtosPeerChallengeIssueRuntime &
    operator=(const FreeRtosPeerChallengeIssueRuntime &) = delete;
    bool beginWorker() noexcept;
    bool copyIssue(const PeerChallengeIssueRequest &) noexcept override;
    PeerChallengeIssueResult service() noexcept;

  private:
    Esp32GattsSource &source_;
    PeerProofOwner &owner_;
    FreeRtosPeerProofGrantRuntime &grant_;
    const FreeRtosPeerRegistryRuntime &registry_;
    const IProfileImportClock &clock_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(PeerChallengeIssueRequest) std::uint8_t
        storage_[PeerChallengeIssueLimits::Depth * sizeof(PeerChallengeIssueRequest)] = {};
    TaskHandle_t worker_ = nullptr;
    bool attempted_ = false, taking_ = false;
    bool workerNow() const noexcept;
    bool current(const PeerChallengeIssueRequest &) const noexcept;
};
} // namespace companion
} // namespace larp
