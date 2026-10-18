#include <Arduino.h>
#include "LedOutput.h"
#include <vector>
#include <stdexcept>
#include <iostream>
std::uint32_t testNow = 0;
TestSerial Serial;
using namespace larp::led;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct FakeBus final : LedBus {
    bool busy = false;
    std::uint32_t completeAt = 0;
    std::vector<Frame> frames;
    bool begin() override { return true; }
    bool submit(const Frame &frame) override {
        check(!busy, "overlapping transfer"); busy = true; completeAt = testNow + 3;
        frames.push_back(frame); return true;
    }
    TxResult poll() override {
        check(busy, "poll without transfer");
        if (testNow < completeAt) return TxResult::Busy;
        busy = false; return TxResult::Complete;
    }
    bool reset() override { busy = false; return true; }
} fake;
bool black(const Frame &frame) {
    for (const auto &p : frame) if (p.red || p.green || p.blue) return false;
    return true;
}
}
namespace larp { namespace led { LedBus &cameraLabBus() { return fake; }}}
#include "../src/CameraLabMain.cpp"
int main(int argc, char **argv) {
    try {
        check(argc == 2, "need case");
        setup(); for (unsigned i = 0; i < 10; ++i) loop();
        check(engine.snapshot().ready && black(fake.frames.back()), "boot not confirmed dark");
        const std::string name = argv[1];
        Serial.send("CAL 0123456789abcdef"); loop();
        if (name == "sequence") {
            for (unsigned i = 0; i < 45000; ++i) {
                if (i % 200 == 0) Serial.send("KEEP 0123456789abcdef");
                loop();
            }
            check(!running && !stopPending && black(fake.frames.back()), "sequence did not finish dark");
            // Initial black, 133 fixed calibration stages, final fresh black.
            check(fake.frames.size() == 135, "missing/duplicate calibration frame");
            for (unsigned s = 0; s < 133; ++s) {
                const auto expected = camera::step(s);
                const auto &frame = fake.frames[s+1];
                for (unsigned i = 0; i < 23; ++i) {
                    Pixel pixel;
                    if (expected.index == -1 || expected.index == int(i)) pixel = expected.pixel;
                    check(frame[i].red == pixel.red && frame[i].green == pixel.green &&
                          frame[i].blue == pixel.blue, "wrong index/color in emitted calibration frame");
                }
            }
        } else if (name == "watchdog") {
            for (unsigned i = 0; i < 1700; ++i) loop();
            check(!running && !stopPending && black(fake.frames.back()), "watchdog failed dark");
            check(Serial.output.find("watchdog") != std::string::npos, "watchdog not reported");
        } else if (name == "burst" || name == "ripple" || name == "twinkle") {
            Serial.send("STOP"); for (unsigned i=0;i<10;++i) loop();
            Serial.send("RUN 0123456789abcdef "+name);
            for (unsigned i=0;i<8000;++i) {
                if (i%200==0) Serial.send("KEEP 0123456789abcdef");
                loop();
                if (i==5000) check(running && !black(fake.frames.back()),
                                   "effect ended before its actual output duration");
            }
            check(!running && !stopPending && black(fake.frames.back()),"effect did not finish dark");
        } else if (name == "resonance" || name == "resonance-transitions" || name == "resonance-colors" || name == "resonance-watchdog") {
            Serial.send("STOP"); for (unsigned i=0;i<10;++i) loop();
            Serial.send("RUN 0123456789abcdef "+(name == "resonance-watchdog" ? std::string("resonance") : name));
            unsigned observed = 0;
            const unsigned zones[23] = {1,1,1,1,1,1,1,3,3,3,3,3,3,2,2,2,2,2,2,0,0,0,0};
            for (unsigned i=0;i<60000;++i) {
                if (i%200==0 && (name != "resonance-watchdog" || i < 8000)) Serial.send("KEEP 0123456789abcdef");
                loop();
                if (effectMode && resonanceMode && !fake.busy && testNow-resonanceAt>100) {
                    const auto current = camera::resonanceStep(resonanceNumber, resonanceTransitions);
                    if (current.effect != Effect::Resonance) continue;
                    observed |= 1U << resonanceNumber;
                    for (unsigned led=0;led<23;++led) {
                        const auto &pixel = fake.frames.back()[led];
                        const bool active = current.valid && current.mask<=15 && (current.mask&(1U<<zones[led]));
                        check((pixel.blue>0)==active,"synthetic Wake lit the wrong region");
                        if (active) {
                            check(pixel.blue>=4 && pixel.blue<=24 && pixel.red==pixel.blue/6 && pixel.green==pixel.blue*2/3,"wrong resonance color");
                            if (current.mask==15 && testNow-resonanceAt>850) check(pixel.blue==24,"merge did not settle");
                        }
                    }
                }
            }
            check(!running && !stopPending && black(fake.frames.back()),"resonance lab did not finish dark");
            if (name == "resonance") check(observed==65535,"not all 16 input masks observed");
            if (name == "resonance-watchdog") check(Serial.output.find("watchdog")!=std::string::npos,"resonance watchdog missing");
            if (name == "resonance-transitions") check(observed & (1U<<13),"resonance restart not reached");
        } else if (name == "stop-inflight") {
            // This transfer belongs to the direct calibration path, not engine.
            Serial.send("STOP"); for (unsigned i = 0; i < 10; ++i) loop();
            check(!stopPending && black(fake.frames.back()), "stop direct failed");
            Serial.send("RUN 0123456789abcdef burst");
            for (unsigned i = 0; i < 4300; ++i) {
                if (i % 200 == 0) Serial.send("KEEP 0123456789abcdef");
                loop();
            }
            Serial.send("STOP"); for (unsigned i = 0; i < 10; ++i) loop();
            check(!stopPending && black(fake.frames.back()), "stop engine failed");
        } else throw std::runtime_error("unknown case");
        check(!camera::validToken("bad") && !camera::validToken("0123456789abcdef0") &&
              camera::validToken("0123456789abcdef"), "token validation");
        std::cout << name << " passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
