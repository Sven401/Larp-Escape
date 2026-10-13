#include "LarpEscapeApp.h"

namespace larp {
namespace application {
void LarpEscapeApp::update() {
    audio_.service();
    communication_.service();
    game_.machine.run();
    snapshots_.publish(game_.snapshot());
}
} // namespace application
} // namespace larp
