#include "LedOutput.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace larp::led;
namespace {
std::uint32_t now = 0;
std::uint32_t clockNow() { return now; }
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
bool black(const Frame &frame) { for (const auto &p : frame) if (p.red || p.green || p.blue) return false; return true; }
bool equal(const Frame &a, const Frame &b) {
    for (std::size_t i = 0; i < PixelCount; ++i)
        if (a[i].red != b[i].red || a[i].green != b[i].green || a[i].blue != b[i].blue) return false;
    return true;
}
void bounded(const Frame &frame) {
    unsigned sum = 0;
    for (const auto &p : frame) {
        check(p.red <= 32 && p.green <= 32 && p.blue <= 32, "brightness bound exceeded");
        sum += p.red + p.green + p.blue;
    }
    check(sum * 20U / 255U + PixelCount < LedOutput::Milliamps, "current budget exceeded");
}
struct Bus final : LedBus {
    bool beginOk = true, submitOk = true, resetOk = true, busy = false, done = false, failed = false;
    unsigned resets = 0, submits = 0;
    std::vector<Frame> frames;
    const Frame *retained = nullptr;
    bool begin() override { return beginOk; }
    bool submit(const Frame &frame) override {
        check(!busy, "new frame submitted while previous transfer busy");
        ++submits;
        if (!submitOk) return false;
        bounded(frame);
        frames.push_back(frame); retained = &frame;
        busy = true; done = false;
        return true;
    }
    TxResult poll() override {
        check(busy, "poll without accepted transfer");
        check(equal(*retained, frames.back()), "in-flight frame storage was mutated");
        if (failed) return TxResult::Failed;
        if (!done) return TxResult::Busy;
        busy = false; return TxResult::Complete;
    }
    bool reset() override {
        ++resets;
        if (!resetOk) return false;
        busy = false; done = false; failed = false;
        return true;
    }
};
struct Fixture {
    Bus bus;
    LedOutput output;
    Fixture() : output(bus, clockNow) { now = 0; }
    void finish() { bus.done = true; output.service(); }
    void boot() {
        check(output.begin(), "begin failed"); output.service(); finish();
        check(output.snapshot().ready && output.snapshot().off, "boot did not confirm darkness");
    }
    void start(Effect effect) {
        check(output.start(effect), "start rejected"); output.service(); finish();
        check(!output.snapshot().pending, "start did not complete");
    }
    void at(std::uint32_t time) { now = time; output.service(); if (bus.busy) finish(); }
};

void boot() {
    Fixture f;
    check(!f.output.start(Effect::Twinkle), "start before begin");
    check(!f.output.stop(), "stop before begin");
    check(f.output.begin(), "begin rejected");
    auto s = f.output.snapshot();
    check(s.revision == 1 && s.pending && !s.off && !s.ready, "boot fabricated applied state");
    check(!f.output.start(Effect::Twinkle), "start before initial TX_DONE");
    f.output.service();
    check(f.bus.frames.size() == 1 && black(f.bus.frames.back()), "boot must submit black");
    f.output.service();
    check(!f.output.snapshot().ready, "busy initial transfer marked ready");
    f.finish(); s = f.output.snapshot();
    check(s.ready && s.off && !s.pending && s.appliedRevision == 1, "initial completion missing");
    check(!f.output.begin(), "second begin accepted");
}
void boot_failure() {
    Fixture f; f.bus.beginOk = false;
    check(!f.output.begin(), "failed bus began"); f.output.service();
    check(f.output.snapshot().fault && !f.output.snapshot().ready && f.bus.submits == 0, "init failure not closed");
}
void twinkle_all_pixels() {
    Fixture f; f.boot(); f.start(Effect::Twinkle);
    const Frame initial = f.bus.frames.back();
    bool changed[PixelCount]{};
    for (now = 33; now < 8000; now += 33) {
        f.output.service(); f.finish();
        const auto &frame = f.bus.frames.back();
        for (std::size_t i = 0; i < PixelCount; ++i) {
            check(frame[i].red >= 4 && frame[i].red == frame[i].green && frame[i].red == frame[i].blue,
                  "twinkle must keep every LED visibly white");
            changed[i] = changed[i] || frame[i].red != initial[i].red;
        }
    }
    for (const bool value : changed) check(value, "a twinkle pixel never varied");
    check(f.output.snapshot().requested == Effect::Twinkle && !f.output.snapshot().completed, "twinkle ended automatically");
}
void stop_fresh_frame() {
    Fixture f; f.boot();
    const auto before = f.bus.frames.size();
    check(f.output.stop(), "stop rejected");
    check(f.output.snapshot().pending && f.output.snapshot().appliedRevision == 1, "stop prematurely acknowledged");
    f.output.service();
    check(f.bus.frames.size() == before + 1 && black(f.bus.frames.back()), "already-off stop needs fresh black");
    f.finish(); check(f.output.snapshot().appliedRevision == 2 && !f.output.snapshot().pending, "fresh stop not applied");
}
void stop_inflight() {
    Fixture f; f.boot(); check(f.output.start(Effect::Twinkle), "start rejected"); f.output.service();
    check(f.output.stop(), "stop rejected"); const auto revision = f.output.snapshot().revision;
    f.output.service(); check(f.bus.frames.size() == 2, "overwrote busy start");
    f.finish();
    check(f.output.snapshot().pending && f.output.snapshot().appliedRevision != revision, "stale start satisfied stop");
    check(black(f.bus.frames.back()), "stop did not follow old transfer with black");
    f.finish(); check(f.output.snapshot().off && f.output.snapshot().appliedRevision == revision, "stop confirmation missing");
}
void latest_wins() {
    Fixture f; f.boot();
    check(f.output.start(Effect::Twinkle) && f.output.start(Effect::Ripple), "queued starts failed");
    f.output.service(); f.finish();
    check(f.bus.frames.size() == 2 && f.output.snapshot().applied == Effect::Ripple, "superseded start emitted");
    check(f.bus.frames.back()[11].blue == 32, "ripple did not begin in middle");
}
void burst_once() {
    Fixture f; f.boot(); f.start(Effect::Burst);
    check(f.bus.frames.back()[0].red == 1, "burst must visibly begin");
    for (const auto time : {400U, 800U, 1200U, 1599U}) {
        f.at(time);
        const auto &frame = f.bus.frames.back();
        for (const auto &p : frame) check(p.red == frame[0].red && p.red == p.green && p.red == p.blue, "burst not global white");
        if (time == 800) check(frame[0].red == 32, "burst peak missing");
    }
    const auto revision = f.output.snapshot().revision;
    now = 1600; f.output.service();
    check(f.output.snapshot().pending && !f.output.snapshot().completed, "burst completion preceded black TX_DONE");
    check(black(f.bus.frames.back()), "burst final black missing");
    f.finish();
    auto s = f.output.snapshot();
    check(s.completed && s.off && s.applied == Effect::Off && s.revision == revision && s.appliedRevision == revision,
          "automatic burst completion lost start token");
    const auto count = f.bus.frames.size(); f.at(5000);
    check(f.bus.frames.size() == count, "burst repeated");
}
void restart_same() {
    Fixture f; f.boot(); f.start(Effect::Burst); f.at(800);
    check(f.output.start(Effect::Burst), "same effect restart rejected"); f.output.service(); f.finish();
    check(f.bus.frames.back()[0].red == 1, "same-effect restart did not reset phase");
    f.at(1600); check(!f.output.snapshot().completed && f.bus.frames.back()[0].red == 32, "old deadline survived restart");
    f.at(2400); check(f.output.snapshot().completed && f.output.snapshot().off, "restarted pulse did not end");
}
void ripple_once() {
    Fixture f; f.boot(); f.start(Effect::Ripple);
    check(f.bus.frames.back()[11].blue == 32 && f.bus.frames.back()[0].blue == 0, "ripple not centered");
    bool edgeLit = false;
    for (now = 33; now < 1400; now += 33) {
        f.output.service(); f.finish();
        const auto &frame = f.bus.frames.back();
        unsigned active = 0;
        for (std::size_t i = 0; i < PixelCount; ++i) {
            check(frame[i].blue == frame[PixelCount - 1 - i].blue, "ripple asymmetric");
            if (frame[i].blue) ++active;
        }
        check(active <= 4, "multiple waves appeared");
        edgeLit = edgeLit || frame[0].blue > 10;
    }
    check(edgeLit, "ripple never reached both strip ends");
    f.at(1400); check(f.output.snapshot().completed && f.output.snapshot().off, "ripple did not finish dark");
    const auto count = f.bus.frames.size(); f.at(9000); check(f.bus.frames.size() == count, "ripple restarted");
}
void replace_automatic_off() {
    Fixture f; f.boot(); f.start(Effect::Burst); now = 1600; f.output.service();
    check(f.output.start(Effect::Twinkle), "replacement during automatic off rejected");
    const auto revision = f.output.snapshot().revision;
    f.finish(); check(f.output.snapshot().pending && !f.output.snapshot().completed, "old auto-off completed new request");
    f.finish(); check(f.output.snapshot().applied == Effect::Twinkle && f.output.snapshot().appliedRevision == revision,
                      "replacement did not apply");
}
void submit_failure() {
    Fixture f; f.boot(); f.bus.submitOk = false;
    check(f.output.start(Effect::Twinkle), "start rejected"); f.output.service();
    check(f.output.snapshot().fault && !f.output.snapshot().ready && f.output.snapshot().pending, "failed submit acknowledged");
    f.bus.submitOk = true; f.output.service();
    check(f.bus.resets == 1 && black(f.bus.frames.back()), "failure did not recover to black");
    f.finish(); check(f.output.snapshot().off && f.output.snapshot().fault && !f.output.snapshot().ready, "fault cleared itself");
    check(!f.output.start(Effect::Twinkle), "start allowed after sticky fault");
}
void failed_completion() {
    Fixture f; f.boot(); check(f.output.start(Effect::Twinkle), "start rejected"); f.output.service();
    f.bus.failed = true; f.output.service();
    check(f.output.snapshot().fault && f.output.snapshot().appliedRevision == 1, "failed TX fabricated completion");
    f.output.service(); f.finish(); check(f.output.snapshot().off, "failed TX did not attempt fresh black");
}
void timeout() {
    Fixture f; f.boot(); check(f.output.start(Effect::Twinkle), "start rejected"); f.output.service();
    now = 1999; f.output.service(); check(!f.output.snapshot().fault, "timeout early");
    now = 2000; f.output.service(); check(f.output.snapshot().fault && f.output.snapshot().pending, "timeout not faulted");
    f.bus.resetOk = false; f.output.service();
    check(f.bus.resets == 1, "recovery missing");
    f.output.service(); check(f.bus.resets == 1, "recovery busy-looped");
    now += 250; f.bus.resetOk = true; f.output.service();
    check(f.bus.resets == 2 && black(f.bus.frames.back()), "timeout recovery not black"); f.finish();
    check(f.output.snapshot().off && !f.output.snapshot().ready, "timeout recovery cleared fault");
}
void clock_wrap() {
    Fixture f; f.boot(); now = 0xffffff00U; f.start(Effect::Burst);
    now += 800; f.output.service(); f.finish();
    check(f.bus.frames.back()[0].red == 32 && !f.output.snapshot().fault, "pulse failed across clock wrap");
    now += 800; f.output.service(); f.finish();
    check(f.output.snapshot().completed && f.output.snapshot().off, "wrapped deadline did not end pulse");
}
void invalid_effect() {
    Fixture f; f.boot(); const auto revision = f.output.snapshot().revision;
    check(!f.output.start(Effect::Off) && !f.output.start(static_cast<Effect>(99)), "invalid effect accepted");
    check(f.output.snapshot().revision == revision && f.output.snapshot().off, "invalid effect changed output");
}
void frame_rate() {
    Fixture f; f.boot(); f.start(Effect::Twinkle); const auto count = f.bus.frames.size();
    for (now = 1; now < 33; ++now) f.output.service();
    check(f.bus.frames.size() == count, "effect ignored frame budget");
    f.output.service(); check(f.bus.frames.size() == count + 1, "effect missed next frame");
}
void resonance_masks() {
    // Explicit physical partition expected from the existing Wake origins.
    const unsigned zones[PixelCount] = {1,1,1,1,1,1,1,3,3,3,3,3,3,2,2,2,2,2,2,0,0,0,0};
    for (unsigned mask = 0; mask < 16; ++mask) {
        Fixture f; f.boot(); f.output.updateWake(true, static_cast<std::uint8_t>(mask));
        const auto count = f.bus.frames.size(); f.at(33);
        check(f.bus.frames.size() == count, "Wake started light without explicit Start");
        f.start(Effect::Resonance);
        const auto initial = f.bus.frames.back();
        f.at(633);
        const auto &frame = f.bus.frames.back();
        for (unsigned i = 0; i < PixelCount; ++i) {
            const bool active = (mask & (1U << zones[i])) != 0;
            check((frame[i].blue > 0) == active, "Wake mask lit the wrong physical zone");
            if (active && mask != 15) check(frame[i].blue != initial[i].blue, "active zone did not breathe");
        }
        check(f.output.snapshot().applied == Effect::Resonance && !f.output.snapshot().completed,
              "resonance must remain an active effect even with no inputs");
    }
}
void resonance_join_release_unknown() {
    Fixture f; f.boot(); f.output.updateWake(true, 7); f.start(Effect::Resonance);
    f.at(600); f.output.updateWake(true, 15); f.at(633);
    check(f.bus.frames.back()[2].blue != f.bus.frames.back()[22].blue, "all-four merge skipped its transition");
    f.at(1400);
    const auto together = f.bus.frames.back();
    for (const auto &p : together) check(p.blue == 24 && p.green == 16 && p.red == 4, "all-four did not settle together");
    f.at(2000); check(equal(together, f.bus.frames.back()), "settled light kept breathing");
    const auto revision = f.output.snapshot().revision;
    f.output.updateWake(true, 14); f.at(2033);
    check(f.bus.frames.back()[22].blue == 0 && f.bus.frames.back()[2].blue > 0, "release kept a missing Wake lit");
    f.output.updateWake(false, 15); f.at(2066);
    check(black(f.bus.frames.back()), "unknown Wake reused an old success mask");
    check(f.output.snapshot().revision == revision && !f.output.snapshot().completed,
          "input updates changed command revision or falsely completed the effect");
    f.output.updateWake(true, 0xff); f.at(2099);
    check(black(f.bus.frames.back()), "invalid mask produced light");
    f.output.updateWake(true, 15); f.at(2132);
    check(!equal(together, f.bus.frames.back()), "recovered all-four reused old settled state");
    f.at(2900); check(equal(together, f.bus.frames.back()), "all-four did not recover after valid reading");
}
void resonance_stop_restart_inflight() {
    Fixture f; f.boot(); f.output.updateWake(true, 15); f.start(Effect::Resonance); f.at(800);
    const auto settled = f.bus.frames.back();
    check(f.output.start(Effect::Resonance), "resonance restart rejected"); f.output.service();
    check(!equal(settled, f.bus.frames.back()), "explicit restart retained settled light");
    f.output.updateWake(false, 15); // Must not mutate a frame still retained by the bus.
    f.finish(); f.at(833); check(black(f.bus.frames.back()), "unknown during TX was not applied on next frame");
    f.output.updateWake(true, 15); f.at(866);
    check(!equal(settled, f.bus.frames.back()), "restart did not reset merge");
    check(f.output.stop(), "resonance stop rejected"); f.output.service();
    check(f.output.snapshot().pending && black(f.bus.frames.back()), "stop must submit and await black");
    f.finish(); check(f.output.snapshot().off && f.output.snapshot().applied == Effect::Off, "resonance Stop not confirmed");
    const auto count = f.bus.frames.size(); f.output.updateWake(true, 1); f.at(2000);
    check(f.bus.frames.size() == count, "Wake restarted a stopped effect");
    f.start(Effect::Ripple); check(f.bus.frames.back()[11].blue == 32, "switch from resonance broke Ripple");
}
void resonance_clock_wrap() {
    Fixture f; f.boot(); now = 0xffffff00U;
    f.output.updateWake(true, 15); f.start(Effect::Resonance);
    now += 800; f.output.service(); f.finish();
    const auto together = f.bus.frames.back();
    check(together[0].blue == 24 && together[22].blue == 24, "resonance merge failed across wrap");
    // A later full clock cycle must not restart an already settled merge.
    now = 0xffffff00U; f.output.service(); f.finish();
    check(equal(together, f.bus.frames.back()), "settled resonance replayed merge after wrap");
}
void twinkle_intensity() {
    Fixture full; full.boot(); full.start(Effect::Twinkle); full.at(330);
    const auto reference = full.bus.frames.back();
    Fixture dim; dim.boot(); dim.output.setTwinkleIntensity(20); dim.start(Effect::Twinkle); dim.at(330);
    const auto revision = dim.output.snapshot().revision;
    for (std::size_t i=0;i<PixelCount;++i)
        check(dim.bus.frames.back()[i].red == reference[i].red * 20U / 100U, "twinkle modulation lost original phase");
    dim.output.setTwinkleIntensity(1000); dim.at(363);
    check(dim.output.snapshot().revision == revision, "intensity restarted effect or incremented command revision");
    bounded(dim.bus.frames.back());
    dim.output.setTwinkleIntensity(0); dim.at(396); check(black(dim.bus.frames.back()), "zero intensity not dark");
    dim.start(Effect::Burst); check(!black(dim.bus.frames.back()), "twinkle intensity leaked into other effect");
    dim.output.updateWake(true, 15); dim.start(Effect::Resonance);
    check(!black(dim.bus.frames.back()), "RSSI twinkle intensity leaked into resonance");
}
}
int main(int argc, char **argv) {
    const struct { const char *name; void (*run)(); } cases[] = {
        {"boot", boot}, {"boot_failure", boot_failure}, {"twinkle_all_pixels", twinkle_all_pixels},
        {"stop_fresh_frame", stop_fresh_frame}, {"stop_inflight", stop_inflight}, {"latest_wins", latest_wins},
        {"burst_once", burst_once}, {"restart_same", restart_same}, {"ripple_once", ripple_once},
        {"replace_automatic_off", replace_automatic_off}, {"submit_failure", submit_failure},
        {"failed_completion", failed_completion}, {"timeout", timeout}, {"clock_wrap", clock_wrap},
        {"invalid_effect", invalid_effect}, {"frame_rate", frame_rate},
        {"resonance_masks", resonance_masks}, {"resonance_join_release_unknown", resonance_join_release_unknown},
        {"resonance_stop_restart_inflight", resonance_stop_restart_inflight}, {"resonance_clock_wrap", resonance_clock_wrap},
        {"twinkle_intensity", twinkle_intensity},
    };
    try {
        for (const auto &test : cases) if (argc == 1 || std::string(argv[1]) == test.name) {
            test.run(); std::cout << test.name << " passed\n";
        }
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
}
