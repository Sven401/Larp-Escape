#include "LarpEscapeApp.h"

namespace larp {
namespace application {
bool LarpEscapeApp::fenceUnstartedGameForDiagnostics() {
    if (gameTicked_ || diagnosticFence_)
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
    snapshots_.publish(game_.snapshot());
}
} // namespace application
} // namespace larp
