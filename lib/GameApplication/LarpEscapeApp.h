#pragma once

#include "GameApplication/IGameSnapshotSink.h"
#include "GamePorts/IRuntimeService.h"
#include "Gamestate.h"

namespace larp {
namespace application {
// All dependencies outlive this application. update() has one loop owner.
class LarpEscapeApp {
  public:
    LarpEscapeApp(ReichstagGame &game, ports::IRuntimeService &audio,
                  ports::IRuntimeService &communication, IGameSnapshotSink &snapshots)
        : game_(game), audio_(audio), communication_(communication), snapshots_(snapshots) {}
    LarpEscapeApp(const LarpEscapeApp &) = delete;
    LarpEscapeApp &operator=(const LarpEscapeApp &) = delete;
    // Called only by the update() loop owner before its first update. Closed
    // preflight only: setup() workers remain active and no device is granted.
    bool fenceUnstartedGameForDiagnostics();
    bool gameFencedForDiagnostics() const { return diagnosticFence_; }
    void update();

  private:
    ReichstagGame &game_;
    ports::IRuntimeService &audio_;
    ports::IRuntimeService &communication_;
    IGameSnapshotSink &snapshots_;
    bool gameTicked_ = false;
    bool diagnosticFence_ = false;
};
} // namespace application
} // namespace larp
