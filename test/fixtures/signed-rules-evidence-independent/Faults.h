#pragma once
struct RecordingSigner final : d0::IFactSigner {
    d0::IFactSigner &delegate;
    std::uint8_t hash[32] = {}, signature[64] = {};
    std::size_t written = 0;
    unsigned calls = 0;
    explicit RecordingSigner(d0::IFactSigner &s) : delegate(s) {}
    d0::SignStatus publicKey(d0::CandidatePublicKey &out) override {
        return delegate.publicKey(out);
    }
    d0::SignStatus signHash(const d0::CandidatePublicKey &key, const std::uint8_t h[32],
                            std::uint8_t s[64], std::size_t &n) override {
        ++calls;
        std::memcpy(hash, h, 32);
        const auto r = delegate.signHash(key, h, s, n);
        written = n;
        if (n == 64)
            std::memcpy(signature, s, 64);
        return r;
    }
};
rp::Application applicationBase(const rp::Application &base, const xr::Plan &plan) {
    rp::Application a;
    a.operation = base.operation;
    a.request = base.request;
    a.catalog = base.catalog;
    a.plan = plan;
    a.overrideRecord = base.overrideRecord;
    return a;
}
bool emptyFrozen(const rp::Application &a, const EvidenceMemory &m) {
    return !a.proofs.count && !a.outbox.count && !a.authorization.count && !m.storage.bytes.count;
}
void runBasicFaults(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                    SignedSet &facts, const rp::Application &base, const ev::ClockWitness &clock) {
    for (unsigned kind = 0; kind < 3; ++kind) {
        EvidenceMemory memory;
        auto app = applicationBase(base, plan);
        memory.bind(app);
        OriginalSources sources(facts);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        facts.signer.get().publicKey(installs.expected);
        sr::IOriginalSources unavailableSources;
        sr::IOriginalInstallations unavailableInstalls;
        sr::IOriginalInputs unavailableInputs;
        sr::SignedPlanEvidence adapter(
            facts.hash, facts.verifier,
            kind == 0 ? static_cast<sr::IOriginalSources &>(unavailableSources) : sources,
            kind == 1 ? static_cast<sr::IOriginalInstallations &>(unavailableInstalls) : installs,
            kind == 2 ? static_cast<sr::IOriginalInputs &>(unavailableInputs) : inputs,
            memory.work);
        const auto original = modelPlan(app.plan, app.overrideRecord);
        const auto r = adapter.freeze(program, before, clock, app, memory.storage);
        requireCodec(r.scope == ev::EvidenceScope::Unavailable && emptyFrozen(app, memory) &&
                         modelPlan(app.plan, app.overrideRecord) == original,
                     "missing original sources/default install/default input stay closed even with "
                     "real keys");
    }
    if (plan.emissions.count) {
        EvidenceMemory memory;
        auto app = applicationBase(base, plan);
        memory.bind(app);
        OriginalSources sources(facts);
        sources.wrongOffer = true;
        RecordingSigner signer(facts.signer.get());
        sources.overrideSigner = &signer;
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        facts.signer.get().publicKey(installs.expected);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                       memory.work);
        const auto r = adapter.freeze(program, before, clock, app, memory.storage);
        requireCodec(
            r.scope == ev::EvidenceScope::Rejected &&
                r.error == ev::EvidenceError::OriginKeyUnavailable && emptyFrozen(app, memory) &&
                signer.calls == 1 && signer.written == 64,
            "actual signer differs from valid offered key: reject, not caller-projected XY trust");
        std::cout << "{\"negativeActualSignature\":{\"hash\":\"" << byteHex(signer.hash, 32)
                  << "\",\"signature\":\"" << byteHex(signer.signature, 64) << "\"}}\n";
        sources.wrongOffer = false;
        requireCodec(adapter.freeze(program, before, clock, app, memory.storage).scope ==
                         ev::EvidenceScope::FrozenOriginal,
                     "wrong offered key healthy reuse");
    }
}
struct Cbor {
    std::vector<std::uint8_t> bytes;
    void head(unsigned major, std::uint64_t n) {
        if (n < 24) {
            bytes.push_back(static_cast<std::uint8_t>((major << 5) | n));
            return;
        }
        unsigned width = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        bytes.push_back(static_cast<std::uint8_t>((major << 5) | (width == 1   ? 24
                                                                  : width == 2 ? 25
                                                                  : width == 4 ? 26
                                                                               : 27)));
        for (unsigned i = width; i; --i)
            bytes.push_back(static_cast<std::uint8_t>(n >> (8 * (i - 1))));
    }
    void u(std::uint64_t n) {
        head(0, n);
    }
    void blob(const std::uint8_t *p, std::size_t n) {
        head(2, n);
        bytes.insert(bytes.end(), p, p + n);
    }
    void text(const char *s) {
        const auto n = std::strlen(s);
        head(3, n);
        bytes.insert(bytes.end(), s, s + n);
    }
    void id(const xr::FactId &v) {
        head(4, 4);
        blob(v.event.bytes, 16);
        blob(v.origin.bytes, 16);
        blob(v.epoch.bytes, 16);
        u(v.sequence);
    }
};
std::vector<std::uint8_t> authBytes(const ev::ClockWitness &clock,
                                    const std::vector<rp::SignedFact> &proofs) {
    Cbor c;
    c.head(5, 5);
    c.u(0);
    c.text("larp.rules.evidence.v1");
    c.u(1);
    c.head(4, 5);
    c.blob(clock.boot.bytes, 16);
    c.u(clock.monotonicBefore);
    c.u(clock.monotonicAfter);
    c.u(clock.activeBefore);
    c.u(clock.activeAfter);
    c.u(2);
    c.blob(ProgramMarker, 3);
    c.u(3);
    c.blob(InputMarker, 3);
    c.u(4);
    c.head(4, proofs.size());
    for (const auto &p : proofs) {
        c.head(4, 3);
        c.id(p.ref.id);
        c.blob(p.ref.digest.bytes, 32);
        c.blob(FactMarker, 3);
    }
    return c.bytes;
}
void closureFaults(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                   SignedSet &facts, const rp::Application &base, const ev::ClockWitness &clock) {
    if (plan.receipt.input.kind != xr::InputKind::Fact || !plan.emissions.count)
        return;
    for (unsigned advance = 0; advance < 2; ++advance) {
        SignedSet variant;
        xr::FactRef future;
        future.id = plan.emissions.data[0].candidate;
        future.id.sequence += advance;
        future.deployment = plan.context.deployment;
        future.binding = plan.context.binding;
        future.run = plan.context.run;
        future.station = plan.context.station;
        future.subject = program.subjects.data[plan.emissions.data[0].subject - 1].binding.subject;
        auto &futureSaved = variant.freeze(future);
        xr::FactRef dependency = futureSaved.ref;
        xr::Evidence deps;
        deps.refs.data = &dependency;
        deps.refs.count = 1;
        auto &trigger = variant.freeze(plan.receipt.input.fact.ref, deps);
        auto input = plan.receipt.input;
        input.fact.ref = trigger.ref;
        PlannerMemory replanned;
        xr::Capacities caps;
        caps.receipts = 1024;
        caps.facts = 256;
        caps.latches = 64;
        caps.outbox = caps.intents = 8;
        requireCodec(
            replanned.planner
                    .plan(program, before, input, caps, replanned.plan, replanned.workspace)
                    .decision == xr::Decision::Ready,
            "actual changed signed trigger replans before closure range test");
        if (!advance)
            requireCodec(sameId(replanned.plan.emissions.data[0].candidate, futureSaved.ref.id),
                         "signed discovered own cause exactly equals new candidate");
        auto app = applicationBase(base, replanned.plan);
        EvidenceMemory memory;
        memory.bind(app);
        OriginalSources sources(facts);
        sources.alternate = &variant;
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        facts.signer.get().publicKey(installs.expected);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                       memory.work);
        auto result = adapter.freeze(program, before, clock, app, memory.storage);
        requireCodec(result.scope == ev::EvidenceScope::Rejected &&
                         result.error == ev::EvidenceError::Range && emptyFrozen(app, memory),
                     "actual valid signed remote trigger -> unissued own closure cause rejected");
        for (const auto &seen : installs.seen)
            requireCodec(!sameId(seen, futureSaved.ref.id),
                         "own future cause fails after math but before installation callback");
        // Historical negative uses independently packed complete original evidence,
        // actual signed bytes and original before.lastIssued, not today's source calls.
        std::vector<rp::SignedFact> proofs;
        for (std::size_t i = 0; i < base.proofs.count; ++i) {
            const auto &p = base.proofs.data[i];
            proofs.push_back(sameId(p.ref.id, trigger.ref.id) ? variant.projection(trigger) : p);
        }
        proofs.push_back(variant.projection(futureSaved));
        SignedSet outSigner;
        std::vector<rp::SignedFact> outgoing;
        for (std::size_t i = 0; i < replanned.plan.emissions.count; ++i) {
            const auto &e = replanned.plan.emissions.data[i];
            xr::FactRef ref;
            ref.id = e.candidate;
            ref.deployment = program.context.deployment;
            ref.binding = program.context.binding;
            ref.run = program.context.run;
            ref.station = program.context.station;
            ref.subject = program.subjects.data[e.subject - 1].binding.subject;
            ref.type = e.type;
            outgoing.push_back(outSigner.projection(outSigner.freeze(ref, e.dependencies)));
        }
        auto historical = applicationBase(base, replanned.plan);
        historical.proofs.data = proofs.data();
        historical.proofs.capacity = historical.proofs.count = proofs.size();
        historical.outbox.data = outgoing.data();
        historical.outbox.capacity = historical.outbox.count = outgoing.size();
        const auto auth = authBytes(clock, proofs);
        historical.authorization.data = auth.data();
        historical.authorization.count = auth.size();
        std::vector<std::uint8_t> bytes(32768);
        Pool<std::uint8_t> encoded;
        encoded.data = bytes.data();
        encoded.capacity = bytes.size();
        requireCodec(rp::encodeApplication(historical, encoded).scope ==
                         rp::Scope::EncodedUnpublished,
                     "complete malicious historical representation encodes without claiming trust");
        AppMemory decoded;
        requireCodec(
            rp::decodeApplication(view(bytes, encoded.count), decoded.app, decoded.storageBytes)
                    .scope == rp::Scope::DecodedUnauthenticated,
            "malicious historical original remains unexecutable representation");
        const auto final = syntheticFinal(decoded.app);
        sources.blocked = true;
        const auto lookups = sources.lookups;
        ev::ClockWitness restored;
        restored.boot.bytes[0] = 0xff;
        result = adapter.historical(program, before, decoded.app, final, restored);
        requireCodec(
            result.scope == ev::EvidenceScope::Rejected &&
                result.error == ev::EvidenceError::Range && sources.lookups == lookups &&
                sameClock(restored, ev::ClockWitness{}),
            "historical own future closure rejected against original before without sources");
        for (const auto &seen : installs.seen)
            requireCodec(!sameId(seen, futureSaved.ref.id),
                         "historical future cause cannot reach installation callback");
        std::cout << "{\"negativeGraph\":{\"kind\":\"own-future\",\"root\":"
                  << modelSigned(variant.projection(trigger))
                  << ",\"extra\":" << modelSigned(variant.projection(futureSaved))
                  << ",\"candidate\":" << modelId(replanned.plan.emissions.data[0].candidate)
                  << ",\"prior\":" << before.fence.lastIssued << "}}\n";
    }
}

void cycleFaults(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                 SignedSet &facts, const rp::Application &base, const ev::ClockWitness &clock) {
    if (plan.receipt.input.kind != xr::InputKind::Fact || !plan.emissions.count)
        return;
    SignedSet variant;
    auto other = plan.receipt.input.fact.ref;
    other.id.sequence = 900;
    xr::FactRef originalRoot = plan.receipt.input.fact.ref;
    xr::Evidence rootDependency;
    rootDependency.refs.data = &originalRoot;
    rootDependency.refs.count = 1;
    auto &extra = variant.freeze(other, rootDependency);
    xr::FactRef extraRef = extra.ref;
    xr::Evidence extraDependency;
    extraDependency.refs.data = &extraRef;
    extraDependency.refs.count = 1;
    auto &root = variant.freeze(originalRoot, extraDependency);
    auto input = plan.receipt.input;
    input.fact.ref = root.ref;
    PlannerMemory replanned;
    xr::Capacities caps;
    caps.receipts = 1024;
    caps.facts = 256;
    caps.latches = 64;
    caps.outbox = caps.intents = 8;
    requireCodec(
        replanned.planner.plan(program, before, input, caps, replanned.plan, replanned.workspace)
                .decision == xr::Decision::Ready,
        "actual cyclic signed trigger replans as pure FactRef input");
    auto app = applicationBase(base, replanned.plan);
    EvidenceMemory memory;
    memory.bind(app);
    OriginalSources sources(facts);
    sources.alternate = &variant;
    SyntheticInstalls installs;
    SyntheticInputs inputs;
    facts.signer.get().publicKey(installs.expected);
    sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                   memory.work);
    auto result = adapter.freeze(program, before, clock, app, memory.storage);
    requireCodec(result.scope == ev::EvidenceScope::Rejected &&
                     result.error == ev::EvidenceError::Dependency && emptyFrozen(app, memory),
                 "actual valid signatures cannot hide a dependency cycle");
    std::vector<rp::SignedFact> proofs;
    for (std::size_t i = 0; i < base.proofs.count; ++i)
        proofs.push_back(sameId(base.proofs.data[i].ref.id, root.ref.id) ? variant.projection(root)
                                                                         : base.proofs.data[i]);
    proofs.push_back(variant.projection(extra));
    SignedSet outgoingSigner;
    std::vector<rp::SignedFact> outgoing;
    for (std::size_t i = 0; i < replanned.plan.emissions.count; ++i) {
        const auto &e = replanned.plan.emissions.data[i];
        xr::FactRef ref;
        ref.id = e.candidate;
        ref.deployment = program.context.deployment;
        ref.binding = program.context.binding;
        ref.run = program.context.run;
        ref.station = program.context.station;
        ref.subject = program.subjects.data[e.subject - 1].binding.subject;
        ref.type = e.type;
        outgoing.push_back(outgoingSigner.projection(outgoingSigner.freeze(ref, e.dependencies)));
    }
    auto historical = applicationBase(base, replanned.plan);
    historical.proofs.data = proofs.data();
    historical.proofs.count = historical.proofs.capacity = proofs.size();
    historical.outbox.data = outgoing.data();
    historical.outbox.count = historical.outbox.capacity = outgoing.size();
    const auto auth = authBytes(clock, proofs);
    historical.authorization.data = auth.data();
    historical.authorization.count = auth.size();
    std::vector<std::uint8_t> bytes(32768);
    Pool<std::uint8_t> encoded;
    encoded.data = bytes.data();
    encoded.capacity = bytes.size();
    requireCodec(rp::encodeApplication(historical, encoded).scope == rp::Scope::EncodedUnpublished,
                 "complete cyclic representation is not claimed authenticated");
    AppMemory decoded;
    requireCodec(
        rp::decodeApplication(view(bytes, encoded.count), decoded.app, decoded.storageBytes)
                .scope == rp::Scope::DecodedUnauthenticated,
        "cyclic original reaches actual historical checker only as unauthenticated");
    sources.blocked = true;
    const auto lookups = sources.lookups;
    ev::ClockWitness restored;
    restored.boot.bytes[0] = 1;
    result =
        adapter.historical(program, before, decoded.app, syntheticFinal(decoded.app), restored);
    requireCodec(result.scope == ev::EvidenceScope::Rejected &&
                     result.error == ev::EvidenceError::Dependency && sources.lookups == lookups &&
                     sameClock(restored, ev::ClockWitness{}),
                 "actual cyclic historical proof rejects without current sources");
    std::cout << "{\"negativeGraph\":{\"kind\":\"cycle\",\"root\":"
              << modelSigned(variant.projection(root))
              << ",\"extra\":" << modelSigned(variant.projection(extra))
              << ",\"candidate\":" << modelId(replanned.plan.emissions.data[0].candidate)
              << ",\"prior\":" << before.fence.lastIssued << "}}\n";
}

void positiveClosure(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                     SignedSet &facts, const rp::Application &base, const ev::ClockWitness &clock) {
    if (plan.receipt.input.kind != xr::InputKind::Fact || !plan.emissions.count)
        return;
    SignedSet variant;
    auto ref = plan.receipt.input.fact.ref;
    ref.id.epoch.bytes[0] ^= 0x11;
    ref.id.sequence = 1;
    auto &extra = variant.freeze(ref);
    xr::FactRef extraRef = extra.ref;
    xr::Evidence dependency;
    dependency.refs.data = &extraRef;
    dependency.refs.count = 1;
    auto &root = variant.freeze(plan.receipt.input.fact.ref, dependency);
    auto input = plan.receipt.input;
    input.fact.ref = root.ref;
    PlannerMemory runtime;
    xr::Capacities caps;
    caps.receipts = 1024;
    caps.facts = 256;
    caps.latches = 64;
    caps.outbox = caps.intents = 8;
    requireCodec(runtime.planner.plan(program, before, input, caps, runtime.plan, runtime.workspace)
                         .decision == xr::Decision::Ready,
                 "actual changed signed trigger has a sufficient direct witness");
    for (std::size_t i = 0; i < runtime.plan.evidence.count; ++i)
        requireCodec(!sameId(extra.ref.id, runtime.plan.evidence.data[i].id),
                     "nonwitness dependency is absent from actual Plan witness pool");
    auto app = applicationBase(base, runtime.plan);
    EvidenceMemory memory;
    memory.bind(app);
    OriginalSources sources(facts);
    sources.alternate = &variant;
    SyntheticInstalls installs;
    SyntheticInputs inputs;
    facts.signer.get().publicKey(installs.expected);
    sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                   memory.work);
    requireCodec(
        adapter.freeze(program, before, clock, app, memory.storage).scope ==
            ev::EvidenceScope::FrozenOriginal,
        "actual complete nonwitness closure frozen under explicit synthetic installation policy");
    bool included = false;
    for (std::size_t i = 0; i < app.proofs.count; ++i)
        included |= sameId(app.proofs.data[i].ref.id, extra.ref.id);
    requireCodec(included && app.proofs.count == base.proofs.count + 1,
                 "actual signed nonwitness closure persisted, not truncated to outgoing witness");
    std::vector<std::uint8_t> catalog(32768), bytes(32768);
    Pool<std::uint8_t> output;
    output.data = catalog.data();
    output.capacity = catalog.size();
    requireCodec(rp::encodeCatalog(program, output).scope == rp::Scope::EncodedUnpublished,
                 "closure positive catalog");
    const auto catCount = output.count;
    output.data = bytes.data();
    output.capacity = bytes.size();
    requireCodec(rp::encodeApplication(app, output).scope == rp::Scope::EncodedUnpublished,
                 "actual closure complete app encode");
    const auto count = output.count;
    AppMemory decoded;
    requireCodec(
        rp::decodeApplication(view(bytes, count), decoded.app, decoded.storageBytes).scope ==
            rp::Scope::DecodedUnauthenticated,
        "actual closure app owns complete proof bytes");
    sources.blocked = true;
    const auto lookups = sources.lookups;
    ev::ClockWitness original;
    requireCodec(
        adapter.historical(program, before, decoded.app, syntheticFinal(decoded.app), original)
                    .scope == ev::EvidenceScope::VerifiedHistorical &&
            sameClock(original, clock) && lookups == sources.lookups,
        "actual nonwitness closure historically verified without source lookup");
    std::cout << "{\"positiveClosure\":{\"codecApplication\":\"" << byteHex(bytes.data(), count)
              << "\",\"model\":" << modelApplication(app) << ",\"catalog\":\""
              << byteHex(catalog.data(), catCount)
              << "\",\"catalogModel\":" << catalogModel(program)
              << ",\"evidenceClock\":" << modelClock(clock)
              << ",\"adapterScopes\":[2,3],\"extra\":" << modelSigned(variant.projection(extra))
              << "}}\n";
}

void projectionFaults(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                      SignedSet &facts, const rp::Application &base,
                      const ev::ClockWitness &clock) {
    if (plan.receipt.input.kind != xr::InputKind::Fact || !plan.emissions.count)
        return;
    SignedSet variant;
    xr::FactRef cause = plan.receipt.input.fact.ref;
    cause.id.sequence = 901;
    xr::Evidence dependency;
    dependency.refs.data = &cause;
    dependency.refs.count = 1;
    auto &actual = variant.freeze(plan.receipt.input.fact.ref, dependency);
    auto projection = variant.projection(actual);
    projection.ref = plan.receipt.input.fact.ref;
    auto app = applicationBase(base, plan);
    EvidenceMemory memory;
    memory.bind(app);
    OriginalSources sources(facts);
    sources.projectionOverride = &projection;
    SyntheticInstalls installs;
    SyntheticInputs inputs;
    facts.signer.get().publicKey(installs.expected);
    sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, sources, installs, inputs,
                                   memory.work);
    auto r = adapter.freeze(program, before, clock, app, memory.storage);
    requireCodec(
        r.scope == ev::EvidenceScope::Rejected && r.error == ev::EvidenceError::Context &&
            emptyFrozen(app, memory),
        "actual valid new signature cannot borrow old claimed ID/payload digest projection");
    std::vector<rp::SignedFact> proofs;
    for (std::size_t i = 0; i < base.proofs.count; ++i)
        proofs.push_back(sameId(base.proofs.data[i].ref.id, projection.ref.id)
                             ? projection
                             : base.proofs.data[i]);
    const auto auth = authBytes(clock, proofs);
    auto historical = applicationBase(base, plan);
    historical.proofs.data = proofs.data();
    historical.proofs.count = historical.proofs.capacity = proofs.size();
    historical.outbox = base.outbox;
    historical.authorization.data = auth.data();
    historical.authorization.count = auth.size();
    std::vector<std::uint8_t> bytes(32768);
    Pool<std::uint8_t> wire;
    wire.data = bytes.data();
    wire.capacity = bytes.size();
    requireCodec(rp::encodeApplication(historical, wire).scope == rp::Scope::EncodedUnpublished,
                 "untrusted mismatched digest projection reaches history as representation only");
    AppMemory decoded;
    requireCodec(
        rp::decodeApplication(view(bytes, wire.count), decoded.app, decoded.storageBytes).scope ==
            rp::Scope::DecodedUnauthenticated,
        "historical projection remains unexecutable");
    sources.blocked = true;
    const auto calls = sources.lookups;
    ev::ClockWitness output;
    output.boot.bytes[0] = 1;
    r = adapter.historical(program, before, decoded.app, syntheticFinal(decoded.app), output);
    requireCodec(r.scope == ev::EvidenceScope::Rejected && r.error == ev::EvidenceError::Context &&
                     sameClock(output, ev::ClockWitness{}) && sources.lookups == calls,
                 "historical actual signature/digest binding rejects old metadata without lookup");
    std::cout << "{\"negativeProjection\":{\"actual\":" << modelSigned(variant.projection(actual))
              << ",\"claimedDigest\":" << Y(projection.ref.digest.bytes, 32) << "}}\n";
}
