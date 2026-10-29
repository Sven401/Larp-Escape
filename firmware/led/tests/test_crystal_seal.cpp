#include "CrystalSeal.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace larp::led;
namespace {
void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
bool rgb(Pixel p, unsigned r, unsigned g, unsigned b) {
    return p.red == r && p.green == g && p.blue == b;
}
bool sameFrame(const Frame &a, const Frame &b) {
    for (unsigned i = 0; i < PixelCount; ++i)
        if (!rgb(a[i], b[i].red, b[i].green, b[i].blue)) return false;
    return true;
}
bool sameParameters(const seal::Parameters &a, const seal::Parameters &b) {
    return a.count == b.count && a.valid == b.valid && a.cause == b.cause &&
        rgb(a.colour, b.colour.red, b.colour.green, b.colour.blue) &&
        rgb(a.peak, b.peak.red, b.peak.green, b.peak.blue) &&
        a.sweepMs == b.sweepMs && a.pixelMs == b.pixelMs &&
        a.transitionMs == b.transitionMs && a.pulseMs == b.pulseMs;
}
bool sameSnapshot(const seal::Snapshot &a, const seal::Snapshot &b) {
    return a.state == b.state && a.round == b.round && a.generation == b.generation &&
        a.sequence == b.sequence && sameParameters(a.parameters, b.parameters) &&
        a.pulseConsumed == b.pulseConsumed && a.pulsePending == b.pulsePending;
}
void uniform(const Frame &frame, unsigned r, unsigned g, unsigned b, const char *message) {
    for (const auto &pixel : frame) check(rgb(pixel, r, g, b), message);
}
void snapshotFrame(const Frame &frame, unsigned count, unsigned r, unsigned g, unsigned b,
                   const char *message) {
    const unsigned ends[] = {0, 8, 15, 23};
    for (unsigned i = 0; i < PixelCount; ++i)
        check(i < ends[count] ? rgb(frame[i], r, g, b) : rgb(frame[i], 0, 0, 0), message);
}
seal::Command command(unsigned count = 0) {
    seal::Command c;
    c.round = c.generation = c.sequence = 1;
    c.parameters.valid = true;
    c.parameters.count = static_cast<std::uint8_t>(count);
    c.parameters.cause = seal::Cause::Snapshot;
    return c;
}
void apply(seal::Scene &scene, const seal::Command &c, std::uint32_t now) {
    scene.service(now);
    check(std::strcmp(scene.apply(c, now), "applied") == 0, "valid command rejected");
}
void progress(seal::Scene &scene, seal::Command &c, unsigned count, std::uint32_t now,
              bool authorize = false) {
    c.operation = seal::Operation::Update;
    ++c.sequence;
    c.parameters.count = static_cast<std::uint8_t>(count);
    c.parameters.cause = seal::Cause::Progress;
    c.inheritedConsumed = false;
    c.authorizePulse = authorize;
    apply(scene, c, now);
}
void reject(seal::Scene &scene, const seal::Command &c, std::uint32_t now,
            const char *reason, const char *message) {
    scene.service(now);
    seal::Scene reference = scene;
    const auto before = scene.snapshot();
    check(std::strcmp(scene.apply(c, now), reason) == 0, message);
    check(sameSnapshot(scene.snapshot(), before), "rejected command changed snapshot");
    check(sameFrame(scene.render(now), reference.render(now)), "rejected command changed frame");
    seal::Scene after = scene;
    check(sameFrame(after.render(now + 75U), reference.render(now + 75U)),
          "rejected command changed a running timer");
}
void fastSequence(seal::Scene &scene, seal::Command &c, std::uint32_t base = 0) {
    c = command();
    apply(scene, c, base);
    progress(scene, c, 1, base);
    progress(scene, c, 2, base + 100U);
    progress(scene, c, 3, base + 200U, true);
}

void defaultsAndSnapshots() {
    check(seal::Scale == 1000000U, "fixed point scale");
    seal::Scene empty;
    check(empty.snapshot().state == seal::State::Empty, "boot created an instance");
    uniform(empty.render(123), 0, 0, 0, "boot contribution");
    auto defaults = command();
    check(rgb(defaults.parameters.colour, 0, 8, 12), "default colour");
    check(rgb(defaults.parameters.peak, 4, 18, 24), "default peak");
    check(defaults.parameters.sweepMs == 600 && defaults.parameters.pixelMs == 150 &&
          defaults.parameters.transitionMs == 300 && defaults.parameters.pulseMs == 800,
          "default timings");
    for (unsigned count = 0; count <= 3; ++count) {
        seal::Scene scene;
        auto c = command(count);
        // A Start is always a snapshot, even if the caller labels its cause Progress.
        c.parameters.cause = seal::Cause::Progress;
        apply(scene, c, 0);
        snapshotFrame(scene.render(0), count, 0, 0, 0, "start was not dark");
        snapshotFrame(scene.render(150), count, 0, 4, 6, "snapshot midpoint / 8-7-8 map");
        snapshotFrame(scene.render(300), count, 0, 8, 12, "snapshot endpoint / 8-7-8 map");
        snapshotFrame(scene.render(700), count, 0, 8, 12, "start replayed progress or pulse");
        check(scene.snapshot().pulseConsumed == (count == 3), "snapshot pulse consumption");
        check(!scene.snapshot().pulsePending, "snapshot scheduled a pulse");
    }
}

void sweepGoldenPoints() {
    seal::Scene scene;
    auto c = command();
    apply(scene, c, 0);
    progress(scene, c, 1, 0);
    check(rgb(scene.render(75)[0], 0, 4, 6), "eight-pixel sweep first midpoint");
    check(rgb(scene.render(150)[0], 0, 8, 12), "eight-pixel sweep first endpoint");
    check(rgb(scene.render(450)[7], 0, 0, 0), "eight-pixel sweep last delay");
    check(rgb(scene.render(525)[7], 0, 4, 6), "eight-pixel sweep last midpoint");
    snapshotFrame(scene.render(600), 1, 0, 8, 12, "eight-pixel sweep endpoint");
    progress(scene, c, 2, 600);
    const auto second = scene.render(675);
    check(rgb(second[8], 0, 4, 6) && rgb(second[9], 0, 0, 0), "seven-pixel sweep spacing");
    for (unsigned i = 0; i < 8; ++i) check(rgb(second[i], 0, 8, 12), "second sweep changed first section");
    check(rgb(scene.render(1050)[14], 0, 0, 0), "seven-pixel sweep last delay");
    check(rgb(scene.render(1125)[14], 0, 4, 6), "seven-pixel sweep last midpoint");
    snapshotFrame(scene.render(1200), 2, 0, 8, 12, "seven-pixel sweep endpoint");
}

void fastProgressAndPulse() {
    seal::Scene scene;
    seal::Command c;
    fastSequence(scene, c);
    check(scene.snapshot().pulseConsumed && scene.snapshot().pulsePending, "live pulse not reserved");
    check(std::strcmp(scene.apply(c, 250), "duplicate") == 0, "live command duplicate");
    auto conflict = c;
    conflict.authorizePulse = false;
    reject(scene, conflict, 250, "conflict", "same sequence changed pulse grant");
    progress(scene, c, 3, 300);
    const auto at600 = scene.render(600);
    for (unsigned i = 0; i < 8; ++i) check(rgb(at600[i], 0, 8, 12), "fast first sweep was restarted");
    check(rgb(at600[14], 0, 2, 3), "fast second sweep did not retain its start");
    check(rgb(at600[22], 0, 0, 0), "fast third sweep advanced too early");
    const auto at700 = scene.render(700);
    for (unsigned i = 0; i < 15; ++i) check(rgb(at700[i], 0, 8, 12), "fast second sweep endpoint");
    check(rgb(at700[22], 0, 2, 3), "fast third sweep did not retain its start");
    uniform(scene.render(800), 0, 8, 12, "pulse start must be latest sweep endpoint");
    uniform(scene.render(1000), 2, 13, 18, "pulse rising midpoint");
    progress(scene, c, 3, 1100);
    uniform(scene.render(1200), 4, 18, 24, "equal progress cancelled or restarted pulse");
    uniform(scene.render(1400), 2, 13, 18, "pulse falling midpoint");
    uniform(scene.render(1600), 0, 8, 12, "pulse endpoint");
    check(!scene.snapshot().pulsePending && scene.snapshot().pulseConsumed, "pulse completion flags");
    uniform(scene.render(5000), 0, 8, 12, "pulse repeated after completion");
}

void invalidAndRecovery() {
    seal::Scene scene;
    auto c = command(3);
    c.parameters.valid = false;
    apply(scene, c, 0);
    uniform(scene.render(300), 0, 0, 0, "invalid start contributed light");
    check(!scene.snapshot().pulseConsumed, "invalid count consumed pulse");
    c.operation = seal::Operation::Update;
    ++c.sequence;
    c.parameters.valid = true;
    c.parameters.cause = seal::Cause::Progress;
    c.authorizePulse = true;
    apply(scene, c, 300);
    uniform(scene.render(450), 0, 4, 6, "invalid start recovery was not a snapshot");
    uniform(scene.render(600), 0, 8, 12, "invalid start recovery endpoint");
    uniform(scene.render(1000), 0, 8, 12, "recovery played pulse");
    check(scene.snapshot().pulseConsumed && !scene.snapshot().pulsePending, "recovery consumption");

    seal::Scene building;
    c = command();
    apply(building, c, 0);
    progress(building, c, 1, 0);
    c.parameters.valid = false;
    ++c.sequence;
    apply(building, c, 75);
    ++c.sequence;
    c.parameters.count = 3;
    apply(building, c, 150);
    check(rgb(building.render(225)[0], 0, 2, 3), "repeated invalid extended fade");
    uniform(building.render(375), 0, 0, 0, "invalid fade endpoint");
    check(building.snapshot().state == seal::State::Active, "invalid stopped the instance");
    check(!building.snapshot().pulseConsumed, "ignored invalid count consumed pulse");
    c.parameters.valid = true;
    ++c.sequence;
    apply(building, c, 375);
    uniform(building.render(525), 0, 4, 6, "valid recovery with same count was discarded");
    uniform(building.render(675), 0, 8, 12, "valid recovery endpoint");
    check(!building.snapshot().pulsePending, "recovery replayed missed confirmations");

    seal::Scene pulsing;
    fastSequence(pulsing, c);
    pulsing.render(1200);
    c.operation = seal::Operation::Update;
    c.authorizePulse = false;
    c.parameters.valid = false;
    ++c.sequence;
    apply(pulsing, c, 1200);
    check(!pulsing.snapshot().pulsePending && pulsing.snapshot().pulseConsumed,
          "invalid did not cancel pulse and preserve consumption");
    ++c.sequence;
    apply(pulsing, c, 1300);
    uniform(pulsing.render(1350), 2, 9, 12, "invalid did not fade current pulse value");
    uniform(pulsing.render(1500), 0, 0, 0, "repeated invalid during pulse extended fade");
    c.parameters.valid = true;
    ++c.sequence;
    apply(pulsing, c, 1500);
    uniform(pulsing.render(1650), 0, 4, 6, "same-count recovery after pulse was discarded");
    uniform(pulsing.render(1900), 0, 8, 12, "same-count recovery replayed pulse");
}

void stopAndInterruptionPrecision() {
    seal::Scene scene;
    seal::Command c;
    fastSequence(scene, c);
    uniform(scene.render(1200), 4, 18, 24, "stop precondition pulse peak");
    c.operation = seal::Operation::Stop;
    c.authorizePulse = false;
    ++c.sequence;
    apply(scene, c, 1200);
    check(scene.snapshot().state == seal::State::Stopping && !scene.snapshot().pulsePending,
          "stop did not cancel pulse");
    ++c.sequence;
    apply(scene, c, 1250);
    auto update = c;
    update.operation = seal::Operation::Update;
    ++update.sequence;
    reject(scene, update, 1250, "lifecycle", "update accepted while stopping");
    auto restart = update;
    restart.operation = seal::Operation::Start;
    ++restart.generation;
    restart.sequence = 1;
    reject(scene, restart, 1250, "lifecycle", "Start accepted while stopping");
    uniform(scene.render(1350), 2, 9, 12, "stop midpoint did not include current pulse");
    uniform(scene.render(1500), 0, 0, 0, "repeated stop extended fade");
    check(scene.snapshot().state == seal::State::Stopped, "stop did not finish");
    reject(scene, update, 1500, "lifecycle", "update revived stopped instance");
    restart.generation = c.generation;
    restart.sequence = c.sequence + 1;
    reject(scene, restart, 1500, "lifecycle", "same generation restarted after stop");

    seal::Scene precision;
    c = command(1);
    apply(precision, c, 0);
    c.operation = seal::Operation::Stop;
    ++c.sequence;
    apply(precision, c, 75);
    // At interruption green=1.25, blue=1.875 codes; 75 ms into the new fade
    // multiplies by 0.84375. Rounding the captured frame first would yield zero.
    snapshotFrame(precision.render(150), 1, 0, 1, 1, "interruption discarded sub-code precision");
    uniform(precision.render(375), 0, 0, 0, "precision stop endpoint");

    seal::Scene invalid;
    c = command(1);
    apply(invalid, c, 0);
    invalid.render(300);
    c.operation = seal::Operation::Update;
    ++c.sequence;
    c.parameters.valid = false;
    apply(invalid, c, 300);
    c.operation = seal::Operation::Stop;
    ++c.sequence;
    apply(invalid, c, 450);
    snapshotFrame(invalid.render(600), 1, 0, 2, 3, "stop during invalid did not start at current frame");
    uniform(invalid.render(750), 0, 0, 0, "stop during invalid endpoint");
}

void snapshotsCorrectionsAndReconfiguration() {
    seal::Scene scene;
    auto c = command();
    apply(scene, c, 0);
    progress(scene, c, 3, 0, true);
    uniform(scene.render(150), 0, 4, 6, "count jump was not a snapshot");
    uniform(scene.render(700), 0, 8, 12, "count jump played a pulse");
    check(scene.snapshot().pulseConsumed && !scene.snapshot().pulsePending, "jump consumption");

    seal::Scene correction;
    c = command(2);
    apply(correction, c, 0);
    progress(correction, c, 3, 300, true);
    progress(correction, c, 2, 600);
    check(!correction.snapshot().pulsePending, "correction retained pending pulse");
    snapshotFrame(correction.render(900), 2, 0, 8, 12, "correction endpoint");
    progress(correction, c, 3, 900, true);
    uniform(correction.render(1500), 0, 8, 12, "corrected third section endpoint");
    uniform(correction.render(1900), 0, 8, 12, "3-2-3 replayed pulse");

    seal::Scene snapshot;
    fastSequence(snapshot, c);
    snapshot.render(1200);
    c.operation = seal::Operation::Update;
    c.authorizePulse = false;
    ++c.sequence;
    c.parameters.cause = seal::Cause::Snapshot;
    apply(snapshot, c, 1200);
    check(!snapshot.snapshot().pulsePending, "explicit snapshot did not cancel pulse");
    ++c.sequence;
    apply(snapshot, c, 1300);
    uniform(snapshot.render(1350), 2, 13, 18, "snapshot did not capture pulse value");
    uniform(snapshot.render(1500), 0, 8, 12, "identical snapshot restarted ramp");

    seal::Scene config;
    fastSequence(config, c);
    config.render(1200);
    c.operation = seal::Operation::Update;
    c.authorizePulse = false;
    ++c.sequence;
    c.parameters.colour.red = 32;
    c.parameters.colour.green = 0;
    c.parameters.colour.blue = 16;
    c.parameters.sweepMs = 1000;
    apply(config, c, 1200);
    uniform(config.render(1200), 4, 18, 24, "reconfiguration jumped from current value");
    ++c.sequence;
    apply(config, c, 1300);
    uniform(config.render(1350), 18, 9, 20, "reconfiguration midpoint");
    uniform(config.render(1500), 32, 0, 16, "reconfiguration / repeated-input endpoint");
    check(!config.snapshot().pulsePending, "reconfiguration retained pulse");
}

void pulseAuthorityAndGenerations() {
    seal::Scene noGrant;
    auto c = command(2);
    apply(noGrant, c, 0);
    progress(noGrant, c, 3, 300);
    check(noGrant.snapshot().pulseConsumed && !noGrant.snapshot().pulsePending,
          "ungranted completion was not consumed quietly");
    uniform(noGrant.render(1300), 0, 8, 12, "ungranted completion played pulse");

    seal::Scene inherited;
    c = command(2);
    c.inheritedConsumed = true;
    apply(inherited, c, 0);
    progress(inherited, c, 3, 300, true);
    uniform(inherited.render(1300), 0, 8, 12, "inherited pulse consumed flag ignored");

    seal::Scene generation;
    c = command(3);
    apply(generation, c, 0);
    generation.render(300);
    c.operation = seal::Operation::Stop;
    ++c.sequence;
    apply(generation, c, 300);
    generation.render(600);
    c = command(2);
    c.generation = 2;
    apply(generation, c, 600);
    check(generation.snapshot().pulseConsumed, "new generation cleared round-wide consumption");
    progress(generation, c, 3, 900, true);
    uniform(generation.render(1900), 0, 8, 12, "new generation replayed round pulse");
    c = command();
    c.round = 2;
    apply(generation, c, 1900);
    uniform(generation.render(1900), 0, 0, 0, "new round retained old contribution");
    check(!generation.snapshot().pulseConsumed, "new round retained consumed flag");
    progress(generation, c, 1, 1900);
    progress(generation, c, 2, 2000);
    progress(generation, c, 3, 2100, true);
    uniform(generation.render(3100), 4, 18, 24, "new round did not restore one pulse");
}

void identityAndAtomicity() {
    seal::Scene scene;
    auto c = command(1);
    c.round = 10;
    c.generation = 5;
    apply(scene, c, 0);
    reject(scene, c, 0, "duplicate", "exact duplicate not acknowledged");
    auto bad = c;
    bad.parameters.count = 2;
    reject(scene, bad, 75, "conflict", "same-sequence changed payload accepted");
    bad = c;
    ++bad.sequence;
    ++bad.generation;
    reject(scene, bad, 150, "lifecycle", "new generation replaced active instance");
    bad = c;
    bad.operation = seal::Operation::Update;
    ++bad.sequence;
    --bad.round;
    reject(scene, bad, 225, "round", "old round accepted");
    bad.round = c.round + 1;
    reject(scene, bad, 300, "round", "new round update started an instance");
    bad.round = c.round;
    ++bad.generation;
    reject(scene, bad, 375, "generation", "foreign generation accepted");
    progress(scene, c, 2, 450);
    bad = c;
    --bad.sequence;
    reject(scene, bad, 450, "stale", "old sequence accepted");
    bad = c;
    ++bad.sequence;
    bad.operation = seal::Operation::Stop;
    bad.parameters.count = 0;
    reject(scene, bad, 525, "stop-parameters", "stop secretly changed parameters");
    bad = c;
    ++bad.sequence;
    bad.operation = seal::Operation::Start;
    reject(scene, bad, 600, "lifecycle", "same-generation Start accepted while active");
}

void parameterValidation() {
    seal::Scene scene;
    auto c = command(1);
    apply(scene, c, 0);
    for (unsigned which = 0; which < 22; ++which) {
        auto bad = c;
        bad.operation = seal::Operation::Update;
        ++bad.sequence;
        // Formal ranges must still be checked on an invalid input snapshot.
        bad.parameters.valid = false;
        switch (which) {
        case 0: bad.parameters.count = 4; break;
        case 1: bad.parameters.colour.red = 33; break;
        case 2: bad.parameters.colour.green = 33; break;
        case 3: bad.parameters.colour.blue = 33; break;
        case 4: bad.parameters.peak.red = 33; break;
        case 5: bad.parameters.peak.green = 33; break;
        case 6: bad.parameters.peak.blue = 33; break;
        case 7: bad.parameters.transitionMs = 49; break;
        case 8: bad.parameters.transitionMs = 5001; break;
        case 9: bad.parameters.pixelMs = 49; break;
        case 10: bad.parameters.pixelMs = 1001; break;
        case 11: bad.parameters.sweepMs = 149; break;
        case 12: bad.parameters.sweepMs = 5001; break;
        case 13: bad.parameters.pulseMs = 199; break;
        case 14: bad.parameters.pulseMs = 5001; break;
        case 15: bad.parameters.pulseMs = 801; break;
        case 16: bad.parameters.cause = static_cast<seal::Cause>(99); break;
        case 17: bad.operation = static_cast<seal::Operation>(99); break;
        case 18: bad.round = 0; break;
        case 19: bad.generation = 0; break;
        case 20: bad.sequence = 0; break;
        case 21: bad.inheritedConsumed = true; break;
        }
        reject(scene, bad, 0, which == 17 ? "operation" : "parameters", "malformed parameters accepted");
    }
    auto badStart = command();
    badStart.authorizePulse = true;
    seal::Scene empty;
    reject(empty, badStart, 0, "parameters", "Start accepted pulse authorization");
    auto stop = c;
    stop.operation = seal::Operation::Stop;
    ++stop.sequence;
    stop.authorizePulse = true;
    reject(scene, stop, 0, "parameters", "Stop accepted pulse authorization");
    stop.authorizePulse = false;
    stop.inheritedConsumed = true;
    reject(scene, stop, 0, "parameters", "Stop accepted inherited consumption");

    for (unsigned edge = 0; edge < 2; ++edge) {
        seal::Scene bounds;
        auto legal = command(3);
        legal.parameters.colour.red = legal.parameters.colour.green = legal.parameters.colour.blue = 32;
        legal.parameters.peak.red = legal.parameters.peak.green = legal.parameters.peak.blue = 0;
        legal.parameters.pixelMs = edge ? 1000 : 50;
        legal.parameters.sweepMs = edge ? 5000 : 50;
        legal.parameters.transitionMs = edge ? 5000 : 50;
        legal.parameters.pulseMs = edge ? 5000 : 200;
        apply(bounds, legal, 0);
        uniform(bounds.render(legal.parameters.transitionMs / 2), 16, 16, 16,
                "legal duration boundary midpoint");
        uniform(bounds.render(legal.parameters.transitionMs), 32, 32, 32, "legal parameter boundary rejected");

        seal::Scene inversePulse;
        legal.parameters.count = 0;
        apply(inversePulse, legal, 0);
        progress(inversePulse, legal, 1, 0);
        progress(inversePulse, legal, 2, 1);
        progress(inversePulse, legal, 3, 2, true);
        const std::uint32_t pulseStart = 2U + legal.parameters.sweepMs;
        const std::uint32_t half = legal.parameters.pulseMs / 2;
        uniform(inversePulse.render(pulseStart), 32, 32, 32, "boundary sweep / pulse start");
        uniform(inversePulse.render(pulseStart + half / 2), 16, 16, 16,
                "darker peak pulse midpoint / wide arithmetic");
        uniform(inversePulse.render(pulseStart + half), 0, 0, 0,
                "darker peak pulse underflow");
        uniform(inversePulse.render(pulseStart + legal.parameters.pulseMs), 32, 32, 32,
                "boundary pulse endpoint");
    }
}

void resetIdentityFence() {
    seal::Scene empty;
    empty.reset();
    check(empty.snapshot().state == seal::State::Empty, "empty reset created identity");
    uniform(empty.render(0), 0, 0, 0, "empty reset frame");

    seal::Scene scene;
    seal::Command c;
    fastSequence(scene, c);
    scene.render(1200);
    const auto before = scene.snapshot();
    scene.reset();
    const auto after = scene.snapshot();
    check(after.state == seal::State::Stopped && after.round == before.round &&
          after.generation == before.generation && after.sequence == before.sequence &&
          after.pulseConsumed && !after.pulsePending, "reset lost identity / pulse tombstone");
    uniform(scene.render(1200), 0, 0, 0, "reset did not remove contribution immediately");
    reject(scene, c, 1200, "duplicate", "reset did not retain accepted command identity");
    ++c.sequence;
    c.authorizePulse = false;
    reject(scene, c, 1200, "lifecycle", "old update revived reset scene");
    c.operation = seal::Operation::Start;
    reject(scene, c, 1200, "lifecycle", "old generation revived reset scene");
    ++c.generation;
    c.sequence = 1;
    c.parameters.count = 2;
    apply(scene, c, 1200);
    check(scene.snapshot().pulseConsumed, "reset then new generation renewed consumed pulse");
    progress(scene, c, 3, 1500, true);
    uniform(scene.render(2500), 0, 8, 12, "reset then new generation replayed pulse");
}

void timeAndFrameIndependence() {
    const std::uint32_t base = 0xffffff00U;
    seal::Scene wrapped;
    seal::Command c;
    fastSequence(wrapped, c, base);
    uniform(wrapped.render(base + 1000U), 2, 13, 18, "wrap pulse midpoint");
    uniform(wrapped.render(base + 1200U), 4, 18, 24, "wrap pulse peak");
    uniform(wrapped.render(base + 1600U), 0, 8, 12, "wrap pulse endpoint");
    for (std::uint32_t now : {1000000000U, 2000000000U, 3000000000U, 4000000000U, 0U, 1000000000U})
        uniform(wrapped.render(now), 0, 8, 12, "completed animation revived after uptime wrap");

    seal::Scene delayed;
    fastSequence(delayed, c);
    uniform(delayed.render(5000), 0, 8, 12, "late frame replayed missed pulse");
    check(!delayed.snapshot().pulsePending, "late frame did not retire pulse");
    seal::Scene delayedPeak;
    fastSequence(delayedPeak, c);
    uniform(delayedPeak.render(1200), 4, 18, 24, "late frame moved pulse start to render time");

    // Independent scene histories must agree at common observation times.
    for (unsigned step : {17U, 33U, 67U}) {
        seal::Scene sampled, sparse;
        fastSequence(sampled, c);
        fastSequence(sparse, c);
        unsigned sampledAt = 200;
        for (unsigned target : {300U, 600U, 700U, 800U, 1000U, 1200U, 1400U, 1600U, 2000U}) {
            while (sampledAt + step < target) {
                sampledAt += step;
                sampled.render(sampledAt);
            }
            check(sameFrame(sampled.render(target), sparse.render(target)), "frame cadence changed output");
            sampledAt = target;
        }
    }
}
}

int main() {
    try {
        defaultsAndSnapshots();
        sweepGoldenPoints();
        fastProgressAndPulse();
        invalidAndRecovery();
        stopAndInterruptionPrecision();
        snapshotsCorrectionsAndReconfiguration();
        pulseAuthorityAndGenerations();
        identityAndAtomicity();
        parameterValidation();
        resetIdentityFence();
        timeAndFrameIndependence();
        std::cout << "CrystalSeal: 11 groups including independent golden points, lifecycle, atomicity and time passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
