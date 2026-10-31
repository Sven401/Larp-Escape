#pragma once
#include "Faults.h"
struct CryptoFailure final : d0::IFactCrypto {
    d0::IFactCrypto &real;
    unsigned mode = 0, calls = 0;
    explicit CryptoFailure(d0::IFactCrypto &r) : real(r) {}
    bool reset() override {
        const bool ok = real.reset();
        return ok && mode != 4;
    }
    d0::VerifyResult verifyHash(const d0::CandidatePublicKey &k, const std::uint8_t h[32],
                                const std::uint8_t s[64]) override {
        ++calls;
        const auto r = real.verifyHash(k, h, s);
        return mode == 1   ? d0::VerifyResult::InvalidSignature
               : mode == 2 ? d0::VerifyResult::Unavailable
               : mode == 3 ? d0::VerifyResult::Failure
                           : r;
    }
};
void boundaryFaults(const xr::Program &program, const xr::State &before, const xr::Plan &plan,
                    SignedSet &facts, const rp::Application &base, const ev::ClockWitness &clock,
                    std::size_t byteCount, std::size_t catalogBytes,
                    const rp::Application &decoded) {
    const auto planBefore = modelPlan(plan, base.overrideRecord);
    const auto setup = [&](SyntheticInstalls &p) {
        requireCodec(facts.signer.get().publicKey(p.expected) == d0::SignStatus::Success,
                     "real expected key for boundary");
    };
    // Exact and one-short capacities are measured from actual complete frozen data.
    for (unsigned field = 0; field < 5; ++field) {
        const auto need = field == 0   ? byteCount
                          : field == 1 ? base.proofs.count
                          : field == 2 ? base.outbox.count
                          : field == 3 ? base.proofs.count
                                       : catalogBytes;
        if (!need)
            continue;
        for (bool exact : {false, true}) {
            EvidenceMemory mem;
            auto app = applicationBase(base, plan);
            mem.bind(app);
            if (field == 0)
                mem.storage.bytes.capacity = need - (exact ? 0 : 1);
            if (field == 1)
                app.proofs.capacity = need - (exact ? 0 : 1);
            if (field == 2)
                app.outbox.capacity = need - (exact ? 0 : 1);
            if (field == 3)
                mem.work.origins.capacity = need - (exact ? 0 : 1);
            if (field == 4)
                mem.work.catalog.capacity = need - (exact ? 0 : 1);
            OriginalSources source(facts);
            SyntheticInstalls installs;
            SyntheticInputs inputs;
            setup(installs);
            sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                           mem.work);
            const auto r = adapter.freeze(program, before, clock, app, mem.storage);
            if (!(exact ? r.scope == ev::EvidenceScope::FrozenOriginal
                        : (r.scope == ev::EvidenceScope::Rejected &&
                           r.error == ev::EvidenceError::Capacity && emptyFrozen(app, mem))))
                std::cerr << "CAPDIAG field=" << field << " need=" << need << " exact=" << exact
                          << " scope=" << unsigned(r.scope) << " error=" << unsigned(r.error)
                          << '\n';
            requireCodec(exact ? r.scope == ev::EvidenceScope::FrozenOriginal
                               : (r.scope == ev::EvidenceScope::Rejected &&
                                  r.error == ev::EvidenceError::Capacity && emptyFrozen(app, mem)),
                         "actual required arena capacity exact / -1");
            requireCodec(modelPlan(app.plan, app.overrideRecord) == planBefore,
                         "capacity failure never edits actual Plan");
        }
    }
    // Every output/workspace capacity participates in overlap checks before resets.
    {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        mem.storage.bytes.data = mem.work.annotations.data;
        mem.storage.bytes.count = 17;
        app.proofs.count = 1;
        OriginalSources source(facts);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        const auto r = adapter.freeze(program, before, clock, app, mem.storage);
        requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                         r.error == ev::EvidenceError::Argument && app.proofs.count == 1 &&
                         mem.storage.bytes.count == 17,
                     "invalid overlap rejected before touching previous output counts");
    }
    {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        OriginalSources source(facts);
        std::memcpy(mem.work.annotations.data, ProgramMarker, 3);
        source.localAlias.data = mem.work.annotations.data;
        source.localAlias.count = 3;
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        const auto r = adapter.freeze(program, before, clock, app, mem.storage);
        requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                         r.error == ev::EvidenceError::Argument && emptyFrozen(app, mem) &&
                         installs.programs == 0,
                     "source installation view cannot alias unused workspace capacity");
    }
    if (base.proofs.count) {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        OriginalSources source(facts);
        source.coseAlias.data = mem.storage.bytes.data + 100;
        source.coseAlias.count = base.proofs.data[0].cose.count;
        std::memcpy(mem.storage.bytes.data + 100, base.proofs.data[0].cose.data,
                    source.coseAlias.count);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        const auto r = adapter.freeze(program, before, clock, app, mem.storage);
        requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                         r.error == ev::EvidenceError::Argument && emptyFrozen(app, mem),
                     "source COSE cannot alias unused output capacity");
    }
    if (before.ledger.count) {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        app.proofs.data =
            reinterpret_cast<rp::SignedFact *>(const_cast<xr::Receipt *>(before.ledger.data));
        app.proofs.capacity = 1;
        app.proofs.count = 0;
        OriginalSources source(facts);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        const auto r = adapter.freeze(program, before, clock, app, mem.storage);
        requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                         r.error == ev::EvidenceError::Argument,
                     "active receipt ledger cannot alias output descriptor capacity");
    }
    if (before.solved.count) {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        app.proofs.data =
            reinterpret_cast<rp::SignedFact *>(const_cast<xr::Id *>(before.solved.data));
        app.proofs.capacity = 1;
        app.proofs.count = 0;
        OriginalSources source(facts);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        const auto r = adapter.freeze(program, before, clock, app, mem.storage);
        requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                         r.error == ev::EvidenceError::Argument,
                     "active solved span cannot alias output descriptor capacity");
    }
    // Missing/failing current source ports close and the same adapter can retry
    // healthy data. Failure on the second lookup exercises partial-copy clearing.
    for (unsigned port = 0; port < 3; ++port) {
        if (port == 2 && !base.proofs.count)
            continue;
        for (auto mode :
             {sr::SourceRead::Missing, sr::SourceRead::Unavailable, sr::SourceRead::Failure}) {
            EvidenceMemory mem;
            auto app = applicationBase(base, plan);
            mem.bind(app);
            OriginalSources source(facts);
            if (port == 0)
                source.localMode = mode;
            if (port == 1)
                source.inputMode = mode;
            if (port == 2) {
                source.lookupMode = mode;
                source.lookupFailureAt = base.proofs.count > 1 ? 2 : 1;
            }
            SyntheticInstalls installs;
            SyntheticInputs inputs;
            setup(installs);
            sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                           mem.work);
            const auto r = adapter.freeze(program, before, clock, app, mem.storage);
            requireCodec(
                r.scope == (mode == sr::SourceRead::Failure ? ev::EvidenceScope::Rejected
                                                            : ev::EvidenceScope::Unavailable) &&
                    emptyFrozen(app, mem),
                "missing/unavailable/failing original source clears complete frozen output");
            source.localMode = source.inputMode = source.lookupMode = sr::SourceRead::OriginalBytes;
            source.lookupFailureAt = 0;
            requireCodec(adapter.freeze(program, before, clock, app, mem.storage).scope ==
                             ev::EvidenceScope::FrozenOriginal,
                         "source failure healthy reuse");
        }
    }
    // Actual signature verification is run before deliberately returning its error.
    if (base.proofs.count || base.outbox.count)
        for (unsigned mode = 1; mode <= 4; ++mode) {
            EvidenceMemory mem;
            auto app = applicationBase(base, plan);
            mem.bind(app);
            OriginalSources source(facts);
            SyntheticInstalls installs;
            SyntheticInputs inputs;
            setup(installs);
            CryptoFailure crypto(facts.verifier);
            crypto.mode = mode;
            sr::SignedPlanEvidence adapter(facts.hash, crypto, source, installs, inputs, mem.work);
            const auto r = adapter.freeze(program, before, clock, app, mem.storage);
            requireCodec(
                r.scope == ev::EvidenceScope::Rejected && emptyFrozen(app, mem),
                "crypto unavailable/signature/failure/cleanup never publishes partial evidence");
            crypto.mode = 0;
            requireCodec(adapter.freeze(program, before, clock, app, mem.storage).scope ==
                             ev::EvidenceScope::FrozenOriginal,
                         "crypto failure healthy reuse");
        }
    // Historical final metadata and clock errors never consult current sources.
    for (unsigned field = 0; field < 9; ++field) {
        EvidenceMemory mem;
        OriginalSources source(facts);
        source.blocked = true;
        SyntheticInstalls installs;
        installs.denyCurrent = true;
        SyntheticInputs inputs;
        inputs.denyCurrent = true;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        auto final = syntheticFinal(decoded);
        switch (field) {
        case 0:
            final.operationId[0] ^= 1;
            break;
        case 1:
            ++final.issueCount;
            break;
        case 2:
            ++final.expectedLastIssued;
            break;
        case 3:
            ++final.profileRevision;
            break;
        case 4:
            ++final.trustRevision;
            break;
        case 5:
            ++final.firstIssued;
            break;
        case 6:
            ++final.lastIssued;
            break;
        case 7:
            final.kind = larp::devices::ProgressKind::Control;
            break;
        default:
            final.issueCount = 9;
        }
        ev::ClockWitness output;
        output.boot.bytes[0] = 0xff;
        const auto r = adapter.historical(program, before, decoded, final, output);
        requireCodec(
            r.scope == ev::EvidenceScope::Rejected && sameClock(output, ev::ClockWitness{}) &&
                !source.locals && !source.inputs && !source.lookups,
            "historical Final range/context error clears clock and never uses current sources");
        requireCodec(
            adapter.historical(program, before, decoded, syntheticFinal(decoded), output).scope ==
                    ev::EvidenceScope::VerifiedHistorical &&
                sameClock(output, clock),
            "historical metadata failure healthy reuse");
    }
    {
        EvidenceMemory mem;
        auto app = applicationBase(base, plan);
        mem.bind(app);
        OriginalSources source(facts);
        SyntheticInstalls installs;
        SyntheticInputs inputs;
        setup(installs);
        sr::SignedPlanEvidence adapter(facts.hash, facts.verifier, source, installs, inputs,
                                       mem.work);
        source.callback = [&]() {
            const auto count = mem.storage.bytes.count, proofs = app.proofs.count;
            const auto r = adapter.freeze(program, before, clock, app, mem.storage);
            ev::ClockWitness output;
            output.boot.bytes[0] = 0xfe;
            const auto h =
                adapter.historical(program, before, decoded, syntheticFinal(decoded), output);
            requireCodec(r.scope == ev::EvidenceScope::Rejected &&
                             h.scope == ev::EvidenceScope::Rejected &&
                             mem.storage.bytes.count == count && app.proofs.count == proofs &&
                             output.boot.bytes[0] == 0xfe,
                         "callback reentry preserves all outer outputs before any reset");
        };
        requireCodec(adapter.freeze(program, before, clock, app, mem.storage).scope ==
                         ev::EvidenceScope::FrozenOriginal,
                     "outer evidence freeze survives callback reentry");
    }
    requireCodec(modelPlan(base.plan, base.overrideRecord) == planBefore,
                 "all boundary variants leave actual original Plan intact");
}
