#include "ClosedDiagnosticArbiter.h"

namespace larp { namespace application {
bool ClosedDiagnosticArbiter::requestClosedHandoff(std::uint32_t ticket, bool preflight) {
    if (state_ != ClosedHandoff::Game || ticket == 0U)
        return false;
    if (!(preflight ? app_.fenceUnstartedGameForDiagnostics()
                    : app_.fenceObservedStandbyAppLoopForDiagnostics()))
        return false;
    // The game loop is fenced before the timer or light worker can be called.
    // Even a timer failure cannot reopen the already fenced game.
    ticket_ = ticket;
    state_ = ClosedHandoff::Fault;
    const bool armed = timer_.arm(ticket, lights_);
    const bool parked = lights_.requestPark(ticket);
    if (armed && parked)
        state_ = ClosedHandoff::Fenced;
    return armed && parked;
}
}} // namespace larp::application
