#include "wled.h"
#include "WledLedOutput.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

std::uint32_t fakeNow = 100;
bool turnOnAtBoot = true, transitionActive = false, nightlightActive = false,
     gammaCorrectCol = false, gammaCorrectBri = false, offMode = false;
std::uint8_t bootPreset = 0, bri = 0, briT = 0, briOld = 0, briS = 0, briLast = 0,
             briMultiplier = 100, realtimeMode = 0, realtimeOverride = 0, errorFlag = 0;
float gammaCorrectVal = 2.2f;
std::vector<BusConfig> busConfigs;
namespace BusManager { std::vector<Bus> buses; }
FakeStrip strip;
bool rippleAllocationFails = false;
// Models only WLED's runtime storage lifecycle, including adversarial random
// spawning on *every* empty slot. Native pixel propagation is not mocked as proof.
void mode_ripple() {
    if (rippleAllocationFails) { SEGENV.data = nullptr; return; }
    SEGENV.data = SEGENV.storage.data();
    if (SEGENV.call == 0) SEGENV.storage.fill(0);
    for (unsigned i = 0; i < 6; ++i) {
        auto &state = SEGENV.data[i*4];
        if (state) { unsigned next = state + 9; state = next > 254 ? 0 : next; }
        else state = 1;
    }
}
using larp::led::Effect;
using larp::led::WledLedOutput;

void completeFrame(WledLedOutput &output) {
    strip.frame();
    output.service(); // Transfer still in flight: no premature application.
    strip.updating = false;
    output.service();
}
void ready(WledLedOutput &output) {
    assert(output.begin());
    assert(!output.snapshot().ready && !output.snapshot().off);
    assert(!output.start(Effect::Twinkle));
    output.service();
    completeFrame(output);
    assert(output.snapshot().ready && output.snapshot().off && !output.snapshot().pending);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string test = argv[1];
    WledLedOutput output;
    if (test == "boot") {
        assert(output.begin());
        output.service();
        assert(!output.snapshot().ready);
        strip.frame();
        output.service();
        assert(!output.snapshot().ready && output.snapshot().pending);
        strip.updating = false;
        output.service();
        assert(output.snapshot().ready && output.snapshot().off);
    } else {
        ready(output);
        if (test == "latest_stop") {
            assert(output.start(Effect::Twinkle));
            assert(output.stop());
            output.service();
            assert(strip.segment.mode == FX_MODE_STATIC && strip.brightness == 0);
            completeFrame(output);
            assert(output.snapshot().off && output.snapshot().appliedRevision == 3);
        } else if (test == "stop_inflight") {
            assert(output.start(Effect::Twinkle)); output.service(); strip.frame();
            assert(output.stop()); output.service();
            assert(strip.segment.mode == FX_MODE_TWINKLEFOX);
            assert(output.snapshot().appliedRevision == 1 && output.snapshot().pending);
            strip.updating = false; output.service();
            assert(strip.segment.mode == FX_MODE_STATIC);
            assert(output.snapshot().appliedRevision == 1);
            completeFrame(output);
            assert(output.snapshot().appliedRevision == 3 && output.snapshot().off);
        } else if (test == "same_restart") {
            assert(output.start(Effect::Burst)); output.service(); completeFrame(output);
            const unsigned resets = strip.restarts;
            fakeNow += 700;
            assert(output.start(Effect::Burst)); output.service();
            assert(strip.restarts == resets + 1 && strip.segment.reset);
            completeFrame(output);
            fakeNow += 1000; output.service();
            assert(output.snapshot().requested == Effect::Burst && !output.snapshot().completed);
        } else if (test == "burst_completion") {
            assert(output.start(Effect::Burst)); output.service(); completeFrame(output);
            const auto token = output.snapshot().revision;
            fakeNow += WledLedOutput::BurstMilliseconds;
            output.service();
            assert(output.snapshot().pending && !output.snapshot().completed);
            assert(output.snapshot().applied == Effect::Burst);
            assert(strip.segment.mode == FX_MODE_STATIC && strip.brightness == 0);
            completeFrame(output);
            assert(output.snapshot().completed && output.snapshot().off);
            assert(output.snapshot().appliedRevision == token && output.snapshot().revision == token);
        } else if (test == "ripple_once") {
            assert(output.start(Effect::Ripple)); output.service(); completeFrame(output);
            assert(strip.segment.storage[0] == 1);
            std::uint16_t origin = 0; std::memcpy(&origin, strip.segment.storage.data()+2, sizeof(origin));
            assert(origin == 11);
            for (unsigned frame = 0; frame < 35 && !output.snapshot().completed; ++frame) {
                fakeNow += 33; completeFrame(output);
                for (unsigned slot = 1; slot < 6; ++slot) assert(strip.segment.storage[slot*4] == 0);
            }
            assert(output.snapshot().completed && output.snapshot().off);
        } else if (test == "ripple_replacement") {
            assert(output.start(Effect::Ripple)); output.service(); completeFrame(output);
            completeFrame(output); assert(strip.segment.storage[0] == 10);
            assert(output.start(Effect::Ripple)); output.service(); completeFrame(output);
            assert(strip.segment.storage[0] == 1 && !output.snapshot().completed);
        } else if (test == "allocation_failure") {
            rippleAllocationFails = true;
            assert(output.start(Effect::Ripple)); output.service(); completeFrame(output);
            assert(output.snapshot().fault && !output.snapshot().ready);
            assert(output.snapshot().applied != Effect::Ripple);
            completeFrame(output);
            assert(output.snapshot().off);
        } else if (test == "missing_frame") {
            assert(output.start(Effect::Twinkle)); output.service();
            fakeNow += 2001; output.service();
            assert(output.snapshot().fault && !output.snapshot().ready);
            assert(output.snapshot().requested == Effect::Off);
            assert(output.snapshot().appliedRevision == 1);
            completeFrame(output); assert(output.snapshot().off && output.snapshot().fault);
        } else if (test == "bus_never_idle") {
            strip.updating = true;
            assert(output.start(Effect::Twinkle)); output.service();
            fakeNow += 2001; output.service();
            assert(output.snapshot().fault && !output.snapshot().ready);
            assert(output.snapshot().appliedRevision == 1);
        } else if (test == "black_verified") {
            assert(output.start(Effect::Twinkle)); output.service(); completeFrame(output);
            assert(!output.snapshot().off);
            assert(output.stop()); output.service();
            strip.corruptBlack = true; completeFrame(output);
            assert(output.snapshot().fault && !output.snapshot().off);
            assert(output.snapshot().applied == Effect::Twinkle);
        } else if (test == "burst_missed_window") {
            assert(output.start(Effect::Burst)); output.service();
            fakeNow += WledLedOutput::BurstMilliseconds; output.service();
            assert(output.snapshot().fault && output.snapshot().requested == Effect::Off);
            assert(strip.segment.mode == FX_MODE_STATIC);
            assert(output.snapshot().appliedRevision == 1);
        } else if (test == "replace_automatic_off") {
            assert(output.start(Effect::Burst)); output.service(); completeFrame(output);
            fakeNow += WledLedOutput::BurstMilliseconds; output.service();
            assert(output.snapshot().pending && output.snapshot().requested == Effect::Off);
            assert(output.start(Effect::Twinkle)); output.service(); completeFrame(output);
            assert(output.snapshot().applied == Effect::Twinkle && !output.snapshot().completed);
            fakeNow += 1800; output.service();
            assert(output.snapshot().requested == Effect::Twinkle);
        } else if (test == "hardware_fault") {
            BusManager::buses[0].valid = false; output.service();
            assert(output.snapshot().fault && !output.snapshot().ready);
            assert(!output.start(Effect::Twinkle));
        } else if (test == "clock_wrap") {
            fakeNow = 0xFFFFFF00U;
            assert(output.start(Effect::Burst)); output.service(); completeFrame(output);
            fakeNow += WledLedOutput::BurstMilliseconds; output.service(); completeFrame(output);
            assert(output.snapshot().completed && output.snapshot().off && !output.snapshot().fault);
        } else { assert(false); }
    }
    std::cout << test << " passed\n";
}
