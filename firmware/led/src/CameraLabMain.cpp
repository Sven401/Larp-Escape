#ifdef LARP_LED_CAMERA_LAB
#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include "CameraSequence.h"
#include "ResonanceCameraSequence.h"
#include "CameraLabController.h"
#ifdef LARP_LED_CAMERA_RSSI_LAB
#include "CameraLabRssi.h"
#include "EspNowRadio.h"
#endif

namespace larp { namespace led { LedBus &cameraLabBus(); }}
using namespace larp::led;
namespace {
char token[17] = "0000000000000000";
unsigned stage = 0, frameNumber = 0;
std::uint32_t lastKeep = 0, started = 0, appliedAt = 0;
bool running = false, effectMode = false, waiting = false, stopPending = false;
bool directOutstanding = false;
bool resonanceMode = false, resonanceTransitions = false;
bool resonanceColors = false;
unsigned resonanceNumber = 0;
std::uint32_t resonanceAt = 0;
Effect selected = Effect::Off;
camera::LiveController live;
bool liveMode = false, traceLive = false;
std::uint32_t traceRevision = 0, lastLiveSubmit = 0;
void liveState();
std::uint32_t loopNow = 0;
#ifdef LARP_LED_CAMERA_RSSI_LAB
struct LabRadio final : camera::RssiRadio {
    void service(std::uint32_t now) override { larp::espnowradio::service(now); }
    larp::ledwire::EspNowSnapshot snapshot() override { return larp::espnowradio::snapshot(); }
    bool submit(const std::array<std::uint8_t, 6> &peer, std::uint32_t now) override {
        return larp::espnowradio::submit(peer, now, true, false);
    }
    void cancel() override { larp::espnowradio::cancel(); }
} labRadio;
struct MeasureOutput final : camera::RssiObserver {
    void measuring(const char *id, std::uint32_t sample, std::uint32_t now) override {
        Serial.printf("MEASURING %s %lu %lu\n", id, (unsigned long)sample, (unsigned long)now);
    }
    void rejected(const char *id, std::uint32_t sample, const char *reason, std::uint32_t now) override {
        Serial.printf("MEASURE_REJECT %s %lu %s %lu\n", id, (unsigned long)sample, reason, (unsigned long)now);
    }
    void measured(const camera::RssiResult &result) override {
        const auto &r = result.radio;
        Serial.printf("MEASURE %s %lu %lu %u %u %u %u %u %d %d %lu %lu ", result.token,
                      (unsigned long)result.sample, (unsigned long)result.now,
                      unsigned(r.outcome), unsigned(r.accepted), unsigned(r.link),
                      unsigned(r.received), unsigned(r.rssiValid), int(r.rssi), int(r.filteredRssi),
                      (unsigned long)r.receiverBoot, (unsigned long)r.receiverCount);
        const char *digits = "0123456789abcdef";
        for (const auto b : r.token) { Serial.write(digits[b >> 4]); Serial.write(digits[b & 15]); }
        Serial.write(' ');
        for (const auto b : r.peer) { Serial.write(digits[b >> 4]); Serial.write(digits[b & 15]); }
        Serial.println();
    }
} measureOutput;
camera::RssiController rssi(labRadio, measureOutput);
#endif

struct TraceBus final : LedBus {
    LedBus &bus = cameraLabBus();
    Frame pending{};
    std::uint32_t submitted = 0;
    bool inFlight = false, pendingLive = false;
    char pendingToken[17] = {};
    unsigned pendingStage = 0, pendingNumber = 0;
    std::uint32_t pendingRevision = 0;
    bool begin() override { return bus.begin(); }
    bool submit(const Frame &frame) override {
        if (inFlight || !bus.submit(frame)) return false;
        pending = frame; submitted = millis(); inFlight = true;
        pendingLive = traceLive; pendingRevision = traceRevision;
        std::strcpy(pendingToken, token); pendingStage = stage; pendingNumber = frameNumber++;
        return true;
    }
    TxResult poll() override {
        const auto result = bus.poll();
        if (result == TxResult::Complete && inFlight) {
            appliedAt = millis();
            if (pendingLive)
                Serial.printf("L %s %lu %u %lu %lu ", pendingToken, (unsigned long)pendingRevision,
                              pendingNumber, (unsigned long)submitted, (unsigned long)appliedAt);
            else Serial.printf("F %s %u %u %lu %lu ", pendingToken, pendingStage, pendingNumber,
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

void liveState() {
    const auto bg = live.scene().background();
    Serial.printf("STATE %s %lu %lu %u %u %u", token, (unsigned long)live.scene().revision(),
                  (unsigned long)millis(), unsigned(bg.red), unsigned(bg.green), unsigned(bg.blue));
    for (unsigned slot = 0; slot < near::Slots; ++slot) {
        const auto &i = live.scene().instance(slot); const auto &p = i.parameters;
        Serial.printf(" %u %lu %lu %u %u %u %u %u %u %u %u", unsigned(i.state),
                      (unsigned long)i.generation, (unsigned long)i.sequence, unsigned(p.level),
                      unsigned(p.valid), unsigned(p.colour.red), unsigned(p.colour.green),
                      unsigned(p.colour.blue), unsigned(p.gain), unsigned(p.duration), unsigned(p.curve));
    }
    Serial.println();
}
void stop(const char *reason) {
#ifdef LARP_LED_CAMERA_RSSI_LAB
    rssi.end(loopNow);
#endif
    if (liveMode) {
        live.hardStop(); traceRevision = live.scene().revision(); liveState(); liveMode = false;
    }
    running = false; waiting = false; effectMode = false;
    // Let the engine drain a prior direct transfer, then request a fresh black.
    engine.stop(); stopPending = true;
    Serial.printf("STOP %s %s\n", token, reason);
}
bool resonanceNext() {
    const auto step = camera::resonanceStep(resonanceNumber, resonanceTransitions);
    if (!step.duration) { stop("finished"); return false; }
    stage = 100 + resonanceNumber;
    resonanceAt = millis();
    engine.updateWake(step.valid, step.mask);
    if (step.restart && !(step.effect == Effect::Off ? engine.stop() : engine.start(step.effect))) {
        stop("engine-rejected"); return false;
    }
    Serial.printf("W %s %u %lu %u %u %u %u\n", token, resonanceNumber,
                  (unsigned long)resonanceAt, unsigned(step.valid), unsigned(step.mask),
                  unsigned(step.effect), unsigned(step.restart));
    return true;
}
void command(char *line) {
    if (std::strcmp(line, "HELLO") == 0) {
        Serial.printf("LARP-CAMERA/1 23 32 GPIO15 %lu %s\n",
                      (unsigned long)ESP.getSketchSize(), ESP.getSketchMD5().c_str());
        Serial.println("LARP-LIVE/1 2 23 32");
#ifdef LARP_LED_CAMERA_RSSI_LAB
        Serial.println("LARP-RSSI/1 1 2000 3000");
#endif
        return;
    }
    if (std::strcmp(line, "STOP") == 0) { stop("host"); return; }
#ifdef LARP_LED_CAMERA_RSSI_LAB
    if (std::strncmp(line, "RSSI ", 5) == 0 || std::strcmp(line, "RSSI") == 0) {
        rssi.command(line, loopNow); return;
    }
#endif
    if (std::strncmp(line, "GLOW ", 5) == 0 || std::strncmp(line, "BG ", 3) == 0 ||
        std::strncmp(line, "STATUS ", 7) == 0) {
        if (!running || !liveMode) { Serial.printf("REJECT %s 0 inactive 0 %lu\n", token, (unsigned long)millis()); return; }
        const auto reply = live.command(line, millis());
        if (std::strcmp(reply.kind, "STATUS") == 0) { liveState(); return; }
        if (std::strcmp(reply.kind, "REJECT") == 0)
            Serial.printf("REJECT %s %lu %s %lu %lu\n", token, (unsigned long)reply.request,
                          reply.reason, (unsigned long)live.scene().revision(), (unsigned long)millis());
        else {
            Serial.printf("%s %s %lu %lu %lu\n", reply.kind, token, (unsigned long)reply.request,
                          (unsigned long)live.scene().revision(), (unsigned long)millis());
            liveState();
        }
        return;
    }
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
    if (std::strcmp(verb, "LIVE") == 0) {
        if (strtok_r(nullptr, " ", &context) || (traceLive && std::strcmp(id, token) == 0)) return;
        if (!live.begin(id)) return;
        std::strcpy(token, id); frameNumber = stage = 0;
        started = lastKeep = millis(); lastLiveSubmit = started - LedOutput::FrameMilliseconds;
        running = liveMode = traceLive = true; waiting = effectMode = false;
        traceRevision = live.scene().revision();
#ifdef LARP_LED_CAMERA_RSSI_LAB
        rssi.begin(token);
#endif
        Serial.printf("LIVE %s %lu %lu\n", token, (unsigned long)traceRevision, (unsigned long)started);
        liveState(); return;
    }
    if (std::strcmp(verb, "CAL") == 0) {
        if (strtok_r(nullptr, " ", &context)) return;
        selected = Effect::Off;
        resonanceMode = false;
        resonanceColors = false;
    } else if (std::strcmp(verb, "RUN") == 0) {
        char *effect = strtok_r(nullptr, " ", &context);
        if (!effect || strtok_r(nullptr, " ", &context)) return;
        resonanceMode = std::strcmp(effect,"resonance") == 0 ||
                        std::strcmp(effect,"resonance-transitions") == 0;
        resonanceTransitions = std::strcmp(effect,"resonance-transitions") == 0;
        resonanceColors = std::strcmp(effect,"resonance-colors") == 0;
        selected = (resonanceMode || resonanceColors) ? Effect::Resonance :
                   std::strcmp(effect,"twinkle") == 0 ? Effect::Twinkle :
                   std::strcmp(effect,"burst") == 0 ? Effect::Burst :
                   std::strcmp(effect,"ripple") == 0 ? Effect::Ripple : Effect::Off;
        if (selected == Effect::Off) return;
    } else return;
    traceLive = false;
    std::strcpy(token, id); frameNumber = stage = 0;
    started = lastKeep = millis(); running = true; waiting = effectMode = false;
    Serial.printf("BEGIN %s %lu\n", token, (unsigned long)started);
}
void usb() {
    static char line[192]; static unsigned used = 0; static bool overflow = false;
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
#ifdef LARP_LED_CAMERA_RSSI_LAB
    if (!larp::espnowradio::begin()) Serial.println("FAULT rssi-init");
#endif
    Serial.println("LARP-CAMERA/1 23 32 GPIO15");
}
void loop() {
    loopNow = millis();
#ifdef LARP_LED_CAMERA_RSSI_LAB
    rssi.service(loopNow);
#endif
    usb();
    if (running && (millis()-lastKeep > 1500 || millis()-started > 90000)) stop("watchdog");
    if (stopPending) {
        // A direct lab frame is not owned by the engine. Drain it first.
        if (directOutstanding && bus.inFlight) {
            const auto result = bus.poll();
            if (result == TxResult::Busy && millis()-bus.submitted < LedOutput::TransferTimeoutMilliseconds) { delay(1); return; }
            if (result == TxResult::Failed || result == TxResult::Busy) bus.reset();
            directOutstanding = false;
        }
        engine.service();
        if (!engine.snapshot().pending && !bus.inFlight) {
            stopPending = false; Serial.printf("DONE %s\n", token);
        }
        delay(1); return;
    }
    if (!running) { delay(1); return; }
    if (liveMode) {
        const auto now = millis(); live.scene().service(now);
        if (bus.inFlight) {
            const auto result = bus.poll();
            if (result == TxResult::Failed || (result == TxResult::Busy && now-bus.submitted >= 2000)) {
                bus.reset(); directOutstanding = false; stop("live-bus-fault");
            } else if (result == TxResult::Complete) directOutstanding = false;
        }
        if (liveMode && !bus.inFlight && now-lastLiveSubmit >= LedOutput::FrameMilliseconds) {
            const Frame frame = live.scene().render(now); traceRevision = live.scene().revision();
            if (!bus.submit(frame)) stop("live-submit");
            else { directOutstanding = true; lastLiveSubmit = now; }
        }
        delay(1); return;
    }
    if (effectMode) {
        if (resonanceMode) {
            const auto step = camera::resonanceStep(resonanceNumber, resonanceTransitions);
            if (millis()-resonanceAt >= step.duration) {
                ++resonanceNumber;
                if (!resonanceNext()) { delay(1); return; }
            }
        }
        engine.service();
        if (engine.snapshot().fault) stop("engine-fault");
        else if (!resonanceMode && millis()-appliedAt > 1000 && engine.snapshot().completed && engine.snapshot().off)
            stop("finished");
        else if (selected == Effect::Twinkle && millis()-started >= 7500) stop("finished");
        delay(1); return;
    }
    const auto current = selected == Effect::Off ? camera::step(stage) :
        stage < 6 ? camera::step(stage) :
        stage == 6 ? camera::dark(1000) : resonanceColors ? camera::resonanceColorStep(stage-7) :
        camera::Step{-2, {}, 0, selected};
    if (current.index == -3) { stop("finished"); return; }
    if (current.index == -2) {
        if (resonanceMode) {
            resonanceNumber = 0;
            if (!resonanceNext()) { delay(1); return; }
        } else if (!engine.start(current.effect)) { stop("engine-rejected"); return; }
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
