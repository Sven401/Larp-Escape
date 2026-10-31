#include "DeviceRuntime/RulesProgressEffect.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
namespace p = larp::devices::rules_progress;
namespace r = larp::execution::rules;
using namespace larp::devices;
static unsigned checks = 0;
static void require(bool ok) {
    ++checks;
    if (!ok) {
        std::cerr << "effect check " << checks << " failed\n";
        std::exit(1);
    }
}
static auto fields(const r::Fence &f)
    -> decltype(std::make_tuple(f.progress, f.profile, f.trust, f.facts, f.ledger, f.resources,
                                f.rights, f.capacity, f.lastIssued, f.activeTime)) {
    return std::make_tuple(f.progress, f.profile, f.trust, f.facts, f.ledger, f.resources, f.rights,
                           f.capacity, f.lastIssued, f.activeTime);
}
static bool same(const p::Effect &a, const p::Effect &b) {
    return !std::memcmp(a.application.bytes, b.application.bytes, 16) &&
           !std::memcmp(a.catalog.bytes, b.catalog.bytes, 32) &&
           !std::memcmp(a.ownerBoot.bytes, b.ownerBoot.bytes, 16) &&
           a.intentIndex == b.intentIndex && a.phase == b.phase && a.result == b.result &&
           fields(a.expected) == fields(b.expected);
}
static p::Effect value(unsigned phase, unsigned result, unsigned index) {
    p::Effect e;
    for (unsigned i = 0; i < 16; ++i) {
        e.application.bytes[i] = i;
        e.ownerBoot.bytes[i] = i + 16;
    }
    for (unsigned i = 0; i < 32; ++i)
        e.catalog.bytes[i] = i;
    e.intentIndex = static_cast<std::uint16_t>(index);
    e.phase = static_cast<p::EffectPhase>(phase);
    e.result = static_cast<p::HandlerResult>(result);
    auto &f = e.expected;
    f.profile = 1;
    f.trust = 23;
    f.facts = 24;
    f.ledger = 255;
    f.resources = 256;
    f.rights = 65535;
    f.capacity = 65536;
    f.lastIssued = UINT64_C(4294967296);
    f.activeTime = UINT64_MAX;
    return e;
}
int main() {
    std::string line;
    unsigned cases = 0;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        unsigned phase = 0, result = 0, index = 0;
        std::string hex;
        input >> phase >> result >> index >> hex;
        require(!hex.empty() && hex.size() % 2 == 0);
        std::vector<std::uint8_t> expected;
        for (std::size_t i = 0; i < hex.size(); i += 2)
            expected.push_back(
                static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        const auto source = value(phase, result, index);
        p::Effect decoded;
        require(p::effectDomain(expected.data(), expected.size()));
        require(p::decodeEffect(expected.data(), expected.size(), decoded).valid &&
                same(decoded, source));
        std::uint8_t bytes[3073] = {};
        r::Pool<std::uint8_t> out;
        out.data = bytes;
        out.capacity = 3073;
        require(p::encodeEffect(source, out).valid && out.count == expected.size() &&
                !std::memcmp(bytes, expected.data(), out.count));
        for (std::size_t n = 0; n < expected.size(); ++n) {
            decoded = source;
            require(!p::decodeEffect(expected.data(), n, decoded).valid &&
                    same(decoded, p::Effect{}));
            out.capacity = n;
            out.count = 123;
            require(!p::encodeEffect(source, out).valid && !out.count);
        }
        out.capacity = 3073;
        require(p::encodeEffect(source, out).valid);
        bytes[out.count] = 0;
        decoded = source;
        require(!p::decodeEffect(bytes, out.count + 1, decoded).valid &&
                same(decoded, p::Effect{}));
        auto changed = expected;
        changed[0] = 0xbf;
        require(!p::decodeEffect(changed.data(), changed.size(), decoded).valid);
        changed = expected;
        changed[1] = 0x18;
        changed.insert(changed.begin() + 2, 0);
        require(!p::decodeEffect(changed.data(), changed.size(), decoded).valid);
        p::IntentReservation reservation;
        reservation.index = source.intentIndex;
        reservation.claim.bytes[0] = 51;
        reservation.result.bytes[0] = 52;
        ProgressRecord record;
        record.kind = phase == 1 ? ProgressKind::EffectClaim : ProgressKind::EffectResult;
        const auto &id = phase == 1 ? reservation.claim : reservation.result;
        std::memcpy(record.operationId.data(), id.bytes, 16);
        record.expectedRevision = 999;
        record.revision = 1000;
        require(p::validateEffectRecord(source, record, reservation).valid);
        const auto clean = record;
        record.kind = ProgressKind::Application;
        require(!p::validateEffectRecord(source, record, reservation).valid);
        record = clean;
        record.issueCount = 1;
        require(!p::validateEffectRecord(source, record, reservation).valid);
        record = clean;
        record.firstIssued = 1;
        require(!p::validateEffectRecord(source, record, reservation).valid);
        record = clean;
        record.lastIssued = 1;
        require(!p::validateEffectRecord(source, record, reservation).valid);
        record = clean;
        record.operationId[1] ^= 1;
        require(!p::validateEffectRecord(source, record, reservation).valid);
        record = clean;
        auto bad = reservation;
        bad.index = static_cast<std::uint16_t>((index + 1) % 32);
        require(!p::validateEffectRecord(source, record, bad).valid);
        bad = reservation;
        bad.result = bad.claim;
        require(!p::validateEffectRecord(source, record, bad).valid);
        bad = reservation;
        bad.claim = source.application;
        require(!p::validateEffectRecord(source, record, bad).valid);
        auto invalid = source;
        invalid.ownerBoot = {};
        require(!p::encodeEffect(invalid, out).valid && !out.count);
        invalid = source;
        invalid.application = {};
        require(!p::encodeEffect(invalid, out).valid && !out.count);
        invalid = source;
        invalid.intentIndex = 32;
        require(!p::encodeEffect(invalid, out).valid && !out.count);
        invalid = source;
        invalid.phase = p::EffectPhase::Claim;
        invalid.result = p::HandlerResult::Returned;
        require(!p::encodeEffect(invalid, out).valid && !out.count);
        invalid = source;
        invalid.phase = p::EffectPhase::Result;
        invalid.result = p::HandlerResult::None;
        require(!p::encodeEffect(invalid, out).valid && !out.count);
        auto mutableSource = source;
        r::Pool<std::uint8_t> alias;
        alias.data = reinterpret_cast<std::uint8_t *>(&mutableSource);
        alias.capacity = sizeof(mutableSource);
        alias.count = 17;
        require(p::encodeEffect(mutableSource, alias).error == p::EffectError::Alias &&
                alias.count == 17 && same(mutableSource, source));
        require(p::decodeEffect(reinterpret_cast<const std::uint8_t *>(&mutableSource),
                                sizeof(mutableSource), mutableSource)
                        .error == p::EffectError::Alias &&
                same(mutableSource, source));
        require(p::encodeEffect(source, out).valid);
        require(p::decodeEffect(out.data, out.count, decoded).valid && same(decoded, source));
        ++cases;
    }
    require(cases == 5);
    std::cout << cases << " fixed Python byte vectors / " << checks
              << " checks; no IO/effect/authority proof\n";
}
