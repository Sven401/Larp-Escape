#include "LarpEscapeApp.h"

namespace larp {
namespace application {
bool LarpEscapeApp::fenceUnstartedGameForDiagnostics() {
    if (gameTicked_ || diagnosticFence_)
        return false;
    diagnosticFence_ = true;
    return true;
}
bool LarpEscapeApp::fenceObservedStandbyAppLoopForDiagnostics() {
    if (diagnosticFence_ || settledStandbyTicks_ < 2U)
        return false;
    diagnosticFence_ = true;
    return true;
}
void LarpEscapeApp::update() {
    if (diagnosticFence_)
        return;
    // Close the preflight window before any dependency can run game IO.
    gameTicked_ = true;
    audio_.service();
    communication_.service();
    game_.machine.run();
    const GameSnapshot snapshot = game_.snapshot();
    // A selected state may still have its entry action pending until the next
    // run(). Two observations exclude that transition frame, but do not prove
    // physical owner quiescence.
    if (snapshot.phase == GamePhase::Standby && !snapshot.rolePresent &&
        !snapshot.audioBusy) {
        if (settledStandbyTicks_ < 2U)
            ++settledStandbyTicks_;
    } else {
        settledStandbyTicks_ = 0U;
    }
    snapshots_.publish(snapshot);
}
} // namespace application
} // namespace larp
