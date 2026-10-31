// Implementation boundary tests. Synthetic install/input policy seams are NOT
// production trust or a positive GM/event/installation proof. Real CNG SHA only.
#include "../d0-fact-review/Support.h"
#include "CompanionIntegration/RulesSignedEvidence.h"
#include <cstdlib>
namespace r = larp::execution::rules;
namespace c = larp::execution::rules_progress;
namespace p = larp::devices::rules_progress;
namespace s = larp::integration::signed_rules;
unsigned checks = 0;
void check(bool ok) {
    ++checks;
    if (!ok) {
        std::cerr << "boundary check " << checks << " failed\n";
        std::exit(1);
    }
}
template <typename T, std::size_t N> void bind(r::Pool<T> &p, T (&data)[N]) {
    p.data = data;
    p.capacity = N;
    p.count = 0;
}
struct Memory {
    r::Program program;
    r::StateStorage stateStorage;
    r::State before;
    r::PlannerWorkspace plannerWork;
    r::Plan plan;
    r::Fact facts[8];
    r::Receipt receipts[8];
    r::Id solved[8], proofRefs[64];
    r::FactRef stateEvidence[64], planEvidence[64], catalog[64];
    r::Proof proofs[64];
    char strings[7] = {'b', 'u', 't', 't', 'o', 'n', 0};
    std::uint32_t offsets[1] = {0};
    r::Port ports[1];
    Memory() {
        auto &ctx = program.context;
        ctx.event.bytes[0] = 1;
        ctx.run.bytes[0] = 2;
        ctx.station.bytes[0] = 3;
        ctx.box.bytes[0] = 4;
        ctx.sourceEpoch.bytes[0] = 5;
        ctx.bindingGeneration = 7;
        bind(program.strings, strings);
        program.strings.count = 7;
        bind(program.offsets, offsets);
        program.offsets.count = 1;
        bind(program.ports, ports);
        program.ports.count = 1;
        ports[0].name = 1;
        ports[0].type = r::Type::Boolean;
        ports[0].input = true;
        check(r::validateOwnedProgram(program).decision == r::Decision::Ready);
        program.ready = true;
        bind(stateStorage.facts, facts);
        bind(stateStorage.ledger, receipts);
        bind(stateStorage.solved, solved);
        bind(stateStorage.evidence, stateEvidence);
        r::Fence fence;
        fence.profile = fence.trust = 1;
        check(r::Planner::initialize(program, fence, stateStorage, before).decision ==
              r::Decision::Ready);
        check(r::Planner::control(before, r::Mode::Running).decision == r::Decision::Ready);
        bind(plan.evidence, planEvidence);
        bind(plannerWork.catalog, catalog);
        bind(plannerWork.proofs, proofs);
        bind(plannerWork.proofRefs, proofRefs);
        r::State planning = before;
        check(r::Planner::advanceClock(planning, 5).decision == r::Decision::Ready);
        r::Input input;
        input.context = ctx;
        input.bootGeneration = input.sequence = 1;
        input.port = 1;
        input.type = r::Type::Boolean;
        input.value = 1;
        r::Capacities caps;
        caps.receipts = caps.facts = 8;
        caps.outbox = caps.intents = 8;
        r::Planner planner;
        check(planner.plan(program, planning, input, caps, plan, plannerWork).decision ==
              r::Decision::Ready);
    }
};
struct Sources : s::IOriginalSources {
    unsigned calls = 0;
    bool available = true;
    const std::uint8_t bytes[3] = {1, 2, 3};
    s::SourceRead local(const r::Program &, s::LocalOriginView &out) override {
        ++calls;
        if (!available)
            return s::SourceRead::Unavailable;
        out.installation.data = bytes;
        out.installation.count = 3;
        return s::SourceRead::OriginalBytes;
    }
    s::SourceRead input(const r::Program &, const r::State &, const p::ClockWitness &,
                        const c::Application &, r::Span<std::uint8_t> &out) override {
        ++calls;
        if (!available)
            return s::SourceRead::Unavailable;
        out.data = bytes;
        out.count = 3;
        return s::SourceRead::OriginalBytes;
    }
};
// Explicit synthetic acceptance seams test packing/clearing only. They cannot
// qualify an installation, clock source, input owner, key or authority in product.
p::EvidenceResult seam(s::Use use, r::Span<std::uint8_t> bytes) {
    p::EvidenceResult result;
    if (bytes.count == 3 && bytes.data[0] == 1 && bytes.data[1] == 2 && bytes.data[2] == 3)
        result.scope = use == s::Use::CurrentPreparation ? p::EvidenceScope::FrozenOriginal
                                                         : p::EvidenceScope::VerifiedHistorical;
    else
        result.error = p::EvidenceError::Authority;
    return result;
}
struct Install : s::IOriginalInstallations {
    p::EvidenceResult program(const r::Program &, r::Span<std::uint8_t> b, s::Use u) override {
        return seam(u, b);
    }
};
struct Inputs : s::IOriginalInputs {
    bool bad = false;
    p::EvidenceResult current(const r::Program &, const r::State &, const p::ClockWitness &,
                              const c::Application &, r::Span<std::uint8_t> b) override {
        auto value = seam(s::Use::CurrentPreparation, b);
        if (bad)
            value.error = p::EvidenceError::Authority;
        return value;
    }
    p::EvidenceResult historical(const r::Program &, const r::State &, const p::ClockWitness &,
                                 const c::Application &, r::Span<std::uint8_t> b) override {
        return seam(s::Use::OriginalHistorical, b);
    }
};
int main() {
    Memory memory;
    RealHash hash;
    RealCrypto crypto;
    Sources sources;
    Install install;
    Inputs inputs;
    s::Workspace work;
    std::uint8_t annotations[4096], catalogBytes[4096], evidenceBytes[4096], wireBytes[8192],
        decodedBytes[4096];
    s::OriginRecord origins[8];
    bind(work.annotations, annotations);
    bind(work.catalog, catalogBytes);
    bind(work.origins, origins);
    p::EvidenceStorage storage;
    bind(storage.bytes, evidenceBytes);
    c::Application a;
    a.operation.bytes[0] = 9;
    a.plan = memory.plan;
    r::Pool<std::uint8_t> encoded;
    bind(encoded, wireBytes);
    check(c::encodeCatalog(memory.program, encoded).scope == c::Scope::EncodedUnpublished);
    std::size_t n = 0;
    check(hash.begin() && hash.update(encoded.data, encoded.count) &&
          hash.finish(a.catalog.bytes, n) && n == 32 && hash.abort());
    p::ClockWitness clock;
    clock.boot.bytes[0] = 8;
    clock.monotonicBefore = 100;
    clock.monotonicAfter = 105;
    clock.activeAfter = 5;
    s::SignedPlanEvidence adapter(hash, crypto, sources, install, inputs, work);
    check(adapter.freeze(memory.program, memory.before, clock, a, storage).scope ==
          p::EvidenceScope::FrozenOriginal);
    check(a.plan.ready && memory.before.fence.activeTime == 0 && a.plan.expected.activeTime == 5);
    check(a.authorization.count && !a.proofs.count && !a.outbox.count);
    check(c::encodeApplication(a, encoded).scope == c::Scope::EncodedUnpublished);
    c::Application decoded;
    c::ApplicationStorage decodedStorage;
    bind(decodedStorage.bytes, decodedBytes);
    r::Span<std::uint8_t> wire;
    wire.data = encoded.data;
    wire.count = encoded.count;
    check(c::decodeApplication(wire, decoded, decodedStorage).scope ==
          c::Scope::DecodedUnauthenticated);
    larp::devices::ProgressRecord final;
    final.kind = larp::devices::ProgressKind::Application;
    final.profileRevision = final.trustRevision = 1;
    std::memcpy(final.operationId.data(), a.operation.bytes, 16);
    sources.available = false;
    const auto oldCalls = sources.calls;
    p::ClockWitness recovered;
    check(adapter.historical(memory.program, memory.before, decoded, final, recovered).scope ==
          p::EvidenceScope::VerifiedHistorical);
    check(sources.calls == oldCalls && recovered.activeAfter == 5 && !decoded.plan.ready);
    final.issueCount = 1;
    recovered.activeAfter = 99;
    check(adapter.historical(memory.program, memory.before, decoded, final, recovered).scope ==
              p::EvidenceScope::Rejected &&
          !recovered.activeAfter);
    check(adapter.freeze(memory.program, memory.before, clock, a, storage).scope ==
          p::EvidenceScope::Unavailable);
    check(!a.authorization.count && !a.proofs.count && !a.outbox.count && !storage.bytes.count &&
          a.plan.ready);
    sources.available = true;
    s::IOriginalInstallations closedInstall;
    s::IOriginalInputs closedInput;
    s::SignedPlanEvidence closed(hash, crypto, sources, closedInstall, closedInput, work);
    const auto denied = closed.freeze(memory.program, memory.before, clock, a, storage);
    check(denied.scope == p::EvidenceScope::Unavailable &&
          denied.error == p::EvidenceError::OriginalInstallUnavailable);
    clock.activeAfter = 4;
    check(adapter.freeze(memory.program, memory.before, clock, a, storage).scope ==
          p::EvidenceScope::Rejected);
    clock.activeAfter = 5;
    inputs.bad = true;
    check(adapter.freeze(memory.program, memory.before, clock, a, storage).scope ==
          p::EvidenceScope::Rejected);
    check(!storage.bytes.count && !a.authorization.count && a.plan.ready);
    inputs.bad = false;
    s::SignedPlanEvidence missingInput(hash, crypto, sources, install, closedInput, work);
    check(missingInput.freeze(memory.program, memory.before, clock, a, storage).scope ==
          p::EvidenceScope::Unavailable);
    const auto calls = sources.calls;
    const auto savedStorage = storage.bytes;
    memory.before.ledger.count = 1;
    memory.receipts[0].context.event.bytes[0] = 77;
    storage.bytes.data = reinterpret_cast<std::uint8_t *>(memory.receipts);
    storage.bytes.capacity = sizeof(memory.receipts);
    storage.bytes.count = 42;
    const auto ledgerAlias = adapter.freeze(memory.program, memory.before, clock, a, storage);
    check(ledgerAlias.scope == p::EvidenceScope::Rejected &&
          ledgerAlias.error == p::EvidenceError::Argument);
    check(storage.bytes.count == 42 && memory.receipts[0].context.event.bytes[0] == 77 &&
          sources.calls == calls);
    memory.before.ledger.count = 0;
    storage.bytes = savedStorage;
    memory.before.solved.count = 1;
    memory.solved[0] = 77;
    storage.bytes.data = reinterpret_cast<std::uint8_t *>(memory.solved);
    storage.bytes.capacity = sizeof(memory.solved);
    storage.bytes.count = 7;
    const auto solvedAlias = adapter.freeze(memory.program, memory.before, clock, a, storage);
    check(solvedAlias.scope == p::EvidenceScope::Rejected &&
          solvedAlias.error == p::EvidenceError::Argument);
    check(storage.bytes.count == 7 && memory.solved[0] == 77 && sources.calls == calls);
    memory.before.solved.count = 0;
    storage.bytes = savedStorage;
    r::Emission invalid[1];
    const auto previous = a.plan.emissions;
    invalid[0].dependencies.refs.data = reinterpret_cast<const r::FactRef *>(evidenceBytes);
    invalid[0].dependencies.refs.count = 1;
    bind(a.plan.emissions, invalid);
    a.plan.emissions.count = 1;
    storage.bytes.count = 13;
    const auto invalidView = adapter.freeze(memory.program, memory.before, clock, a, storage);
    check(invalidView.scope == p::EvidenceScope::Rejected &&
          invalidView.error == p::EvidenceError::Argument);
    check(storage.bytes.count == 13 && sources.calls == calls);
    a.plan.emissions = previous;
    storage.bytes = savedStorage;
    std::cout << checks
              << " boundary checks; actual planner + real SHA, SYNTHETIC policy seams, no "
                 "signing/GM/install/D1 positive\n";
}
