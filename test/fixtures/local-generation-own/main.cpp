#include "Esp32LocalGeneration.h"
#include "Esp32RulesBootClock.h"
#include <cstdlib>
#include <cstring>
#include <esp_partition.h>
#include <freertos/semphr.h>
#include <iostream>
#include <nvs_flash.h>
#include <string>
#include <type_traits>
using namespace larp::devices;
namespace {
unsigned checks = 0, opens = 0, readCalls = 0, sets = 0, commits = 0, closes = 0;
bool held = false;
std::string mode;
LocalGenerationRecord disk, pending;
esp_partition_t partition;
void check(bool yes, const char *why) {
    ++checks;
    if (!yes) {
        std::cerr << why << "\n";
        std::exit(1);
    }
}
void closed() {
    LocalGenerationSnapshot value;
    value.generation = 77;
    check(!Esp32LocalGeneration::instance().reservation(value) && !value.generation,
          "no partial reservation exposure");
}
} // namespace
static_assert(!std::is_default_constructible<Esp32LocalGeneration>::value, "singleton only");
static_assert(!std::is_copy_constructible<Esp32LocalGeneration>::value, "no copied owner");
extern "C" void bootloader_random_enable() {}
extern "C" void bootloader_random_disable() {}
extern "C" void esp_fill_random(void *p, std::size_t n) {
    std::memset(p, 7, n);
}
extern "C" std::int64_t esp_timer_get_time() {
    return 1234567;
}
extern "C" SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) {
    return s;
}
extern "C" int xSemaphoreTake(SemaphoreHandle_t, int) {
    check(!held, "exclusive mutex");
    if (mode == "locked")
        return 0;
    held = true;
    return pdTRUE;
}
extern "C" int xSemaphoreGive(SemaphoreHandle_t) {
    check(held, "lock kept until end");
    held = false;
    return pdTRUE;
}
extern "C" const esp_partition_t *esp_partition_find_first(int t, int s, const char *p) {
    check(held && t == 1 && s == 2 && !std::strcmp(p, "larpkeys"), "exact partition");
    return &partition;
}
extern "C" int nvs_flash_init_partition(const char *p) {
    check(held && !std::strcmp(p, "larpkeys"), "explicit NVS partition init");
    return mode == "initfail" ? 9 : 0;
}
extern "C" int nvs_open_from_partition(const char *p, const char *ns, int rw, nvs_handle_t *out) {
    ++opens;
    check(held && !std::strcmp(p, "larpkeys") && !std::strcmp(ns, "larp-local-v1"),
          "only owned namespace");
    check(rw == (opens == 2 ? NVS_READWRITE : NVS_READONLY),
          "RO before writer and fresh RO after close");
    if ((mode == "missing" && opens == 1))
        return ESP_ERR_NVS_NOT_FOUND;
    if (mode == "open" + std::to_string(opens))
        return 9;
    *out = opens;
    return 0;
}
extern "C" int nvs_get_blob(nvs_handle_t h, const char *key, void *out, std::size_t *n) {
    ++readCalls;
    check(held && h == opens && !std::strcmp(key, "boot-v1"), "exact record read");
    if (mode == "readfail" && readCalls == 2)
        return 9;
    if (mode == "conflict" && h == 2) {
        LocalGenerationSnapshot changed;
        check(decodeLocalGeneration(disk, changed), "seed valid");
        ++changed.generation;
        check(encodeLocalGeneration(changed, disk), "foreign write seam");
    }
    if (!out) {
        *n = 64;
        return 0;
    }
    check(*n == 64, "bounded read");
    std::memcpy(out, disk.bytes.data(), 64);
    *n = 64;
    return 0;
}
extern "C" int nvs_set_blob(nvs_handle_t, const char *key, const void *p, std::size_t n) {
    ++sets;
    check(held && closes == 1 && !std::strcmp(key, "boot-v1") && n == 64,
          "single frozen set under complete CAS lock");
    closed();
    check(Esp32LocalGeneration::instance().reserveEarlyStartup(Esp32RulesBootClock::instance()) ==
              LocalGenerationStatus::Unavailable,
          "reentry closed");
    std::memcpy(pending.bytes.data(), p, n);
    return mode == "setfail" ? 9 : 0;
}
extern "C" int nvs_commit(nvs_handle_t) {
    ++commits;
    check(held && sets == 1, "commit under same lock");
    disk = pending;
    if (mode == "mismatch")
        disk.bytes[25] ^= 1;
    return mode == "commitfail" ? 9 : 0;
}
extern "C" void nvs_close(nvs_handle_t) {
    ++closes;
    check(held, "close held");
}
int main(int argc, char **argv) {
    check(argc == 2, "case");
    mode = argv[1];
    auto &clock = Esp32RulesBootClock::instance();
    check(clock.beginEarlyStartup(), "real clock seam started");
    LocalGenerationSnapshot seed;
    seed.generation = mode == "overflow" ? UINT64_MAX : (mode == "max" ? UINT64_MAX - 1 : 9);
    for (auto &b : seed.boot.bytes)
        b = 7;
    check(encodeLocalGeneration(seed, disk), "pre-provisioned test record only");
    if (mode == "corrupt")
        disk.bytes[20] ^= 1;
    if (mode == "partition")
        partition.address = 0;
    LocalGenerationSnapshot before;
    check(decodeLocalGeneration(disk, before) || mode == "corrupt", "codec seed");
    auto &owner = Esp32LocalGeneration::instance();
    closed();
    const auto result = owner.reserveEarlyStartup(clock);
    const bool positive = mode == "good" || mode == "max";
    check((result == LocalGenerationStatus::Reserved) == positive, "reservation classification");
    LocalGenerationSnapshot got;
    if (positive) {
        check(owner.reservation(got), "complete copy after readback");
        check(got.generation == seed.generation + 1, "same Boot128 still increments fullwidth");
        check(sets == 1 && commits == 1 && opens == 3 && closes == 3,
              "full write close reopen readback lifecycle");
        check(!std::memcmp(got.boot.bytes, seed.boot.bytes, 16), "same bytes are not retry key");
    } else
        closed();
    const auto writes = sets, reads = readCalls;
    check(owner.reserveEarlyStartup(clock) == result && sets == writes && readCalls == reads,
          "one attempt success or failure no retry");
    check(!held, "lock released");
    if (!positive && mode != "setfail" && mode != "commitfail" && mode != "mismatch" &&
        mode != "open3")
        check(!sets, "prewrite failures no set");
    if (mode == "setfail" || mode == "commitfail" || mode == "mismatch" || mode == "open3")
        check(result == LocalGenerationStatus::Uncertain,
              "possible effective write stays uncertain");
    LocalGenerationRecord validBasis;
    check(encodeLocalGeneration(seed, validBasis), "valid corruption basis");
    for (unsigned i = 0; i < 64; ++i) {
        auto changed = validBasis;
        changed.bytes[i] ^= 1;
        LocalGenerationSnapshot invalid;
        check(!decodeLocalGeneration(changed, invalid) && !invalid.generation,
              "every single-byte corruption rejected");
    }
    std::cout << checks << " " << mode << " SDK_STUBS_PRESEEDED_NOT_PROVISIONING_POWERLOSS\n";
}
