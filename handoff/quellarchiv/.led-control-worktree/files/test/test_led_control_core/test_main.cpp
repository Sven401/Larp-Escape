#include "CompanionIntegration/LedControlV1.h"
#include <cassert>

using namespace larp::ledcontrol;

int main() {
    LedControlV1 control;
    Selection choice;
    choice.red = 16;
    choice.effect = Effect::Chase;
    assert(LedControlV1::valid(choice));
    assert(control.start(1, 1, choice, 100).phase == Phase::Running);
    assert(control.snapshot().outcome == Outcome::Accepted);
    assert(!control.offDue(3099));
    assert(control.stop(2, 1, 1000).outcome == Outcome::Stopped);
    assert(control.offDue(1000));
    assert(!control.blackSubmitted(2, 1000).blackSubmitted);
    assert(control.blackSubmitted(1, 1000).blackSubmitted);
    assert(control.snapshot().phase == Phase::OffSubmitted);
    assert(control.start(3, 2, choice, 1100).phase == Phase::Running);
    assert(control.snapshot().runId == 2);
    assert(control.start(3, 3, choice, 1200).outcome == Outcome::RejectedStale);
    assert(control.stop(4, 1, 1200).outcome == Outcome::RejectedStale);
    assert(control.offDue(4100));
    assert(control.blackSubmitted(2, 4100).outcome == Outcome::Expired);
    assert(control.start(5, 3, choice, 4200).phase == Phase::Running);
    assert(control.fault(3).phase == Phase::Fault);
    assert(control.offDue(4200));
    assert(control.blackSubmitted(3, 4200).phase == Phase::Fault);
    assert(control.start(6, 4, choice, 4300).outcome == Outcome::RejectedBusy);

    Selection invalid = choice;
    invalid.first = 22;
    invalid.count = 2;
    assert(!LedControlV1::valid(invalid));
    LedControlV1 another;
    assert(another.start(1, 1, invalid, 100).outcome == Outcome::RejectedInvalid);
    assert(another.snapshot().phase == Phase::Idle);

    // Millisecond counter wraps while the independent off deadline remains due.
    LedControlV1 wrapping;
    choice.durationMs = 500;
    assert(wrapping.start(1, 1, choice, 0xfffffff0U).phase == Phase::Running);
    assert(!wrapping.offDue(0x000001e3U));
    assert(wrapping.offDue(0x000001e4U));
    return 0;
}
