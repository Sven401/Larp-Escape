#include "GameExecution/RulesProgressCodec.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
namespace rp = larp::execution::rules_progress;
namespace r = larp::execution::rules;
static unsigned checks = 0;
static void require(bool ok) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "codec check %u failed\n", checks);
        std::exit(1);
    }
}
static r::Context context() {
    r::Context c;
    c.event.bytes[0] = 1;
    c.run.bytes[0] = 2;
    c.station.bytes[0] = 3;
    c.box.bytes[0] = 4;
    c.sourceEpoch.bytes[0] = 5;
    c.bindingGeneration = std::numeric_limits<std::uint64_t>::max();
    return c;
}
static r::Span<std::uint8_t> view(const std::vector<std::uint8_t> &v, std::size_t count) {
    r::Span<std::uint8_t> out;
    out.data = v.data();
    out.count = count;
    return out;
}
int main() {
    // Representation fixtures only: no claim of compiler/planner/authority or
    // cryptographic positive integration from these deliberately empty plans.
    std::vector<std::uint8_t> bytes(rp::MaximumBytes), copy(rp::MaximumBytes);
    r::Pool<std::uint8_t> output;
    output.data = bytes.data();
    output.capacity = bytes.size();
    r::Pool<std::uint8_t> again;
    again.data = copy.data();
    again.capacity = copy.size();
    r::Program original;
    original.context = context();
    original.ready = true;
    require(rp::encodeCatalog(original, output).scope == rp::Scope::EncodedUnpublished);
    const auto catalogBytes = output.count;
    r::Program decoded;
    require(rp::decodeCatalog(view(bytes, catalogBytes), decoded).scope ==
            rp::Scope::DecodedUnauthenticated);
    require(decoded.ready && r::sameContext(original.context, decoded.context));
    require(rp::encodeCatalog(decoded, again).scope == rp::Scope::EncodedUnpublished);
    require(again.count == catalogBytes && !std::memcmp(bytes.data(), copy.data(), catalogBytes));
    for (std::size_t i = 0; i < catalogBytes; ++i) {
        decoded.ready = true;
        require(rp::decodeCatalog(view(bytes, i), decoded).scope == rp::Scope::Rejected);
        require(!decoded.ready);
        output.capacity = i;
        require(rp::encodeCatalog(original, output).scope == rp::Scope::Rejected && !output.count);
    }
    output.capacity = bytes.size();
    require(rp::encodeCatalog(original, output).scope == rp::Scope::EncodedUnpublished);
    bytes[catalogBytes] = 0;
    require(rp::decodeCatalog(view(bytes, catalogBytes + 1), decoded).scope == rp::Scope::Rejected);
    require(rp::decodeCatalog(view(bytes, catalogBytes), decoded).scope ==
            rp::Scope::DecodedUnauthenticated);
    rp::Application source;
    source.operation.bytes[0] = 9;
    source.plan.ready = true;
    source.plan.context = context();
    source.plan.receipt.context = source.plan.receipt.input.context = source.plan.context;
    source.plan.receipt.input.sequence = std::numeric_limits<std::uint64_t>::max();
    source.plan.receipt.input.bootGeneration = source.plan.receipt.input.sequence;
    source.plan.receipt.input.value = std::numeric_limits<std::int32_t>::min();
    source.plan.expected.lastIssued = std::numeric_limits<std::uint64_t>::max();
    require(rp::encodeApplication(source, output).scope == rp::Scope::EncodedUnpublished);
    const auto appBytes = output.count;
    rp::Application dest;
    rp::ApplicationStorage storage;
    require(rp::decodeApplication(view(bytes, appBytes), dest, storage).scope ==
            rp::Scope::DecodedUnauthenticated);
    require(dest.decoded && !dest.plan.ready);
    require(dest.plan.receipt.input.value == std::numeric_limits<std::int32_t>::min());
    require(dest.plan.receipt.input.sequence == source.plan.receipt.input.sequence);
    require(dest.plan.expected.lastIssued == source.plan.expected.lastIssued);
    require(r::sameOverrideRecord(dest.plan.receipt.input.overrideAdmission.record(),
                                  r::OverrideRecord{}));
    for (std::size_t i = 0; i < appBytes; ++i) {
        require(rp::decodeApplication(view(bytes, i), dest, storage).scope == rp::Scope::Rejected);
        require(!dest.decoded && !dest.plan.ready && !dest.plan.admitsFact && !storage.bytes.count);
        output.capacity = i;
        require(rp::encodeApplication(source, output).scope == rp::Scope::Rejected &&
                !output.count);
    }
    output.capacity = bytes.size();
    require(rp::encodeApplication(source, output).scope == rp::Scope::EncodedUnpublished);
    bytes[appBytes] = 0;
    require(rp::decodeApplication(view(bytes, appBytes + 1), dest, storage).scope ==
            rp::Scope::Rejected);
    require(rp::decodeApplication(view(bytes, appBytes), dest, storage).scope ==
            rp::Scope::DecodedUnauthenticated);
    source.plan.receipt.input.kind = r::InputKind::Override;
    require(rp::encodeApplication(source, output).scope == rp::Scope::Rejected && !output.count);
    decoded.ready = true;
    r::Span<std::uint8_t> alias;
    alias.data = reinterpret_cast<const std::uint8_t *>(&decoded);
    alias.count = sizeof(decoded);
    require(rp::decodeCatalog(alias, decoded).error == rp::Error::Alias && decoded.ready);
    dest.decoded = dest.plan.ready = true;
    alias.data = reinterpret_cast<const std::uint8_t *>(&dest);
    alias.count = sizeof(dest);
    require(rp::decodeApplication(alias, dest, storage).error == rp::Error::Alias && dest.decoded &&
            dest.plan.ready);
    r::Pool<std::uint8_t> aliasOutput;
    aliasOutput.data = reinterpret_cast<std::uint8_t *>(&original);
    aliasOutput.capacity = sizeof(original);
    aliasOutput.count = 7;
    require(rp::encodeCatalog(original, aliasOutput).error == rp::Error::Alias && original.ready &&
            aliasOutput.count == 7);
    aliasOutput.data = reinterpret_cast<std::uint8_t *>(&source);
    aliasOutput.capacity = sizeof(source);
    require(rp::encodeApplication(source, aliasOutput).error == rp::Error::Alias &&
            source.plan.ready && aliasOutput.count == 7);
    // Negative raw-envelope fixtures: their bytes are deliberately NOT a valid
    // COSE/signature positive. Duplicate/overflow ranges must fail before encode.
    source.plan.receipt.input.kind = r::InputKind::Local;
    r::Emission emissions[2];
    rp::SignedFact outgoing[2];
    const std::uint8_t invalidCose[] = {0xa0};
    for (unsigned i = 0; i < 2; ++i) {
        auto &id = emissions[i].candidate;
        id.event = source.plan.context.event;
        id.origin = source.plan.context.box;
        id.epoch = source.plan.context.sourceEpoch;
        id.sequence = 1;
        auto &ref = outgoing[i].ref;
        ref.id = id;
        ref.deployment = source.plan.context.deployment;
        ref.binding = source.plan.context.binding;
        ref.run = source.plan.context.run;
        ref.station = source.plan.context.station;
        outgoing[i].cose.data = invalidCose;
        outgoing[i].cose.count = sizeof(invalidCose);
    }
    source.plan.emissions.data = emissions;
    source.plan.emissions.capacity = source.plan.emissions.count = 2;
    source.outbox.data = outgoing;
    source.outbox.capacity = source.outbox.count = 2;
    require(rp::encodeApplication(source, output).scope == rp::Scope::Rejected && !output.count);
    source.plan.expected.lastIssued = 0;
    require(rp::encodeApplication(source, output).scope == rp::Scope::Rejected && !output.count);
    std::printf(
        "%u representation checks; catalog=%u application=%u bytes; no owner/crypto proof\n",
        checks, unsigned(catalogBytes), unsigned(appBytes));
}
