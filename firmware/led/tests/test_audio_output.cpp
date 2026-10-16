#include "AudioOutput.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace larp::audio;
static std::uint32_t now;
static std::uint32_t clockNow() { return now; }
struct FakeBus final : AudioBus {
    bool beginOkay = true, submitOkay = true, low = false, inFlight = false;
    TxResult result = TxResult::Complete;
    std::vector<CommandFrame> sent;
    bool begin() override { return beginOkay; }
    bool submit(const CommandFrame &frame) override {
        if (!submitOkay || inFlight) return false;
        sent.push_back(frame); inFlight = true; return true;
    }
    TxResult poll() override {
        assert(inFlight);
        if (result != TxResult::Busy) inFlight = false;
        return result;
    }
    bool sampleBusyLow() override { return low; }
};
struct Rig {
    FakeBus bus;
    AudioOutput output{bus, clockNow};
    Rig() { now = 0; }
    void step(std::uint32_t delta = 101) { now += delta; output.service(); }
    void drain() {
        for (int i = 0; i < 20 && output.snapshot().pending; ++i) step();
        assert(!output.snapshot().pending);
    }
    void boot() { assert(output.begin()); drain(); assert(output.snapshot().ready); }
};

static void goldenFrames() {
    const CommandFrame stop{{0x7e,0xff,0x06,0x16,0,0,0,0xfe,0xe5,0xef}};
    const CommandFrame volume{{0x7e,0xff,0x06,0x06,0,0,5,0xfe,0xf0,0xef}};
    const CommandFrame play{{0x7e,0xff,0x06,0x12,0,0,1,0xfe,0xe8,0xef}};
    assert(AudioOutput::encode(0x16,0) == stop);
    assert(AudioOutput::encode(0x06,5) == volume);
    assert(AudioOutput::encode(0x12,1) == play);
    const auto high = AudioOutput::encode(0x12, 0x1234);
    assert(high[5] == 0x12 && high[6] == 0x34);
}
static void bootNoAutoplay() {
    Rig r; r.boot();
    assert(r.bus.sent.size() == 1 && r.bus.sent[0][3] == 0x16);
    const auto s = r.output.snapshot();
    assert(!s.active && !s.completed && s.busyValid && !s.busyLow);
    assert(s.revision == 0 && s.appliedRevision == 0);
    r.step(60000); assert(r.bus.sent.size() == 1);
    assert(!r.output.begin());
}
static void boundedInputsAndPlay() {
    Rig r; assert(!r.output.start(1)); r.boot();
    for (auto track : {0, 256, 65535}) assert(!r.output.start(track));
    assert(!r.output.start(1,0) && !r.output.start(1,11));
    assert(r.output.start(255,10));
    assert(!r.output.start(2));
    assert(r.output.snapshot().pending && r.output.snapshot().appliedRevision == 0);
    r.drain();
    assert(r.bus.sent.size() == 4);
    assert(r.bus.sent[1][3] == 0x16 && r.bus.sent[2][3] == 0x06 && r.bus.sent[2][6] == 10);
    assert(r.bus.sent[3][3] == 0x12 && r.bus.sent[3][6] == 255);
    auto s = r.output.snapshot();
    assert(s.active && !s.completed && !s.busyLow && s.track == 255 && s.volume == 10);
    assert(s.revision == 1 && s.appliedRevision == 1);
}
static void repeatAndStop() {
    Rig r; r.boot(); assert(r.output.start(1)); r.drain();
    assert(r.output.start(1)); r.drain();
    assert(r.output.snapshot().revision == 2 && r.bus.sent.size() == 7);
    assert(r.output.stop());
    assert(r.output.snapshot().active); // Queuing Stop is not transmission.
    r.drain();
    auto s = r.output.snapshot();
    assert(!s.active && s.track == 0 && s.revision == 3 && s.appliedRevision == 3);
    assert(r.bus.sent.back()[3] == 0x16);
    r.step(60000); assert(r.bus.sent.size() == 8);
}
static void busyEvidenceAndBound() {
    Rig r; r.boot(); assert(r.output.start(1)); r.drain();
    r.step(); assert(!r.output.snapshot().completed);
    r.bus.low = true; r.step(); assert(r.output.snapshot().active && r.output.snapshot().busyLow);
    r.bus.low = false; r.step(); assert(r.output.snapshot().completed && !r.output.snapshot().active);
    // A possibly floating BUSY signal cannot cancel the thirty-second Stop.
    r.step(30000); assert(r.output.snapshot().pending && r.output.snapshot().timedOut);
    r.drain(); assert(r.bus.sent.back()[3] == 0x16 && r.output.snapshot().timedOut);
    assert(r.output.start(2)); assert(!r.output.snapshot().timedOut);
}
static void stopDuringPreparation() {
    Rig r; r.boot(); assert(r.output.start(1));
    r.step(); r.step(); // The initial Stop has drained; Volume is not sent yet.
    assert(r.output.stop()); r.drain();
    for (const auto &frame : r.bus.sent) assert(frame[3] == 0x16);
    assert(!r.output.snapshot().active);
}
static void stopDuringPlayTransmission() {
    Rig r; r.boot(); assert(r.output.start(1));
    while (r.bus.sent.back()[3] != 0x12) r.step();
    r.bus.result = TxResult::Busy;
    assert(r.output.stop()); r.step();
    assert(r.output.snapshot().pending && r.output.snapshot().appliedRevision == 0);
    r.bus.result = TxResult::Complete; r.step();
    assert(r.output.snapshot().pending); // Old Play cannot acknowledge new Stop.
    r.drain();
    assert(r.output.snapshot().appliedRevision == 2 && !r.output.snapshot().active);
    assert(r.bus.sent.back()[3] == 0x16);
}
static void respectsGapAndTransmission() {
    Rig r; r.boot(); assert(r.output.start(1));
    r.step(100); assert(r.bus.sent.size() == 2);
    r.step(10); assert(r.bus.sent.size() == 2);
    r.step(99); assert(r.bus.sent.size() == 2);
    r.step(1); assert(r.bus.sent.size() == 3);
}
static void failuresAndRecovery() {
    { Rig r; r.bus.beginOkay = false; assert(!r.output.begin()); assert(r.output.snapshot().fault); }
    { Rig r; r.boot(); r.bus.submitOkay = false; assert(r.output.start(1)); r.step();
      assert(r.output.snapshot().fault && !r.output.snapshot().ready);
      assert(!r.output.start(2)); r.step(); assert(!r.output.snapshot().pending); }
    { Rig r; r.boot(); assert(r.output.start(1)); r.step(); r.bus.result = TxResult::Failed; r.step();
      assert(r.output.snapshot().fault); r.bus.result = TxResult::Complete; r.drain();
      assert(r.bus.sent.back()[3] == 0x16 && !r.output.snapshot().ready); }
    { Rig r; r.boot(); assert(r.output.start(1)); r.step(); r.bus.result = TxResult::Busy;
      r.step(500); assert(r.output.snapshot().fault); r.step();
      r.bus.result = TxResult::Complete; r.drain();
      assert(r.bus.sent.back()[3] == 0x16 && !r.output.snapshot().ready); }
}
static void wraparound() {
    Rig r; r.boot(); now = UINT32_MAX - 600; assert(r.output.start(1)); r.drain();
    r.step(29999); assert(!r.output.snapshot().timedOut);
    r.step(1); assert(r.output.snapshot().timedOut); r.drain();
    assert(!r.output.snapshot().active);
}
static void pauseResumeAndLiveVolume() {
    Rig r; r.boot(); assert(!r.output.pause() && !r.output.resume());
    assert(r.output.start(100, 5)); r.drain();
    const auto played = now;
    assert(r.output.pause()); assert(!r.output.resume()); r.drain();
    assert(r.output.snapshot().paused && !r.output.snapshot().active && r.output.snapshot().track == 100);
    assert(r.bus.sent.back() == AudioOutput::encode(0x0e, 0));
    const auto count = r.bus.sent.size();
    assert(r.output.volume(8)); r.drain();
    assert(r.bus.sent.size() == count + 1 && r.bus.sent.back() == AudioOutput::encode(0x06, 8));
    assert(r.output.snapshot().volume == 8 && r.output.snapshot().paused);
    assert(r.output.resume()); r.drain();
    assert(r.bus.sent.back() == AudioOutput::encode(0x0d, 0) && r.output.snapshot().active && !r.output.snapshot().paused);
    assert(r.output.volume(3)); r.drain(); assert(r.output.snapshot().active && r.output.snapshot().track == 100);
    assert(!r.output.volume(0) && !r.output.volume(11));
    now = played + 30000; r.output.service(); r.drain();
    assert(r.output.snapshot().timedOut && !r.output.snapshot().paused && !r.output.snapshot().active);
    assert(!r.output.resume());
    assert(r.output.start(101)); r.drain(); assert(r.output.pause()); r.step();
    assert(r.output.stop()); r.drain(); assert(!r.output.snapshot().paused && r.bus.sent.back()[3] == 0x16);
}
int main() {
    pauseResumeAndLiveVolume(); goldenFrames(); bootNoAutoplay(); boundedInputsAndPlay(); repeatAndStop();
    busyEvidenceAndBound(); stopDuringPreparation(); stopDuringPlayTransmission();
    respectsGapAndTransmission(); failuresAndRecovery(); wraparound();
    std::cout << "11 audio output cases passed\n";
}
