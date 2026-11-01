#include "Esp32RulesBootClock.h"
#include <bootloader_random.h>
#include <cstdlib>
#include <cstring>
#include <esp_random.h>
#include <esp_timer.h>
#include <iostream>
#include <limits>
#include <type_traits>
static_assert(!std::is_default_constructible<Esp32RulesBootClock>::value, "private singleton");
static_assert(!std::is_copy_constructible<Esp32RulesBootClock>::value, "no copied domain");
static_assert(!std::is_move_constructible<Esp32RulesBootClock>::value, "no moved domain");
namespace {
unsigned checks = 0, enables = 0, fills = 0, disables = 0, timerReads = 0;
bool enabled = false, zero = false, nested = false;
std::int64_t micros = 1234567;
void check(bool ok, const char *why) {
    ++checks;
    if (!ok) {
        std::cerr << why << '\n';
        std::exit(1);
    }
}
void closed() {
    auto &clock = Esp32RulesBootClock::instance();
    larp::devices::rules_progress::ClockObservation out;
    out.boot.bytes[0] = 99;
    out.monotonic = 99;
    std::uint64_t now = 99;
    larp::execution::rules::Identity boot;
    boot.bytes[0] = 99;
    check(!clock.read(out) && !out.boot.bytes[0] && !out.monotonic, "closed coordinator output");
    check(!clock.now(now) && !now, "closed manual output");
    check(!clock.boot(boot) && !boot.bytes[0], "closed boot output");
}
} // namespace
extern "C" void bootloader_random_enable(void) {
    check(!enabled && !enables, "single exclusive entropy enable");
    enabled = true;
    ++enables;
}
extern "C" void esp_fill_random(void *out, std::size_t size) {
    check(enabled && size == 16 && !fills, "exact one 128-bit fill");
    ++fills;
    if (nested) {
        closed();
        check(!Esp32RulesBootClock::instance().beginEarlyStartup(), "nested begin closed");
    }
    for (std::size_t i = 0; i < size; ++i)
        static_cast<unsigned char *>(out)[i] = zero ? 0 : static_cast<unsigned char>(i + 1);
}
extern "C" void bootloader_random_disable(void) {
    check(enabled && fills == 1 && !disables, "entropy released once");
    enabled = false;
    ++disables;
}
extern "C" std::int64_t esp_timer_get_time(void) {
    check(!enabled && disables == 1, "timer sampled after entropy release");
    ++timerReads;
    return micros;
}
int main(int argc, char **argv) {
    check(argc == 2, "case required");
    auto &clock = Esp32RulesBootClock::instance();
    closed();
    check(!enables && !timerReads, "uninitialized calls touch no SDK");
    zero = !std::strcmp(argv[1], "zero");
    nested = !std::strcmp(argv[1], "nested");
    const bool negative = !std::strcmp(argv[1], "negative");
    if (negative)
        micros = -1;
    if (zero || negative) {
        check(!clock.beginEarlyStartup(), "initial failure");
        closed();
        micros = 999999;
        zero = false;
        check(!clock.beginEarlyStartup(), "failure never retries entropy");
        closed();
        check(enables == 1 && fills == 1 && disables == 1, "failed entropy lifecycle frozen");
        check(timerReads == (negative ? 1u : 0u), "no failed domain callbacks");
    } else {
        check(clock.beginEarlyStartup() && &clock == &Esp32RulesBootClock::instance(),
              "actual process singleton ready");
        larp::execution::rules::Identity boot;
        check(clock.boot(boot), "boot copied after readiness");
        for (unsigned i = 0; i < 16; ++i)
            check(boot.bytes[i] == i + 1, "original entropy bytes");
        check(clock.beginEarlyStartup() && enables == 1 && timerReads == 1,
              "repeat begin does not touch SDK");
        larp::devices::rules_progress::IClock &coordinator = clock;
        larp::execution::rules::IManualClock &manual = clock;
        larp::devices::rules_progress::ClockObservation out;
        std::uint64_t now = 0;
        check(coordinator.read(out) && manual.now(now) && out.monotonic == now && now == 1234,
              "same boot ms domain");
        check(!std::memcmp(out.boot.bytes, boot.bytes, 16), "same original boot");
        if (!std::strcmp(argv[1], "regress")) {
            --micros;
            check(!manual.now(now), "raw sub-ms regression closes");
            closed();
            check(!clock.beginEarlyStartup(), "no new boot after regression");
        } else if (!std::strcmp(argv[1], "negative-late")) {
            micros = -1;
            check(!coordinator.read(out), "late signed negative closes");
            closed();
        } else {
            micros = std::numeric_limits<std::int64_t>::max();
            check(manual.now(now) && now == static_cast<std::uint64_t>(micros / 1000),
                  "full signed timer width, no uint32 wrap");
            check(coordinator.read(out) && out.monotonic == now, "same millisecond is valid");
        }
        check(enables == 1 && fills == 1 && disables == 1, "no later entropy acquisition");
    }
    std::cout << checks << " checks " << argv[1] << "; SDK_STUBS_NOT_HARDWARE\n";
}
