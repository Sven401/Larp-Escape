#include "DeviceRuntime/LocalGeneration.h"
#include "Esp32LocalGeneration.h"
#include "Esp32RulesBootClock.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <esp_partition.h>
#include <freertos/semphr.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <nvs_flash.h>
#include <string>
#include <type_traits>
#include <vector>
using namespace larp::devices;
static unsigned checks = 0, creates = 0, takes = 0, gives = 0, opens = 0, readCalls = 0, sets = 0,
                commits = 0, closes = 0, randoms = 0, enable = 0, disable = 0;
static bool held = false, handles[4] = {}, wrote = false;
static std::string mode, path;
static std::vector<unsigned char> disk, pending;
static std::vector<std::string> events;
static esp_partition_t partition;
static std::int64_t micros = 1000000;
void check(bool v, const char *text) {
    ++checks;
    if (!v) {
        std::cerr << "FAIL " << text << '\n';
        std::exit(1);
    }
}
std::vector<unsigned char> unhex(const std::string &v) {
    std::vector<unsigned char> b;
    for (std::size_t i = 0; i < v.size(); i += 2)
        b.push_back(static_cast<unsigned char>(std::stoul(v.substr(i, 2), nullptr, 16)));
    return b;
}
std::string hex(const unsigned char *p, std::size_t n) {
    static const char *d = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += d[p[i] >> 4];
        s += d[p[i] & 15];
    }
    return s;
}
bool zero(const LocalGenerationSnapshot &s) {
    if (s.generation)
        return false;
    for (auto b : s.boot.bytes)
        if (b)
            return false;
    return true;
}
LocalGenerationSnapshot dirty() {
    LocalGenerationSnapshot s;
    s.generation = UINT64_MAX;
    std::memset(s.boot.bytes, 0xff, 16);
    return s;
}
void persist() {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(disk.data()), disk.size());
    check(f.good(), "synthetic durable medium write");
}
void reentry() {
    if (mode != "reentry")
        return;
    auto &owner = Esp32LocalGeneration::instance();
    auto stale = dirty();
    check(owner.status() == LocalGenerationStatus::Unavailable, "status closed during callback");
    check(!owner.reservation(stale) && zero(stale), "reservation clears during callback");
    check(owner.reserveEarlyStartup(Esp32RulesBootClock::instance()) ==
              LocalGenerationStatus::Unavailable,
          "recursive reservation closed");
}
void event(const std::string &s, bool needsLock = true) {
    if (needsLock)
        check(held, "whole store call under same CAS lock");
    events.push_back(s);
    reentry();
}
static_assert(!std::is_default_constructible<Esp32LocalGeneration>::value,
              "no public owner construction");
static_assert(!std::is_copy_constructible<Esp32LocalGeneration>::value &&
                  !std::is_move_constructible<Esp32LocalGeneration>::value,
              "no singleton copy/move");
static_assert(!std::is_copy_assignable<Esp32LocalGeneration>::value &&
                  !std::is_move_assignable<Esp32LocalGeneration>::value,
              "no owner assignment");
static_assert(!std::is_default_constructible<Esp32LocalGenerationStore>::value,
              "private concrete store");
extern "C" void bootloader_random_enable() {
    ++enable;
}
extern "C" void bootloader_random_disable() {
    ++disable;
}
extern "C" void esp_fill_random(void *p, std::size_t n) {
    ++randoms;
    check(p && n == 16, "actual clock requests sixteen bytes");
    for (std::size_t i = 0; i < n; ++i)
        static_cast<unsigned char *>(p)[i] = static_cast<unsigned char>(i + 1);
}
extern "C" std::int64_t esp_timer_get_time() {
    return micros;
}
extern "C" SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *p) {
    ++creates;
    event("create", false);
    return mode == "mutex-create" ? nullptr : p;
}
extern "C" int xSemaphoreTake(SemaphoreHandle_t p, int wait) {
    ++takes;
    check(p && wait == 0 && !held, "exclusive zero-wait CAS");
    event("take", false);
    if (mode == "mutex-take")
        return 0;
    held = true;
    return pdTRUE;
}
extern "C" int xSemaphoreGive(SemaphoreHandle_t p) {
    ++gives;
    check(p && held, "release exact held lock");
    event("give");
    held = false;
    return pdTRUE;
}
extern "C" const esp_partition_t *esp_partition_find_first(int type, int subtype,
                                                           const char *label) {
    check(type == ESP_PARTITION_TYPE_DATA && subtype == ESP_PARTITION_SUBTYPE_DATA_NVS &&
              std::string(label) == "larpkeys",
          "explicit fixed partition lookup");
    event("partition");
    return mode == "partition-missing" ? nullptr : &partition;
}
extern "C" int nvs_flash_init_partition(const char *p) {
    check(std::string(p) == "larpkeys", "no default partition init");
    event("init");
    return mode == "init" ? 9 : ESP_OK;
}
extern "C" int nvs_open_from_partition(const char *p, const char *ns, int access,
                                       nvs_handle_t *out) {
    ++opens;
    check(std::string(p) == "larpkeys" && std::string(ns) == "larp-local-v1",
          "fixed partition namespace");
    check(opens <= 3 && access == (opens == 2 ? NVS_READWRITE : NVS_READONLY),
          "read before write and fresh read-only verify");
    check(!handles[1] && !handles[2] && !handles[3], "previous handle closed before reopen");
    event("open" + std::to_string(opens));
    if (mode == "open" + std::to_string(opens))
        return 9;
    if (mode == "missing-namespace")
        return ESP_ERR_NVS_NOT_FOUND;
    if (mode == "cas-change" && opens == 2) {
        check(disk.size() == 64, "CAS seed");
        disk[24] ^= 0x80;
        persist();
    }
    if (mode == "verify-mismatch" && opens == 3) {
        disk[25] ^= 0x80;
        persist();
    }
    *out = opens;
    handles[opens] = true;
    return ESP_OK;
}
extern "C" int nvs_get_blob(nvs_handle_t h, const char *key, void *out, std::size_t *n) {
    ++readCalls;
    check(h == opens && handles[h] && std::string(key) == "boot-v1" && n,
          "bounded exact key/handle read");
    check((readCalls % 2 == 1) == (out == nullptr), "length then bounded read each handle");
    event("get" + std::to_string(readCalls) + (out ? "D" : "S"));
    if (mode == "get" + std::to_string(readCalls))
        return 9;
    if (mode == "missing" + std::to_string(readCalls))
        return ESP_ERR_NVS_NOT_FOUND;
    if (!out) {
        *n = disk.size();
        if (mode == "short" + std::to_string(readCalls))
            *n = 63;
        if (mode == "long" + std::to_string(readCalls))
            *n = 65;
        return ESP_OK;
    }
    check(*n == 64, "only sixty-four byte allocation");
    std::memcpy(out, disk.data(), std::min(*n, disk.size()));
    *n = disk.size();
    if (mode == "short" + std::to_string(readCalls))
        *n = 63;
    if (mode == "long" + std::to_string(readCalls))
        *n = 65;
    return ESP_OK;
}
extern "C" int nvs_set_blob(nvs_handle_t h, const char *k, const void *p, std::size_t n) {
    ++sets;
    check(h == 2 && handles[2] && readCalls == 4 && closes == 1 && p && n == 64 &&
              std::string(k) == "boot-v1",
          "only frozen candidate after full read CAS");
    event("set");
    pending.assign(static_cast<const unsigned char *>(p),
                   static_cast<const unsigned char *>(p) + n);
    if (mode == "set-persist") {
        disk = pending;
        persist();
    }
    return mode == "set" || mode == "set-persist" ? 9 : ESP_OK;
}
extern "C" int nvs_commit(nvs_handle_t h) {
    ++commits;
    check(h == 2 && handles[2] && sets == 1, "commit same writer under lock");
    event("commit");
    if (mode != "commit-no-effect") {
        disk = pending;
        persist();
        wrote = true;
    }
    if (mode == "clock-after-write") {
        micros = -1;
        std::uint64_t now = 5;
        check(!Esp32RulesBootClock::instance().now(now) && now == 0,
              "clock failure after potential write closes clock");
    }
    return mode == "commit" || mode == "commit-no-effect" ? 9 : ESP_OK;
}
extern "C" void nvs_close(nvs_handle_t h) {
    ++closes;
    check(h <= 3 && handles[h], "close each actual handle once");
    event("close" + std::to_string(h));
    handles[h] = false;
}
const char *name(LocalGenerationStatus s) {
    switch (s) {
    case LocalGenerationStatus::Unavailable:
        return "Unavailable";
    case LocalGenerationStatus::Reserved:
        return "Reserved";
    case LocalGenerationStatus::Missing:
        return "Missing";
    case LocalGenerationStatus::Corrupt:
        return "Corrupt";
    case LocalGenerationStatus::Overflow:
        return "Overflow";
    case LocalGenerationStatus::Conflict:
        return "Conflict";
    case LocalGenerationStatus::Uncertain:
        return "Uncertain";
    }
    return "INVALID";
}
int codec() {
    std::string raw;
    int valid;
    std::uint64_t generation;
    unsigned rows = 0;
    while (std::cin >> raw >> valid >> generation) {
        auto bytes = unhex(raw);
        check(bytes.size() == 64, "fixed oracle record length");
        LocalGenerationRecord r;
        std::copy(bytes.begin(), bytes.end(), r.bytes.begin());
        const auto original = r;
        auto value = dirty();
        check(decodeLocalGeneration(r, value) == bool(valid),
              "independent CRC/structural oracle decision");
        check(r == original, "decode preserves exact input bytes");
        if (valid) {
            check(value.generation == generation, "full uint64 decode");
            check(!std::memcmp(value.boot.bytes, bytes.data() + 24, 16), "all boot bytes copied");
            LocalGenerationRecord encoded;
            check(encodeLocalGeneration(value, encoded) && encoded == original,
                  "entire independent 64-byte encode oracle");
        } else
            check(zero(value), "rejected decoder clears complete output");
        ++rows;
    }
    LocalGenerationSnapshot invalid;
    LocalGenerationRecord out;
    out.bytes.fill(255);
    check(!encodeLocalGeneration(invalid, out), "zero source rejected");
    check(std::all_of(out.bytes.begin(), out.bytes.end(), [](unsigned char b) { return b == 0; }),
          "failed encoder clears all bytes");
    invalid.generation = 1;
    out.bytes.fill(255);
    check(!encodeLocalGeneration(invalid, out), "nonzero generation cannot replace missing boot");
    std::cout << "CODEC " << rows << ' ' << checks << '\n';
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "codec")
        return codec();
    check(argc == 5, "case medium expected status expected generation");
    mode = argv[1];
    path = argv[2];
    std::ifstream f(path, std::ios::binary);
    disk.assign(std::istreambuf_iterator<char>(f), {});
    auto initial = disk;
    if (mode == "partition-type")
        partition.type = 2;
    if (mode == "partition-subtype")
        partition.subtype = 3;
    if (mode == "partition-label")
        partition.label[0] = 'X';
    if (mode == "partition-address")
        ++partition.address;
    if (mode == "partition-size")
        ++partition.size;
    if (mode == "partition-erase")
        ++partition.erase_size;
    if (mode == "partition-encrypted")
        partition.encrypted = true;
    if (mode == "partition-readonly")
        partition.readonly = true;
    auto &clock = Esp32RulesBootClock::instance();
    auto &owner = Esp32LocalGeneration::instance();
    auto stale = dirty();
    check(!owner.reservation(stale) && zero(stale) &&
              owner.status() == LocalGenerationStatus::Unavailable,
          "no lazy generation reservation");
    check(events.empty(), "no startup IO before explicit request");
    if (mode != "clock-unstarted")
        check(clock.beginEarlyStartup(), "real clock before reservation");
    const auto result = owner.reserveEarlyStartup(clock);
    check(std::string(name(result)) == argv[3], "exact reservation status");
    check(!held && !handles[1] && !handles[2] && !handles[3], "all locks/handles released");
    check(owner.status() == result, "public status stable");
    stale = dirty();
    const bool present = owner.reservation(stale);
    const auto expected = std::stoull(argv[4]);
    check(present == (result == LocalGenerationStatus::Reserved), "no partial exposed reservation");
    if (present) {
        check(stale.generation == expected, "exact reserved uint64");
        for (unsigned i = 0; i < 16; ++i)
            check(stale.boot.bytes[i] == i + 1, "copied boot label not a counter derivation");
        LocalGenerationRecord record;
        check(decodeLocalGeneration(record = LocalGenerationRecord{}, stale) == false,
              "decoder failure cannot affect singleton");
        check(owner.reservation(stale) && stale.generation == expected,
              "copied view independent of singleton");
        LocalGenerationRecord committed;
        check(disk.size() == 64, "durable exact length");
        std::copy(disk.begin(), disk.end(), committed.bytes.begin());
        LocalGenerationSnapshot confirmed;
        check(decodeLocalGeneration(committed, confirmed) && confirmed.generation == expected,
              "only confirmed disk generation exposed");
    } else
        check(zero(stale), "failed reservation clears generation and boot");
    const auto savedEvents = events;
    const auto calls = sets + commits + opens + readCalls + creates + takes + gives;
    const auto randomCalls = randoms;
    for (unsigned i = 0; i < 5; ++i) {
        auto &other = Esp32LocalGeneration::instance();
        check(&other == &owner, "no reconstructed owner identity");
        check(other.reserveEarlyStartup(clock) == result,
              "same process attempt is permanently fixed");
        check(other.status() == result, "same process terminal status immutable");
        stale = dirty();
        check(other.reservation(stale) == present, "same process availability fixed");
        if (!present)
            check(zero(stale), "repeat closed output clear");
    }
    check(events == savedEvents &&
              calls == sets + commits + opens + readCalls + creates + takes + gives &&
              randomCalls == randoms,
          "no retry IO/lock/entropy from alias instance");
    check(sets <= 1 && commits <= 1, "at most one candidate per process");
    if (result == LocalGenerationStatus::Reserved) {
        std::vector<std::string> expectedEvents = {
            "create", "take",  "partition", "init",   "open1", "get1S",  "get2D",
            "close1", "open2", "get3S",     "get4D",  "set",   "commit", "close2",
            "open3",  "get5S", "get6D",     "close3", "give"};
        check(events == expectedEvents, "full set commit close reopen readback CAS order");
        check(enable == 1 && disable == 1 && randoms == 1, "startup entropy once and disabled");
    }
    if (sets == 0)
        check(disk == initial || mode == "cas-change",
              "prewrite errors never repair/create record");
    std::cout << mode << ' ' << name(result) << ' ' << checks << ' '
              << hex(disk.data(), disk.size()) << ' ' << wrote << '\n';
    return 0;
}
