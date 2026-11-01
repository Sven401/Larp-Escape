#include "Esp32RulesBootClock.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
namespace r = larp::execution::rules;
namespace q = larp::devices::rules_progress;
static_assert(!std::is_default_constructible<Esp32RulesBootClock>::value,
              "singleton cannot be reconstructed");
static_assert(!std::is_copy_constructible<Esp32RulesBootClock>::value, "no copied domain");
static_assert(!std::is_move_constructible<Esp32RulesBootClock>::value, "no moved domain");
static_assert(!std::is_copy_assignable<Esp32RulesBootClock>::value, "no replaced domain");
static_assert(!std::is_move_assignable<Esp32RulesBootClock>::value, "no replaced moved domain");
static_assert(std::is_base_of<q::IClock, Esp32RulesBootClock>::value, "actual IClock ABI");
static_assert(std::is_base_of<r::IManualClock, Esp32RulesBootClock>::value,
              "actual IManualClock ABI");
namespace {
unsigned checks = 0, enables = 0, fills = 0, disables = 0, times = 0;
bool active = false, skipFill = false;
std::array<std::uint8_t, 16> randomBytes;
std::int64_t micros = 0;
std::string events;
std::function<void()> onFill, onDisable, onTimer;
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
bool zero(const r::Identity &id) {
    return std::all_of(id.bytes, id.bytes + 16, [](std::uint8_t b) { return b == 0; });
}
void reenter() {
    auto &one = Esp32RulesBootClock::instance();
    const auto beforeTimes = times;
    r::Identity boot;
    std::fill(boot.bytes, boot.bytes + 16, 0xaa);
    q::ClockObservation view;
    std::fill(view.boot.bytes, view.boot.bytes + 16, 0xab);
    view.monotonic = UINT64_MAX;
    std::uint64_t now = UINT64_MAX;
    require(!one.boot(boot) && zero(boot), "reentry cannot publish boot");
    require(!one.read(view) && zero(view.boot) && view.monotonic == 0,
            "reentry cannot publish IClock view");
    require(!one.now(now) && now == 0, "reentry cannot publish manual time");
    require(!one.beginEarlyStartup() && times == beforeTimes,
            "reentry cannot reset or sample domain");
}
void preflight() {
    auto &clock = Esp32RulesBootClock::instance();
    r::Identity boot;
    std::fill(boot.bytes, boot.bytes + 16, 0x11);
    q::ClockObservation out;
    std::fill(out.boot.bytes, out.boot.bytes + 16, 0x12);
    out.monotonic = 92;
    std::uint64_t now = 93;
    require(!clock.boot(boot) && zero(boot), "uninitialized boot clears stale output");
    require(!clock.read(out) && zero(out.boot) && out.monotonic == 0, "uninitialized read closed");
    require(!clock.now(now) && now == 0, "uninitialized manual clock closed");
    require(enables == 0 && fills == 0 && disables == 0 && times == 0,
            "no lazy entropy/timer work from read ports");
}
void failed() {
    auto &clock = Esp32RulesBootClock::instance();
    const auto oldTimes = times;
    randomBytes.fill(1);
    micros = 123456789;
    for (unsigned i = 0; i < 3; ++i) {
        r::Identity boot;
        std::fill(boot.bytes, boot.bytes + 16, 0x22);
        q::ClockObservation out;
        out.monotonic = 99;
        std::fill(out.boot.bytes, out.boot.bytes + 16, 0x23);
        std::uint64_t now = 88;
        require(!clock.beginEarlyStartup(), "failure permanent: cannot reroll startup");
        require(!clock.boot(boot) && zero(boot), "failed boot is never old successful identity");
        require(!clock.read(out) && zero(out.boot) && out.monotonic == 0,
                "failed IClock clears output");
        require(!clock.now(now) && now == 0, "failed manual port clears output");
    }
    require(enables == 1 && fills == 1 && disables == 1 && !active && times == oldTimes,
            "failure retry performs no entropy/timer calls");
}
void started() {
    auto &clock = Esp32RulesBootClock::instance();
    require(clock.beginEarlyStartup(), "actual early begin success");
    require(enables == 1 && fills == 1 && disables == 1 && times == 1 && !active &&
                events == "EFDT",
            "exact entropy enable/fill16/disable BEFORE timer sample");
    r::Identity boot;
    require(clock.boot(boot) && !std::memcmp(boot.bytes, randomBytes.data(), 16),
            "all sixteen original random bytes retained exactly");
    const auto original = boot;
    randomBytes.fill(0x55);
    for (unsigned i = 0; i < 5; ++i)
        require(&Esp32RulesBootClock::instance() == &clock && clock.beginEarlyStartup(),
                "multiple references cannot recreate/reset singleton");
    require(enables == 1 && fills == 1 && disables == 1 && times == 1,
            "repeat begin never rerolls entropy or refreshes timer");
    require(clock.boot(boot) && !std::memcmp(boot.bytes, original.bytes, 16),
            "repeat callers preserve original boot label");
}
void equalPorts() {
    auto &clock = Esp32RulesBootClock::instance();
    q::IClock &owner = clock;
    r::IManualClock &manual = clock;
    q::ClockObservation sample;
    std::uint64_t now = 0;
    r::Identity boot;
    require(owner.read(sample) && manual.now(now) && sample.monotonic == now &&
                now == static_cast<std::uint64_t>(micros / 1000),
            "owner/manual ports share exact ms domain including equal timestamps");
    require(clock.boot(boot) && !std::memcmp(sample.boot.bytes, boot.bytes, 16),
            "owner time shares singleton boot scope");
}
} // namespace
extern "C" void bootloader_random_enable(void) {
    ++enables;
    require(!active, "no nested entropy acquisition");
    active = true;
    events += 'E';
}
extern "C" void esp_fill_random(void *destination, std::size_t count) {
    ++fills;
    require(active && destination && count == 16,
            "actual SDK ABI receives one nonnull exact16byte fill");
    events += 'F';
    if (onFill) {
        auto fn = onFill;
        onFill = {};
        fn();
    }
    if (!skipFill)
        std::memcpy(destination, randomBytes.data(), count);
}
extern "C" void bootloader_random_disable(void) {
    ++disables;
    require(active, "release actual acquired entropy");
    active = false;
    events += 'D';
    if (onDisable) {
        auto fn = onDisable;
        onDisable = {};
        fn();
    }
}
extern "C" std::int64_t esp_timer_get_time(void) {
    ++times;
    require(!active, "entropy released even before negative/error clock sample");
    events += 'T';
    if (onTimer) {
        auto fn = onTimer;
        onTimer = {};
        fn();
    }
    return micros;
}
int main(int argc, char **argv) {
    require(argc == 2, "one isolated fresh-process scenario");
    const std::string name = argv[1];
    for (unsigned i = 0; i < 16; ++i)
        randomBytes[i] = static_cast<std::uint8_t>(i + 1);
    preflight();
    if (name == "zero-identity" || name == "unwritten-entropy" || name == "negative-start") {
        if (name == "zero-identity")
            randomBytes.fill(0);
        if (name == "unwritten-entropy")
            skipFill = true;
        if (name == "negative-start")
            micros = -1;
        require(!Esp32RulesBootClock::instance().beginEarlyStartup(),
                "first actual startup failure closes");
        require(events == (name == "negative-start" ? "EFDT" : "EFD"),
                "entropy failure still disables and never invents clock success");
        failed();
    } else {
        if (name == "basic")
            micros = 1000123;
        if (name == "wide")
            micros = (INT64_C(4294967296) + 33) * 1000 + 999;
        if (name == "maximum")
            micros = std::numeric_limits<std::int64_t>::max();
        if (name == "last-byte-only") {
            randomBytes.fill(0);
            randomBytes[15] = 1;
        }
        if (name == "reenter-fill")
            onFill = reenter;
        if (name == "reenter-disable")
            onDisable = reenter;
        if (name == "reenter-start-timer")
            onTimer = reenter;
        started();
        equalPorts();
        if (name == "negative-read") {
            micros = -1;
            q::ClockObservation out;
            out.monotonic = 99;
            require(!Esp32RulesBootClock::instance().read(out) && zero(out.boot) &&
                        out.monotonic == 0,
                    "negative SDK time closes after successful begin");
            failed();
        } else if (name == "micro-regression" || name == "cross-port-regression") {
            auto &clock = Esp32RulesBootClock::instance();
            micros = 2499;
            std::uint64_t now = 0;
            q::ClockObservation out;
            if (name == "micro-regression") {
                require(clock.now(now) && now == 2, "manual clock advances shared micro fence");
                micros = 2498;
                require(!clock.read(out) && zero(out.boot) && out.monotonic == 0,
                        "even regression WITHIN same ms closes domain");
            } else {
                require(clock.read(out) && out.monotonic == 2,
                        "owner clock advances shared micro fence");
                micros = 2498;
                require(!clock.now(now) && now == 0,
                        "manual port cannot bypass owner-clock regression fence");
            }
            failed();
        } else if (name == "reenter-sample") {
            onTimer = reenter;
            micros += 1000;
            q::ClockObservation out;
            require(Esp32RulesBootClock::instance().read(out) && out.monotonic == 1,
                    "recursive calls do not corrupt outer successful sample");
            equalPorts();
        } else {
            require(name == "zero-time" || name == "basic" || name == "wide" || name == "maximum" ||
                        name == "last-byte-only" || name == "reenter-fill" ||
                        name == "reenter-disable" || name == "reenter-start-timer",
                    "known positive scenario");
            if (name == "wide")
                require(static_cast<std::uint64_t>(micros / 1000) > UINT32_MAX,
                        "actual millisecond result exceeds32bit");
        }
    }
    std::cout << name << ' ' << checks
              << " SDK_SEAM_ASSERTIONS_NO_HARDWARE_ENTROPY_SLEEP_QUALIFICATION\n";
}
