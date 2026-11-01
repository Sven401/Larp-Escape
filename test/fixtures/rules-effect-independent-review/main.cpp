#include "DeviceRuntime/RulesProgressEffect.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace r = larp::execution::rules;
namespace d = larp::devices;
namespace e = larp::devices::rules_progress;
static std::size_t checks = 0, good = 0, bad = 0;
static void check(bool b, const char *s) {
    ++checks;
    if (!b) {
        std::cerr << "FAIL " << s << '\n';
        std::exit(1);
    }
}
static std::vector<std::uint8_t> unhex(const std::string &s) {
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < s.size(); i += 2)
        b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return b;
}
static bool zero(const e::Effect &x) {
    return x.intentIndex == 0 && x.expected.progress == 0 && x.expected.activeTime == 0 &&
           std::all_of(x.application.bytes, x.application.bytes + 16,
                       [](std::uint8_t b) { return b == 0; }) &&
           std::all_of(x.catalog.bytes, x.catalog.bytes + 32,
                       [](std::uint8_t b) { return b == 0; }) &&
           std::all_of(x.ownerBoot.bytes, x.ownerBoot.bytes + 16,
                       [](std::uint8_t b) { return b == 0; });
}
static void headers(const e::Effect &x) {
    e::IntentReservation reserve;
    reserve.index = x.intentIndex;
    reserve.claim.bytes[0] = 91;
    reserve.result.bytes[0] = 92;
    d::ProgressRecord h;
    h.kind = x.phase == e::EffectPhase::Claim ? d::ProgressKind::EffectClaim
                                              : d::ProgressKind::EffectResult;
    const auto &op = x.phase == e::EffectPhase::Claim ? reserve.claim : reserve.result;
    std::memcpy(h.operationId.data(), op.bytes, 16);
    h.expectedRevision = UINT64_MAX;
    h.revision = UINT64_MAX;
    check(e::validateEffectRecord(x, h, reserve).valid, "physical revision is not logical fence");
    for (unsigned i = 0; i < 11; ++i) {
        auto rr = reserve;
        auto hh = h;
        if (i == 0)
            ++rr.index;
        if (i == 1)
            rr.claim = {};
        if (i == 2)
            rr.result = {};
        if (i == 3)
            rr.result = rr.claim;
        if (i == 4)
            rr.claim = x.application;
        if (i == 5)
            rr.result = x.application;
        if (i == 6)
            hh.kind = d::ProgressKind::Application;
        if (i == 7)
            hh.operationId[15] ^= 1;
        if (i == 8)
            hh.issueCount = 1;
        if (i == 9)
            hh.firstIssued = UINT64_MAX;
        if (i == 10)
            hh.lastIssued = UINT64_MAX;
        const auto q = e::validateEffectRecord(x, hh, rr);
        check(!q.valid && q.error == e::EffectError::Header,
              "exact immutable phase reservation/header");
    }
}
static void positive(const std::vector<std::uint8_t> &raw, unsigned phase, unsigned result,
                     unsigned index) {
    e::Effect x;
    check(e::decodeEffect(raw.data(), raw.size(), x).valid,
          "actual decode fixed independent bytes");
    const std::uint64_t want[] = {
        0, 23, 24, 255, 256, 65535, 65536, UINT32_MAX, UINT64_C(4294967296), UINT64_MAX};
    const std::uint64_t got[] = {x.expected.progress,  x.expected.profile,  x.expected.trust,
                                 x.expected.facts,     x.expected.ledger,   x.expected.resources,
                                 x.expected.rights,    x.expected.capacity, x.expected.lastIssued,
                                 x.expected.activeTime};
    check(std::equal(got, got + 10, want),
          "all exact uint64 fields preserve independent tuple order");
    check(static_cast<unsigned>(x.phase) == phase && static_cast<unsigned>(x.result) == result &&
              x.intentIndex == index,
          "phase result index exact");
    for (unsigned i = 0; i < 16; ++i)
        check(x.application.bytes[i] == i + 1 && x.ownerBoot.bytes[i] == i + 17,
              "parent and boot bytes exact");
    for (unsigned i = 0; i < 32; ++i)
        check(x.catalog.bytes[i] == i, "catalog full width");
    std::vector<std::uint8_t> buffer(3072);
    r::Pool<std::uint8_t> output;
    output.data = buffer.data();
    output.capacity = buffer.size();
    check(e::encodeEffect(x, output).valid && output.count == raw.size() &&
              std::equal(raw.begin(), raw.end(), buffer.begin()),
          "actual encode equals independent entire byte oracle");
    for (std::size_t n = 0; n < raw.size(); ++n) {
        e::Effect cleared = x;
        const auto q = e::decodeEffect(raw.data(), n, cleared);
        check(!q.valid && zero(cleared), "every truncated prefix clears decoded output");
        output.capacity = n;
        output.count = 99;
        const auto encoded = e::encodeEffect(x, output);
        check(!encoded.valid && encoded.error == e::EffectError::Capacity && output.count == 0,
              "every short capacity clears encoded count");
    }
    output.capacity = raw.size();
    check(e::encodeEffect(x, output).valid && output.count == raw.size(),
          "exact capacity healthy reuse");
    headers(x);
    for (unsigned p = 0; p < 5; ++p)
        for (unsigned v = 0; v < 6; ++v) {
            auto wrong = x;
            wrong.phase = static_cast<e::EffectPhase>(p);
            wrong.result = static_cast<e::HandlerResult>(v);
            const bool allowed = (p == 1 && v == 0) || (p == 2 && v >= 1 && v <= 3);
            output.count = 99;
            check(e::encodeEffect(wrong, output).valid == allowed && (allowed || output.count == 0),
                  "all known/unknown phase handler combinations");
        }
    auto copy = x;
    output.data = reinterpret_cast<std::uint8_t *>(&copy);
    output.capacity = sizeof(copy);
    output.count = 91;
    check(e::encodeEffect(copy, output).error == e::EffectError::Alias && output.count == 91 &&
              copy.intentIndex == x.intentIndex,
          "encode alias preflight preserves shared object/count");
    check(
        e::decodeEffect(reinterpret_cast<const std::uint8_t *>(&copy), sizeof(copy), copy).error ==
                e::EffectError::Alias &&
            copy.intentIndex == x.intentIndex,
        "decode alias preflight preserves object");
    output.data = reinterpret_cast<std::uint8_t *>(&output);
    output.capacity = sizeof(output);
    output.count = 92;
    check(e::encodeEffect(x, output).error == e::EffectError::Alias && output.count == 92,
          "pool wrapper alias blocked");
    output.data = buffer.data();
    output.capacity = buffer.size();
    auto invalid = x;
    invalid.intentIndex = 32;
    check(e::encodeEffect(invalid, output).error == e::EffectError::Value && output.count == 0,
          "reservation profile exact32 rejected");
    invalid = x;
    invalid.intentIndex = UINT16_MAX;
    check(e::encodeEffect(invalid, output).error == e::EffectError::Value,
          "wide index not truncated");
    // Domain classifier deliberately recognizes only a prefix, not full validity.
    check(e::effectDomain(raw.data(), 23), "domain-only prefix recognized");
    e::Effect prefix = x;
    check(!e::decodeEffect(raw.data(), 23, prefix).valid && zero(prefix),
          "recognized domain is not decoded effect");
    auto immutable = raw;
    e::Effect owned;
    check(e::decodeEffect(immutable.data(), immutable.size(), owned).valid,
          "decode before source destruction");
    std::fill(immutable.begin(), immutable.end(), 0);
    check(e::encodeEffect(owned, output).valid &&
              std::equal(raw.begin(), raw.end(), buffer.begin()),
          "decoded fixed values own original source");
}
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        unsigned valid, phase, result, index;
        std::string bytes;
        in >> valid >> phase >> result >> index >> bytes;
        const auto raw = unhex(bytes);
        if (valid) {
            ++good;
            positive(raw, phase, result, index);
        } else {
            ++bad;
            e::Effect sentinel;
            sentinel.application.bytes[0] = 71;
            sentinel.intentIndex = 31;
            const auto q = e::decodeEffect(raw.data(), raw.size(), sentinel);
            check(!q.valid && zero(sentinel), "hostile bytes reject without partial publication");
        }
    }
    e::Effect clear;
    clear.application.bytes[0] = 1;
    check(!e::decodeEffect(nullptr, 0, clear).valid && zero(clear), "null empty decode clearing");
    check(!e::effectDomain(nullptr, 0), "null not domain");
    std::cout << good << " independent positive byte oracles / " << bad << " hostile frames / "
              << checks << " assertions; CODEC_ONLY_NO_OWNER_IO_HARDWARE\n";
}
