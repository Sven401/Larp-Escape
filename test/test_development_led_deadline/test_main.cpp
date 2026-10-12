#include "CompanionIntegration/DevelopmentLedDeadlineCore.h"

#include <cassert>
#include <cstdio>

using larp::diagnostic::DevelopmentLedDeadlineCore;

int main() {
    unsigned cases = 0;
    DevelopmentLedDeadlineCore core;
    assert(core.ready());
    assert(core.arm(1, 1000000));
    assert(!core.arm(2, 1000001));
    assert(!core.requestStop(2));
    assert(core.timerFired(1));
    assert(core.needsBlack());
    assert(core.blackSubmitted(1, 1450000));
    assert(core.blackSubmittedFor(1) && !core.missedFor(1));
    assert(core.ready() && core.lastCompletedTicket() == 1);
    ++cases;

    // A second test action is possible only after the previous timer fired
    // and its off worker completed. Stale events cannot affect the new ticket.
    assert(core.arm(2, 2000000));
    assert(!core.timerFired(1));
    assert(core.requestStop(2));
    assert(core.blackSubmitted(2, 2200000));
    assert(!core.ready()); // The timer still owns its scheduled callback.
    assert(!core.arm(3, 2200001));
    assert(core.timerFired(2));
    assert(core.ready() && core.lastCompletedTicket() == 2);
    assert(core.arm(3, 3000000));
    ++cases;

    assert(core.timerFired(3));
    assert(!core.blackSubmitted(2, 3400000));
    assert(core.needsBlack());
    assert(!core.blackSubmitted(3, 3500001));
    assert(core.faulted() && core.missedFor(3) && !core.ready());
    assert(!core.arm(4, 4000000));
    ++cases;

    DevelopmentLedDeadlineCore failure;
    assert(failure.arm(1, 1000000));
    failure.fault(1); // Timer start or hardware driver failure is permanent.
    assert(failure.faulted() && failure.missedFor(1));
    assert(!failure.requestStop(1) && !failure.arm(2, 2000000));
    ++cases;

    std::printf("%u development LED deadline cases passed\n", cases);
}
