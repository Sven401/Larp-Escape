#include "CrystalSeal.h"
#include <algorithm>

namespace larp { namespace led { namespace seal {

static_assert(PixelCount == 23, "CrystalSeal uses the documented 8/7/8 sector map");

Parameters::Parameters() {
    colour.red = 0; colour.green = 8; colour.blue = 12;
    peak.red = 4; peak.green = 18; peak.blue = 24;
}

namespace {
bool samePixel(Pixel a, Pixel b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}
bool limited(Pixel p) { return p.red <= 32 && p.green <= 32 && p.blue <= 32; }
bool sameConfiguration(const Parameters &a, const Parameters &b) {
    return samePixel(a.colour, b.colour) && samePixel(a.peak, b.peak) &&
        a.sweepMs == b.sweepMs && a.pixelMs == b.pixelMs &&
        a.transitionMs == b.transitionMs && a.pulseMs == b.pulseMs;
}
std::array<std::uint32_t, 3> channels(Pixel p) {
    return {{std::uint32_t(p.red) * Scale, std::uint32_t(p.green) * Scale,
             std::uint32_t(p.blue) * Scale}};
}
std::uint32_t smooth(std::uint32_t elapsed, std::uint16_t duration) {
    if (elapsed >= duration) return Scale;
    const std::uint64_t d = elapsed, full = duration;
    // With duration <= 5000, the numerator is <= 1.25e17.
    return static_cast<std::uint32_t>(Scale * d * d * (3U * full - 2U * d) /
                                      (full * full * full));
}
std::uint32_t mix(std::uint32_t from, std::uint32_t to, std::uint32_t amount) {
    return static_cast<std::uint32_t>((std::uint64_t(from) * (Scale - amount) +
                                       std::uint64_t(to) * amount) / Scale);
}
unsigned sector(unsigned index) { return index < 8 ? 0 : (index < 15 ? 1 : 2); }
bool sameCommand(const Command &a, const Command &b) {
    return a.round == b.round && a.generation == b.generation && a.sequence == b.sequence &&
        a.operation == b.operation && equal(a.parameters, b.parameters) &&
        a.inheritedConsumed == b.inheritedConsumed && a.authorizePulse == b.authorizePulse;
}
}

bool valid(const Parameters &p) {
    return p.count <= 3 && (p.cause == Cause::Snapshot || p.cause == Cause::Progress) &&
        limited(p.colour) && limited(p.peak) &&
        p.transitionMs >= 50 && p.transitionMs <= 5000 &&
        p.pixelMs >= 50 && p.pixelMs <= 1000 &&
        p.sweepMs >= p.pixelMs && p.sweepMs <= 5000 &&
        p.pulseMs >= 200 && p.pulseMs <= 5000 && p.pulseMs % 2 == 0;
}

bool equal(const Parameters &a, const Parameters &b) {
    return a.count == b.count && a.valid == b.valid && a.cause == b.cause &&
        sameConfiguration(a, b);
}

Scene::Values Scene::value(std::uint32_t now) const {
    Values result{};
    if (snapshot_.state == State::Empty || snapshot_.state == State::Stopped) return result;
    if (snapshot_.pulsePending) {
        const std::uint32_t elapsed = now - pulseAnchor_;
        if (elapsed >= pulseDelayMs_) {
            const std::uint32_t age = elapsed - pulseDelayMs_;
            const std::uint16_t half = snapshot_.parameters.pulseMs / 2;
            const Channels resting = channels(snapshot_.parameters.colour);
            const Channels peak = channels(snapshot_.parameters.peak);
            Channels current = resting;
            if (age < snapshot_.parameters.pulseMs) {
                const bool rising = age < half;
                const std::uint32_t amount = smooth(rising ? age : age - half, half);
                for (unsigned c = 0; c < 3; ++c)
                    current[c] = mix(rising ? resting[c] : peak[c],
                                     rising ? peak[c] : resting[c], amount);
            }
            result.fill(current);
            return result;
        }
    }
    for (unsigned i = 0; i < PixelCount; ++i) {
        const Ramp &r = ramps_[i];
        if (!r.moving) { result[i] = r.to; continue; }
        const std::uint32_t elapsed = now - r.startedAt;
        if (elapsed <= r.delayMs) { result[i] = r.from; continue; }
        const std::uint32_t amount = smooth(elapsed - r.delayMs, r.durationMs);
        for (unsigned c = 0; c < 3; ++c) result[i][c] = mix(r.from[c], r.to[c], amount);
    }
    return result;
}

void Scene::transition(const Values &from, const Parameters &p, bool dark, std::uint32_t now) {
    snapshot_.pulsePending = false;
    pulseAnchor_ = 0; pulseDelayMs_ = 0;
    const Channels colour = channels(p.colour);
    for (unsigned i = 0; i < PixelCount; ++i) {
        Ramp &r = ramps_[i];
        r.from = from[i];
        r.to = !dark && p.valid && sector(i) < p.count ? colour : Channels{};
        r.startedAt = now; r.delayMs = 0; r.durationMs = p.transitionMs; r.moving = true;
    }
}

void Scene::sweep(const Values &from, const Parameters &p, std::uint32_t now) {
    const unsigned begin = p.count == 1 ? 0 : (p.count == 2 ? 8 : 15);
    const unsigned length = p.count == 2 ? 7 : 8;
    const Channels colour = channels(p.colour);
    for (unsigned j = 0; j < length; ++j) {
        Ramp &r = ramps_[begin + j];
        r.from = from[begin + j]; r.to = colour; r.startedAt = now;
        r.delayMs = static_cast<std::uint16_t>(std::uint32_t(p.sweepMs - p.pixelMs) * j / (length - 1));
        r.durationMs = p.pixelMs; r.moving = true;
    }
}

void Scene::schedulePulse(std::uint32_t now) {
    std::uint32_t remaining = 0;
    for (const Ramp &r : ramps_) if (r.moving) {
        const std::uint32_t duration = std::uint32_t(r.delayMs) + r.durationMs;
        const std::uint32_t elapsed = now - r.startedAt;
        if (elapsed < duration) remaining = std::max(remaining, duration - elapsed);
    }
    pulseAnchor_ = now;
    pulseDelayMs_ = static_cast<std::uint16_t>(remaining);
    snapshot_.pulsePending = true;
}

void Scene::service(std::uint32_t now) {
    if (snapshot_.state == State::Empty || snapshot_.state == State::Stopped) return;
    bool moving = false;
    for (Ramp &r : ramps_) if (r.moving) {
        if (static_cast<std::uint32_t>(now - r.startedAt) >= std::uint32_t(r.delayMs) + r.durationMs) {
            r.from = r.to; r.startedAt = 0; r.delayMs = 0; r.durationMs = 0; r.moving = false;
        } else moving = true;
    }
    if (snapshot_.pulsePending && static_cast<std::uint32_t>(now - pulseAnchor_) >=
        std::uint32_t(pulseDelayMs_) + snapshot_.parameters.pulseMs) {
        snapshot_.pulsePending = false; pulseAnchor_ = 0; pulseDelayMs_ = 0;
    }
    if (snapshot_.state == State::Stopping && !moving) snapshot_.state = State::Stopped;
}

const char *Scene::apply(const Command &c, std::uint32_t now) {
    // Natural completion is allowed even for a rejected command; no command
    // fields are adopted until all formal, identity, and lifecycle checks pass.
    service(now);
    if (!c.round || !c.generation || !c.sequence || !valid(c.parameters)) return "parameters";
    if (c.operation != Operation::Start && c.operation != Operation::Update && c.operation != Operation::Stop)
        return "operation";
    if ((c.inheritedConsumed && c.operation != Operation::Start) ||
        (c.authorizePulse && c.operation != Operation::Update)) return "parameters";
    if (c.round < snapshot_.round) return "round";
    const bool sameRound = c.round == snapshot_.round;
    if (!sameRound && snapshot_.round && c.operation != Operation::Start) return "round";
    if (sameRound) {
        if (c.generation < snapshot_.generation) return "generation";
        if (c.generation == snapshot_.generation) {
            if (c.sequence < snapshot_.sequence) return "stale";
            if (c.sequence == snapshot_.sequence)
                return sameCommand(c, lastCommand_) ? "duplicate" : "conflict";
        }
    }
    if (c.operation == Operation::Start) {
        if (sameRound && (c.generation <= snapshot_.generation ||
            (snapshot_.state != State::Empty && snapshot_.state != State::Stopped))) return "lifecycle";
    } else {
        if (!snapshot_.round || c.generation != snapshot_.generation) return "generation";
        if (snapshot_.state == State::Empty || snapshot_.state == State::Stopped ||
            (c.operation == Operation::Update && snapshot_.state != State::Active)) return "lifecycle";
        if (c.operation == Operation::Stop && !equal(snapshot_.parameters, c.parameters)) return "stop-parameters";
    }

    if (c.operation == Operation::Start) {
        const bool consumed = (sameRound && snapshot_.pulseConsumed) || c.inheritedConsumed;
        ramps_ = {}; snapshot_ = Snapshot{};
        snapshot_.state = State::Active; snapshot_.round = c.round; snapshot_.generation = c.generation;
        snapshot_.pulseConsumed = consumed || (c.parameters.valid && c.parameters.count == 3);
        transition(Values{}, c.parameters, false, now);
        snapshot_.parameters = c.parameters;
    } else if (c.operation == Operation::Update) {
        const Parameters &previous = snapshot_.parameters;
        const bool configChanged = !sameConfiguration(previous, c.parameters);
        const bool enteredSnapshot = previous.cause == Cause::Progress && c.parameters.cause == Cause::Snapshot;
        const bool changed = previous.valid != c.parameters.valid ||
            (c.parameters.valid && (previous.count != c.parameters.count || configChanged || enteredSnapshot));
        const bool liveStep = previous.valid && c.parameters.valid &&
            c.parameters.cause == Cause::Progress && !configChanged &&
            c.parameters.count == previous.count + 1;
        const bool pulse = liveStep && previous.count == 2 && !snapshot_.pulseConsumed && c.authorizePulse;
        if (changed) {
            const Values current = value(now);
            if (liveStep) sweep(current, c.parameters, now);
            else transition(current, c.parameters, !c.parameters.valid, now);
        }
        if (c.parameters.valid && c.parameters.count == 3) snapshot_.pulseConsumed = true;
        snapshot_.parameters = c.parameters;
        if (pulse) schedulePulse(now);
    } else if (snapshot_.state == State::Active) {
        const Values current = value(now);
        transition(current, c.parameters, true, now);
        snapshot_.state = State::Stopping;
    }
    snapshot_.sequence = c.sequence;
    lastCommand_ = c;
    return "applied";
}

Frame Scene::render(std::uint32_t now) {
    service(now);
    const Values current = value(now);
    Frame frame{};
    for (unsigned i = 0; i < PixelCount; ++i) {
        frame[i].red = static_cast<std::uint8_t>(current[i][0] / Scale);
        frame[i].green = static_cast<std::uint8_t>(current[i][1] / Scale);
        frame[i].blue = static_cast<std::uint8_t>(current[i][2] / Scale);
    }
    return frame;
}

void Scene::reset() {
    // Keep the round/generation tombstone and consumed permission across a
    // barrier. A new owner session must explicitly construct a fresh Scene.
    ramps_ = {}; snapshot_.pulsePending = false; pulseAnchor_ = 0; pulseDelayMs_ = 0;
    snapshot_.state = snapshot_.round ? State::Stopped : State::Empty;
}

}}} // namespace larp::led::seal
