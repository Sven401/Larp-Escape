#ifdef LARP_LED_CAMERA_LAB
#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include "CameraSequence.h"

namespace larp { namespace led { LedBus &cameraLabBus(); }}
using namespace larp::led;
namespace {
char token[17] = "0000000000000000";
unsigned stage = 0, frameNumber = 0;
std::uint32_t lastKeep = 0, started = 0, appliedAt = 0;
bool running = false, effectMode = false, waiting = false, stopPending = false;
bool directOutstanding = false;
Effect selected = Effect::Off;

struct TraceBus final : LedBus {
    LedBus &bus = cameraLabBus();
    Frame pending{};
    std::uint32_t submitted = 0;
    bool inFlight = false;
    bool begin() override { return bus.begin(); }
    bool submit(const Frame &frame) override {
        if (inFlight || !bus.submit(frame)) return false;
        pending = frame; submitted = millis(); inFlight = true; return true;
    }
    TxResult poll() override {
        const auto result = bus.poll();
        if (result == TxResult::Complete && inFlight) {
            appliedAt = millis();
            Serial.printf("F %s %u %u %lu %lu ", token, stage, frameNumber++,
                          (unsigned long)submitted, (unsigned long)appliedAt);
            const char *digits = "0123456789abcdef";
            for (const auto &p : pending) for (const auto c : {p.red, p.green, p.blue}) {
                Serial.write(digits[c >> 4]); Serial.write(digits[c & 15]);
            }
            Serial.println();
            inFlight = false;
        }
        return result;
    }
    bool reset() override { inFlight = false; return bus.reset(); }
} bus;
std::uint32_t clockNow() { return millis(); }
LedOutput engine(bus, clockNow);

void stop(const char *reason) {
    running = false; waiting = false; effectMode = false;
    // Let the engine drain a prior direct transfer, then request a fresh black.
    engine.stop(); stopPending = true;
    Serial.printf("STOP %s %s\n", token, reason);
}
void command(char *line) {
    if (std::strcmp(line, "HELLO") == 0) {
        Serial.printf("LARP-CAMERA/1 23 32 GPIO15 %lu %s\n",
                      (unsigned long)ESP.getSketchSize(), ESP.getSketchMD5().c_str()); return;
    }
    if (std::strcmp(line, "STOP") == 0) { stop("host"); return; }
    char *context = nullptr;
    char *verb = strtok_r(line, " ", &context);
    char *id = strtok_r(nullptr, " ", &context);
    if (!verb || !id || !camera::validToken(id)) return;
    if (std::strcmp(verb, "KEEP") == 0) {
        if (std::strcmp(id, token) == 0 && !strtok_r(nullptr, " ", &context)) lastKeep = millis();
        return;
    }
    if (running || stopPending || bus.inFlight || !engine.snapshot().ready) {
        Serial.println("BUSY"); return;
    }
    if (std::strcmp(verb, "CAL") == 0) {
        if (strtok_r(nullptr, " ", &context)) return;
        selected = Effect::Off;
    } else if (std::strcmp(verb, "RUN") == 0) {
        char *effect = strtok_r(nullptr, " ", &context);
        if (!effect || strtok_r(nullptr, " ", &context)) return;
        selected = std::strcmp(effect,"twinkle") == 0 ? Effect::Twinkle :
                   std::strcmp(effect,"burst") == 0 ? Effect::Burst :
                   std::strcmp(effect,"ripple") == 0 ? Effect::Ripple : Effect::Off;
        if (selected == Effect::Off) return;
    } else return;
    std::strcpy(token, id); frameNumber = stage = 0;
    started = lastKeep = millis(); running = true; waiting = effectMode = false;
    Serial.printf("BEGIN %s %lu\n", token, (unsigned long)started);
}
void usb() {
    static char line[80]; static unsigned used = 0; static bool overflow = false;
    for (unsigned i = 0; i < 96 && Serial.available(); ++i) {
        const char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            line[used] = 0;
            if (!overflow) command(line);
            used = 0; overflow = false;
        } else if (used < sizeof(line)-1) line[used++] = c;
        else overflow = true;
    }
}
}
void setup() {
    Serial.begin(115200);
    if (!engine.begin()) { Serial.println("FAULT init"); return; }
    stopPending = true;
    Serial.println("LARP-CAMERA/1 23 32 GPIO15");
}
void loop() {
    usb();
    if (running && (millis()-lastKeep > 1500 || millis()-started > 90000)) stop("watchdog");
    if (stopPending) {
        // A direct lab frame is not owned by the engine. Drain it first.
        if (directOutstanding && bus.inFlight) {
            const auto result = bus.poll();
            if (result == TxResult::Busy) { delay(1); return; }
            if (result == TxResult::Failed) bus.reset();
            directOutstanding = false;
        }
        engine.service();
        if (!engine.snapshot().pending && !bus.inFlight) {
            stopPending = false; Serial.printf("DONE %s\n", token);
        }
        delay(1); return;
    }
    if (!running) { delay(1); return; }
    if (effectMode) {
        engine.service();
        if (engine.snapshot().fault) stop("engine-fault");
        else if (millis()-appliedAt > 1000 && engine.snapshot().completed && engine.snapshot().off)
            stop("finished");
        else if (selected == Effect::Twinkle && millis()-started >= 7500) stop("finished");
        delay(1); return;
    }
    const auto current = selected == Effect::Off ? camera::step(stage) :
        stage < 6 ? camera::step(stage) :
        stage == 6 ? camera::dark(1000) : camera::Step{-2, {}, 0, selected};
    if (current.index == -3) { stop("finished"); return; }
    if (current.index == -2) {
        if (!engine.start(current.effect)) { stop("engine-rejected"); return; }
        effectMode = true; engine.service(); delay(1); return;
    }
    if (!waiting) {
        Frame frame{};
        for (unsigned i = 0; i < PixelCount; ++i)
            if (current.index == -1 || int(i) == current.index) frame[i] = current.pixel;
        if (!bus.submit(frame)) { stop("bus-submit"); return; }
        directOutstanding = true;
        waiting = true;
    }
    if (bus.inFlight) {
        const auto result = bus.poll();
        if (result == TxResult::Failed) { bus.reset(); stop("bus-poll"); }
        else if (result == TxResult::Busy && millis()-bus.submitted >= 2000) { bus.reset(); stop("bus-timeout"); }
        if (result != TxResult::Busy) directOutstanding = false;
    } else if (millis()-appliedAt >= current.duration) { ++stage; waiting = false; }
    delay(1);
}
#endif
