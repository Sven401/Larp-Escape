#include "NearGlow.h"
#include <algorithm>
#include <limits>
namespace larp { namespace led { namespace near {
bool valid(const Parameters &p) {
    return p.level <= 1000 && p.gain <= 1000 && p.duration >= 50 && p.duration <= 5000 &&
        p.colour.red <= 32 && p.colour.green <= 32 && p.colour.blue <= 32 && p.curve <= 1;
}
bool equal(const Parameters &a, const Parameters &b) {
    return a.level == b.level && a.valid == b.valid && a.gain == b.gain &&
        a.duration == b.duration && a.curve == b.curve && a.colour.red == b.colour.red &&
        a.colour.green == b.colour.green && a.colour.blue == b.colour.blue;
}
namespace {
std::array<std::uint32_t, 3> target(const Parameters &p) {
    const std::uint8_t channels[] = {p.colour.red, p.colour.green, p.colour.blue};
    std::array<std::uint32_t, 3> result{};
    if (p.valid) for (unsigned c = 0; c < 3; ++c)
        result[c] = static_cast<std::uint32_t>(std::uint64_t(channels[c]) * Scale * p.level * p.gain / 1000000U);
    return result;
}
}
std::array<std::uint32_t, 3> Scene::value(const Instance &i, std::uint32_t now) {
    if (i.state == State::Empty || i.state == State::Stopped) return {};
    const std::uint32_t d = std::min<std::uint32_t>(now - i.startedAt, i.parameters.duration);
    std::uint64_t numerator = d, denominator = i.parameters.duration;
    if (i.parameters.curve == 1) {
        // Smoothstep d*d*(3*D-2*d) / D^3. Exact integer rational, no early Q rounding.
        numerator = std::uint64_t(d) * d * (3U * i.parameters.duration - 2U * d);
        denominator = std::uint64_t(i.parameters.duration) * i.parameters.duration * i.parameters.duration;
    }
    std::array<std::uint32_t, 3> result{};
    for (unsigned c = 0; c < 3; ++c)
        result[c] = static_cast<std::uint32_t>((std::uint64_t(i.from[c]) * (denominator - numerator) +
                                             std::uint64_t(i.to[c]) * numerator) / denominator);
    // Largest numerator <=32e6 * 5000^3 =4e18 < UINT64_MAX.
    return result;
}
void Scene::service(std::uint32_t now) {
    for (auto &i : instances_) if (i.state == State::Active || i.state == State::Stopping) {
        if (static_cast<std::uint32_t>(now - i.startedAt) >= i.parameters.duration) {
            i.from = i.to; i.startedAt = now;
            if (i.state == State::Stopping) i.state = State::Stopped;
        }
    }
}
const char *Scene::apply(const Command &c, std::uint32_t now) {
    if (c.slot >= Slots || !c.generation || !c.sequence || !valid(c.parameters)) return "parameters";
    auto &i = instances_[c.slot];
    if (i.generation == c.generation && i.state != State::Empty) {
        if (c.sequence < i.sequence) return "stale";
        if (c.sequence == i.sequence)
            return c.operation == i.lastOperation && equal(c.parameters, i.lastParameters) ? "duplicate" : "conflict";
    }
    if (revision_ == std::numeric_limits<std::uint32_t>::max()) return "revision-exhausted";
    if (c.operation == Operation::Start) {
        if ((i.state != State::Empty && i.state != State::Stopped) || c.generation <= i.generation) return "lifecycle";
        i = Instance{}; i.state = State::Active; i.generation = c.generation;
        i.parameters = c.parameters; i.startedAt = now; i.to = target(c.parameters);
    } else {
        if (i.state == State::Empty || i.generation != c.generation) return "generation";
        if (i.state == State::Stopped) return "lifecycle";
        if (c.operation == Operation::Update) {
            if (i.state != State::Active) return "lifecycle";
            const auto next = target(c.parameters);
            if (next != i.to || i.parameters.duration != c.parameters.duration || i.parameters.curve != c.parameters.curve) {
                const auto current = value(i, now);
                i.from = current; i.to = next; i.startedAt = now;
            }
            // Store the full applied snapshot even when equivalent inputs produce the same target.
            // In particular, changing an ignored p while invalid cannot prolong an ongoing fade.
            i.parameters = c.parameters;
        } else if (c.operation == Operation::Stop) {
            if (!equal(i.parameters, c.parameters)) return "stop-parameters";
            if (i.state == State::Active) {
                i.from = value(i, now); i.to = {}; i.startedAt = now; i.state = State::Stopping;
            }
        } else return "operation";
    }
    i.sequence = c.sequence; i.lastOperation = c.operation; i.lastParameters = c.parameters;
    ++revision_;
    return "applied";
}
Frame Scene::render(std::uint32_t now) {
    service(now);
    std::array<std::uint32_t, 3> sum{{background_.red, background_.green, background_.blue}};
    for (const auto &i : instances_) {
        const auto channels = value(i, now);
        for (unsigned c = 0; c < 3; ++c) sum[c] += channels[c] / Scale;
    }
    Pixel result; result.red = static_cast<std::uint8_t>(std::min(32U, sum[0]));
    result.green = static_cast<std::uint8_t>(std::min(32U, sum[1]));
    result.blue = static_cast<std::uint8_t>(std::min(32U, sum[2]));
    Frame frame; frame.fill(result); return frame;
}
bool Scene::background(Pixel p) {
    if (p.red > 32 || p.green > 32 || p.blue > 32 || revision_ == std::numeric_limits<std::uint32_t>::max()) return false;
    background_ = p; ++revision_; return true;
}
void Scene::reset() {
    instances_ = {}; background_ = {};
    if (revision_ != std::numeric_limits<std::uint32_t>::max()) ++revision_;
}
}}}
