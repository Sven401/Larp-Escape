#pragma once
#include "CompanionIntegration/RulesSignedEvidence.h"
#include <functional>
namespace sr = larp::integration::signed_rules;
namespace ev = larp::devices::rules_progress;
const std::uint8_t ProgramMarker[] = {0xa1, 0, 1}, FactMarker[] = {0xa1, 0, 2},
                   InputMarker[] = {0xa1, 0, 3};
xr::Span<std::uint8_t> marker(const std::uint8_t *p) {
    xr::Span<std::uint8_t> s;
    s.data = p;
    s.count = 3;
    return s;
}
bool markerIs(xr::Span<std::uint8_t> s, const std::uint8_t *p) {
    return s.count == 3 && s.data && !std::memcmp(s.data, p, 3);
}
ev::EvidenceResult policyResult(sr::Use use, bool valid) {
    ev::EvidenceResult r;
    r.scope = valid ? (use == sr::Use::CurrentPreparation ? ev::EvidenceScope::FrozenOriginal
                                                          : ev::EvidenceScope::VerifiedHistorical)
                    : ev::EvidenceScope::Rejected;
    r.error = valid ? ev::EvidenceError::None : ev::EvidenceError::Authority;
    return r;
}
// These exact-marker policies are deliberately synthetic. They cannot prove
// installation, Event/GM authorization or trusted clock/ingress in production.
struct SyntheticInstalls final : sr::IOriginalInstallations {
    bool denyCurrent = false;
    unsigned programs = 0, facts = 0;
    std::vector<xr::FactId> seen;
    d0::CandidatePublicKey expected;
    ev::EvidenceResult program(const xr::Program &, xr::Span<std::uint8_t> b, sr::Use u) override {
        ++programs;
        return policyResult(u, markerIs(b, ProgramMarker) &&
                                   !(denyCurrent && u == sr::Use::CurrentPreparation));
    }
    ev::EvidenceResult fact(const xr::Program &program, const d0::FrozenFact &frame,
                            const rp::SignedFact &f, xr::Span<std::uint8_t> b, sr::Use u) override {
        ++facts;
        seen.push_back(f.ref.id);
        return policyResult(
            u, markerIs(b, FactMarker) || markerIs(b, ProgramMarker)
                   ? frame.fact.generation ==
                             (!std::memcmp(f.ref.id.origin.bytes, program.context.box.bytes, 16) &&
                                      !std::memcmp(f.ref.id.epoch.bytes,
                                                   program.context.sourceEpoch.bytes, 16)
                                  ? program.context.bindingGeneration
                                  : 1) &&
                         !std::memcmp(f.publicKey, expected.x, 32) &&
                         !std::memcmp(f.publicKey + 32, expected.y, 32) &&
                         !(denyCurrent && u == sr::Use::CurrentPreparation)
                   : false);
    }
};
struct SyntheticInputs final : sr::IOriginalInputs {
    bool denyCurrent = false;
    unsigned currents = 0, histories = 0;
    ev::EvidenceResult current(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                               const rp::Application &, xr::Span<std::uint8_t> b) override {
        ++currents;
        return policyResult(sr::Use::CurrentPreparation, markerIs(b, InputMarker) && !denyCurrent);
    }
    ev::EvidenceResult historical(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                                  const rp::Application &, xr::Span<std::uint8_t> b) override {
        ++histories;
        return policyResult(sr::Use::OriginalHistorical, markerIs(b, InputMarker));
    }
};
struct OriginalSources final : sr::IOriginalSources {
    SignedSet &saved;
    SignedSet *alternate = nullptr;
    d0::IFactSigner *overrideSigner = nullptr;
    bool blocked = false, wrongOffer = false;
    sr::SourceRead localMode = sr::SourceRead::OriginalBytes,
                   inputMode = sr::SourceRead::OriginalBytes,
                   lookupMode = sr::SourceRead::OriginalBytes;
    unsigned lookupFailureAt = 0;
    xr::Span<std::uint8_t> localAlias, coseAlias;
    const rp::SignedFact *projectionOverride = nullptr;
    std::function<void()> callback;
    unsigned lookups = 0, locals = 0, inputs = 0;
    explicit OriginalSources(SignedSet &s) : saved(s) {}
    sr::SourceRead lookup(const xr::FactId &id, sr::FactView &out) override {
        ++lookups;
        if (blocked)
            return sr::SourceRead::Failure;
        if (lookupMode != sr::SourceRead::OriginalBytes &&
            (!lookupFailureAt || lookups == lookupFailureAt))
            return lookupMode;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        if (projectionOverride && sameId(projectionOverride->ref.id, id)) {
            out.fact = *projectionOverride;
            out.installation = marker(FactMarker);
            return sr::SourceRead::OriginalBytes;
        }
        auto p = alternate ? alternate->find(id) : nullptr;
        if (!p)
            p = saved.find(id);
        if (!p)
            return sr::SourceRead::Missing;
        out.fact = saved.projection(*p);
        if (coseAlias.count)
            out.fact.cose = coseAlias;
        out.installation = marker(FactMarker);
        return sr::SourceRead::OriginalBytes;
    }
    sr::SourceRead local(const xr::Program &, sr::LocalOriginView &out) override {
        ++locals;
        if (blocked)
            return sr::SourceRead::Failure;
        if (localMode != sr::SourceRead::OriginalBytes)
            return localMode;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        out.signer = overrideSigner ? overrideSigner : &saved.signer.get();
        requireCodec(out.signer->publicKey(out.publicKey) == d0::SignStatus::Success,
                     "actual local test signer public key");
        if (wrongOffer) {
            const auto x = unhex(
                           "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978"),
                       y = unhex(
                           "07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1");
            std::memcpy(out.publicKey.x, x.data(), 32);
            std::memcpy(out.publicKey.y, y.data(), 32);
        }
        out.installation = localAlias.count ? localAlias : marker(ProgramMarker);
        return sr::SourceRead::OriginalBytes;
    }
    sr::SourceRead input(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                         const rp::Application &, xr::Span<std::uint8_t> &out) override {
        ++inputs;
        if (blocked)
            return sr::SourceRead::Failure;
        if (inputMode != sr::SourceRead::OriginalBytes)
            return inputMode;
        out = marker(InputMarker);
        return sr::SourceRead::OriginalBytes;
    }
};
struct EvidenceMemory {
    sr::Workspace work;
    ev::EvidenceStorage storage;
    std::vector<std::uint8_t> annotations, catalog, bytes;
    std::vector<sr::OriginRecord> origins;
    std::vector<rp::SignedFact> proofs, outbox;
    EvidenceMemory()
        : annotations(32768), catalog(32768), bytes(32768), origins(256), proofs(256), outbox(8) {
        storagePool(work.annotations, annotations);
        storagePool(work.catalog, catalog);
        storagePool(storage.bytes, bytes);
        storagePool(work.origins, origins);
    }
    template <class T> void storagePool(Pool<T> &p, std::vector<T> &v) {
        p.data = v.data();
        p.capacity = v.size();
        p.count = 0;
    }
    void bind(rp::Application &a) {
        storagePool(a.proofs, proofs);
        storagePool(a.outbox, outbox);
    }
};
bool sameClock(const ev::ClockWitness &a, const ev::ClockWitness &b) {
    return !std::memcmp(a.boot.bytes, b.boot.bytes, 16) && a.monotonicBefore == b.monotonicBefore &&
           a.monotonicAfter == b.monotonicAfter && a.activeBefore == b.activeBefore &&
           a.activeAfter == b.activeAfter;
}
std::string modelClock(const ev::ClockWitness &w) {
    return A({Y(w.boot.bytes, 16), N(w.monotonicBefore), N(w.monotonicAfter), N(w.activeBefore),
              N(w.activeAfter)});
}
larp::devices::ProgressRecord syntheticFinal(const rp::Application &a) {
    larp::devices::ProgressRecord f;
    std::memcpy(f.operationId.data(), a.operation.bytes, 16);
    f.kind = larp::devices::ProgressKind::Application;
    f.expectedLastIssued = a.plan.expected.lastIssued;
    f.profileRevision = a.plan.expected.profile;
    f.trustRevision = a.plan.expected.trust;
    f.issueCount = static_cast<std::uint8_t>(a.plan.emissions.count);
    f.firstIssued = f.issueCount ? f.expectedLastIssued + 1 : 0;
    f.lastIssued = f.issueCount ? f.expectedLastIssued + f.issueCount : 0;
    return f;
}
#include "Boundaries.h"
#include "Faults.h"
void captureSignedEvidence(const xr::Program &program, const xr::State &before,
                           const xr::Plan &plan, SignedSet &facts) {
    rp::Application app;
    app.plan = plan;
    app.overrideRecord = plan.receipt.input.overrideAdmission.record();
    app.operation.bytes[0] = 0x73;
    app.operation.bytes[15] = static_cast<std::uint8_t>(++facts.applications);
    app.request.bytes[0] = 0x74;
    if (plan.receipt.input.kind == xr::InputKind::Override) {
        app.operation = app.overrideRecord.operation;
        app.request = app.overrideRecord.confirmDigest;
    }
    std::vector<std::uint8_t> catalog(32768);
    Pool<std::uint8_t> wire;
    wire.data = catalog.data();
    wire.capacity = catalog.size();
    requireCodec(rp::encodeCatalog(program, wire).scope == rp::Scope::EncodedUnpublished,
                 "actual catalog before evidence");
    const auto catalogBytes = wire.count;
    std::size_t n = 0;
    requireCodec(facts.hash.begin() && facts.hash.update(catalog.data(), wire.count) &&
                     facts.hash.finish(app.catalog.bytes, n) && n == 32 && facts.hash.abort(),
                 "actual catalog SHA");
    EvidenceMemory memory;
    memory.bind(app);
    OriginalSources sources(facts);
    SyntheticInstalls installs;
    SyntheticInputs inputs;
    requireCodec(facts.signer.get().publicKey(installs.expected) == d0::SignStatus::Success,
                 "actual offered public XY");
    sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                   memory.work);
    ev::ClockWitness clock;
    clock.boot.bytes[0] = 0x75;
    if (plan.receipt.input.kind == xr::InputKind::Override)
        clock.boot = app.overrideRecord.ownerBoot;
    clock.activeBefore = before.fence.activeTime;
    clock.activeAfter = plan.expected.activeTime;
    clock.monotonicBefore = 1000 + clock.activeBefore;
    clock.monotonicAfter = 1000 + clock.activeAfter;
    const auto originalPlan = modelPlan(app.plan, app.overrideRecord);
    const auto result = adapter.freeze(program, before, clock, app, memory.storage);
    requireCodec(result.scope == ev::EvidenceScope::FrozenOriginal &&
                     result.error == ev::EvidenceError::None,
                 "actual signed evidence freeze");
    requireCodec(app.plan.ready && modelPlan(app.plan, app.overrideRecord) == originalPlan,
                 "freeze does not alter actual full planner result");
    const auto typed = modelApplication(app);
    std::vector<std::uint8_t> bytes(32768);
    Pool<std::uint8_t> out;
    out.data = bytes.data();
    out.capacity = bytes.size();
    requireCodec(rp::encodeApplication(app, out).scope == rp::Scope::EncodedUnpublished,
                 "frozen original evidence encodes complete app");
    const auto appBytes = out.count;
    AppMemory decoded;
    requireCodec(
        rp::decodeApplication(view(bytes, appBytes), decoded.app, decoded.storageBytes).scope ==
            rp::Scope::DecodedUnauthenticated,
        "actual frozen app decode for historical math");
    const auto lookupCount = sources.lookups, localCount = sources.locals,
               inputCount = sources.inputs, currentCalls = inputs.currents;
    const auto final = syntheticFinal(decoded.app);
    std::vector<SignedSet::Saved> backup;
    for (auto &r : facts.records) {
        backup.push_back(r);
        std::memset(r.fact.original, 0, r.fact.length);
    }
    sources.blocked = true;
    installs.denyCurrent = true;
    inputs.denyCurrent = true;
    ev::ClockWitness restored;
    restored.boot.bytes[0] = 0xff;
    const auto historical = adapter.historical(program, before, decoded.app, final, restored);
    requireCodec(historical.scope == ev::EvidenceScope::VerifiedHistorical &&
                     sameClock(restored, clock),
                 "actual historical signature/closure check");
    requireCodec(sources.lookups == lookupCount && sources.locals == localCount &&
                     sources.inputs == inputCount && inputs.currents == currentCalls,
                 "historical never consults current original sources/input policy");
    requireCodec(modelApplication(decoded.app) == typed && !decoded.app.plan.ready,
                 "historical does not alter saved plan or mint live admission");
    auto saved = facts.records.begin();
    for (const auto &r : backup)
        *saved++ = r;
    runBasicFaults(program, before, plan, facts, app, clock);
    closureFaults(program, before, plan, facts, app, clock);
    cycleFaults(program, before, plan, facts, app, clock);
    positiveClosure(program, before, plan, facts, app, clock);
    projectionFaults(program, before, plan, facts, app, clock);
    boundaryFaults(program, before, plan, facts, app, clock, memory.storage.bytes.count,
                   catalogBytes, decoded.app);
    // Keep each actual outgoing original for a possible later self-input; no resign.
    for (std::size_t i = 0; i < app.outbox.count; ++i) {
        auto &record = app.outbox.data[i];
        if (facts.find(record.ref.id))
            continue;
        facts.records.emplace_back();
        auto &keep = facts.records.back();
        keep.ref = record.ref;
        std::memcpy(keep.publicKey.x, record.publicKey, 32);
        std::memcpy(keep.publicKey.y, record.publicKey + 32, 32);
        Bytes source;
        source.bytes.assign(reinterpret_cast<const char *>(record.cose.data), record.cose.count);
        requireCodec(
            d0::checkFact(source, keep.publicKey, facts.hash, facts.verifier, keep.fact).scope ==
                d0::Scope::SignatureCheckedUnauthenticated,
            "cache exact adapter-produced original COSE");
    }
    std::cout << "{\"codecApplication\":\"" << byteHex(bytes.data(), appBytes)
              << "\",\"model\":" << typed << ",\"catalog\":\""
              << byteHex(catalog.data(), catalogBytes)
              << "\",\"catalogModel\":" << catalogModel(program)
              << ",\"evidenceClock\":" << modelClock(clock) << ",\"adapterScopes\":["
              << unsigned(result.scope) << ',' << unsigned(historical.scope)
              << "],\"writes\":" << facts.writes << "}\n";
}
