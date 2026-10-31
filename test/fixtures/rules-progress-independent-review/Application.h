#pragma once
#include "Catalog.h"
#include "Signer.h"
std::string modelId(const xr::FactId &v) {
    return A({Y(v.event.bytes, 16), Y(v.origin.bytes, 16), Y(v.epoch.bytes, 16), N(v.sequence)});
}
std::string modelRef(const xr::FactRef &v) {
    return A({modelId(v.id), Y(v.digest.bytes, 32), Y(v.deployment.bytes, 32),
              Y(v.binding.bytes, 32), Y(v.run.bytes, 16), Y(v.station.bytes, 16),
              Y(v.subject.bytes, 16), N(static_cast<unsigned>(v.type))});
}
std::string modelFact(const xr::Fact &v) {
    return A({N(v.subject), modelRef(v.ref), N(static_cast<unsigned>(v.kind))});
}
std::string modelFence(const xr::Fence &v) {
    return A({N(v.progress), N(v.profile), N(v.trust), N(v.facts), N(v.ledger), N(v.resources),
              N(v.rights), N(v.capacity), N(v.lastIssued), N(v.activeTime)});
}
std::string modelAuthority(const xr::OverrideAuthority &v) {
    return A({Y(v.peer.bytes, 16), N(v.principal), N(v.roleEpoch), N(v.connectionToken),
              N(v.registration), N(v.lifecycle), N(v.connectionHandle), N(v.registryRevision),
              N(v.eventRevision)});
}
std::string modelOverride(const xr::OverrideRecord &v) {
    return A({modelContext(v.context), Y(v.program.bytes, 32), modelFence(v.expected),
              modelAuthority(v.authority), Y(v.ownerBoot.bytes, 16), Y(v.token.bytes, 16),
              Y(v.prepareId.bytes, 16), Y(v.confirmId.bytes, 16), Y(v.operation.bytes, 16),
              Y(v.rule.bytes, 16), Y(v.prepareDigest.bytes, 32), Y(v.confirmDigest.bytes, 32),
              N(v.original), N(v.replacement), N(v.originalGeneration), N(v.replacementGeneration),
              N(v.preparedAt), N(v.expiresAt), N(v.confirmedAt)});
}
std::string modelInput(const xr::Input &v, const xr::OverrideRecord &record) {
    return A({modelContext(v.context), N(static_cast<unsigned>(v.kind)), N(v.bootGeneration),
              N(v.sequence), N(v.port), N(v.timer), N(v.armGeneration),
              N(static_cast<unsigned>(v.type)), B(v.known), I(v.value), modelFact(v.fact),
              modelOverride(record)});
}
std::string modelEvidence(xr::Evidence v, const xr::Plan &p) {
    std::size_t start = 0;
    if (v.refs.count)
        start = (reinterpret_cast<std::uintptr_t>(v.refs.data) -
                 reinterpret_cast<std::uintptr_t>(p.evidence.data)) /
                sizeof(xr::FactRef);
    return A({N(start), N(v.refs.count)});
}
std::string modelPlan(const xr::Plan &p, const xr::OverrideRecord &raw) {
    return A({modelContext(p.context), modelFence(p.expected),
              A({modelContext(p.receipt.context), modelInput(p.receipt.input, raw)}),
              N(p.consumedTimer), P(p.evidence, modelRef),
              P(p.values,
                [&p](const xr::ValueWrite &v) {
                    return A({N(v.target), N(static_cast<unsigned>(v.value.type)), I(v.value.value),
                              modelEvidence(v.value.evidence, p)});
                }),
              P(p.timers,
                [&p](const xr::TimerWrite &v) {
                    return A({N(v.target), B(v.value.active), N(v.value.deadline),
                              N(v.value.generation), modelEvidence(v.value.evidence, p)});
                }),
              P(p.latches,
                [&p](const xr::Latch &v) {
                    return A({Y(v.rule.bytes, 16), N(v.generation), modelEvidence(v.evidence, p)});
                }),
              P(p.emissions,
                [&p](const xr::Emission &v) {
                    return A({N(v.subject), Y(v.rule.bytes, 16), N(v.generation),
                              modelId(v.candidate), modelEvidence(v.dependencies, p),
                              N(static_cast<unsigned>(v.type))});
                }),
              P(p.intents,
                [&p](const xr::Intent &v) {
                    return A({N(v.port), N(v.resource), Y(v.rule.bytes, 16),
                              Y(v.exclusive.bytes, 16), modelEvidence(v.evidence, p)});
                }),
              B(p.admitsFact), modelFact(p.fact)});
}
std::string modelSigned(const rp::SignedFact &v) {
    return A({modelRef(v.ref), Y(v.publicKey, 64), Y(v.cose.data, v.cose.count)});
}
std::string modelApplication(const rp::Application &a) {
    return "{\"0\":\"larp.rules.application.v1\",\"1\":" + Y(a.operation.bytes, 16) +
           ",\"2\":" + Y(a.request.bytes, 32) + ",\"3\":" + Y(a.catalog.bytes, 32) +
           ",\"4\":" + modelPlan(a.plan, a.overrideRecord) + ",\"5\":" + P(a.proofs, modelSigned) +
           ",\"6\":" + P(a.outbox, modelSigned) +
           ",\"7\":" + Y(a.authorization.data, a.authorization.count) + '}';
}
bool sameId(const xr::FactId &a, const xr::FactId &b) {
    return a.sequence == b.sequence && !std::memcmp(a.event.bytes, b.event.bytes, 16) &&
           !std::memcmp(a.origin.bytes, b.origin.bytes, 16) &&
           !std::memcmp(a.epoch.bytes, b.epoch.bytes, 16);
}
struct SignedSet {
    struct Saved {
        xr::FactRef ref;
        d0::FrozenFact fact;
        d0::CandidatePublicKey publicKey;
    };
    std::list<Saved> records;
    SigningOwner signer;
    Hash hash;
    RealCrypto verifier;
    unsigned writes = 0, applications = 0;
    Saved *find(const xr::FactId &id) {
        for (auto &r : records)
            if (sameId(id, r.ref.id))
                return &r;
        return nullptr;
    }
    Saved &freeze(xr::FactRef ref, xr::Evidence deps = {}) {
        if (auto old = find(ref.id)) {
            ref.digest = old->ref.digest;
            requireCodec(modelRef(ref) == modelRef(old->ref),
                         "same actual signed ID retains immutable metadata");
            return *old;
        }
        records.emplace_back();
        auto &saved = records.back();
        saved.ref = ref;
        d0::Fact value;
        std::memcpy(value.event.bytes, ref.id.event.bytes, 16);
        std::memcpy(value.run.bytes, ref.run.bytes, 16);
        std::memcpy(value.origin.bytes, ref.id.origin.bytes, 16);
        std::memcpy(value.station.bytes, ref.station.bytes, 16);
        std::memcpy(value.epoch.bytes, ref.id.epoch.bytes, 16);
        std::memcpy(value.subject.bytes, ref.subject.bytes, 16);
        std::memcpy(value.package.bytes, ref.deployment.bytes, 32);
        std::memcpy(value.binding.bytes, ref.binding.bytes, 32);
        value.generation = 1;
        value.sequence = ref.id.sequence;
        value.type = static_cast<std::uint8_t>(ref.type);
        requireCodec(deps.refs.count <= 8, "real D0 dependency limit");
        value.dependencyCount = static_cast<std::uint8_t>(deps.refs.count);
        for (std::size_t i = 0; i < deps.refs.count; ++i) {
            auto &to = value.dependencies[i];
            const auto &from = deps.refs.data[i].id;
            std::memcpy(to.event.bytes, from.event.bytes, 16);
            std::memcpy(to.origin.bytes, from.origin.bytes, 16);
            std::memcpy(to.epoch.bytes, from.epoch.bytes, 16);
            to.sequence = from.sequence;
        }
        d0::FactWriteWorkspace workspace;
        const auto result =
            d0::writeFact(value, hash, signer.get(), verifier, workspace, saved.fact);
        requireCodec(result.scope == d0::WriteScope::SignedUnpublished &&
                         result.verification.scope == d0::Scope::SignatureCheckedUnauthenticated,
                     "actual D0 writer/verifier COSE");
        saved.publicKey = result.publicKey;
        std::memcpy(saved.ref.digest.bytes, saved.fact.payloadDigest.bytes, 32);
        ++writes;
        return saved;
    }
    rp::SignedFact projection(Saved &saved) {
        rp::SignedFact out;
        out.ref = saved.ref;
        std::memcpy(out.publicKey, saved.publicKey.x, 32);
        std::memcpy(out.publicKey + 32, saved.publicKey.y, 32);
        out.cose.data = saved.fact.original;
        out.cose.count = saved.fact.length;
        return out;
    }
    void input(xr::Input &input) {
        if (input.kind == xr::InputKind::Fact)
            input.fact.ref = freeze(input.fact.ref).ref;
    }
};
struct AppMemory {
    rp::Application app;
    rp::ApplicationStorage storageBytes;
    PlannerMemory plan;
    std::vector<rp::SignedFact> proofs, outbox;
    std::vector<std::uint8_t> bytes;
    AppMemory() : proofs(256), outbox(8), bytes(32768) {
        app.plan = plan.plan;
        storage(app.proofs, proofs, 256);
        storage(app.outbox, outbox, 8);
        storage(storageBytes.bytes, bytes, 32768);
    }
};

bool applicationCleared(const AppMemory &v) {
    const auto &a = v.app;
    return !a.decoded && !a.plan.ready && !a.plan.admitsFact && !a.plan.values.count &&
           !a.plan.timers.count && !a.plan.latches.count && !a.plan.emissions.count &&
           !a.plan.intents.count && !a.plan.evidence.count && !a.proofs.count && !a.outbox.count &&
           !a.authorization.count && !v.storageBytes.bytes.count;
}
void applicationCapacity(const std::vector<std::uint8_t> &raw, std::size_t n,
                         rp::Application &source) {
#define CAP(field)                                                                                 \
    if (source.plan.field.count) {                                                                 \
        AppMemory m;                                                                               \
        const auto full = m.app.plan.field.capacity;                                               \
        m.app.plan.field.capacity = source.plan.field.count - 1;                                   \
        requireCodec(rp::decodeApplication(view(raw, n), m.app, m.storageBytes).scope ==           \
                             rp::Scope::Rejected &&                                                \
                         applicationCleared(m),                                                    \
                     "actual application plan pool capacity -1 clears all");                       \
        m.app.plan.field.capacity = full;                                                          \
        requireCodec(rp::decodeApplication(view(raw, n), m.app, m.storageBytes).scope ==           \
                             rp::Scope::DecodedUnauthenticated &&                                  \
                         !m.app.plan.ready,                                                        \
                     "application pool healthy reuse remains unexecutable");                       \
    }
    CAP(values);
    CAP(timers);
    CAP(latches);
    CAP(emissions);
    CAP(intents);
    CAP(evidence);
#undef CAP
#define CAP(field)                                                                                 \
    if (source.field.count) {                                                                      \
        AppMemory m;                                                                               \
        const auto full = m.app.field.capacity;                                                    \
        m.app.field.capacity = source.field.count - 1;                                             \
        requireCodec(rp::decodeApplication(view(raw, n), m.app, m.storageBytes).scope ==           \
                             rp::Scope::Rejected &&                                                \
                         applicationCleared(m),                                                    \
                     "actual application signed pool capacity -1 clears all");                     \
        m.app.field.capacity = full;                                                               \
        requireCodec(rp::decodeApplication(view(raw, n), m.app, m.storageBytes).scope ==           \
                         rp::Scope::DecodedUnauthenticated,                                        \
                     "signed pool healthy reuse");                                                 \
    }
    CAP(proofs);
    CAP(outbox);
#undef CAP
    AppMemory seed;
    requireCodec(rp::decodeApplication(view(raw, n), seed.app, seed.storageBytes).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "application storage seed");
    const auto ownedBytes = seed.storageBytes.bytes.count;
    if (ownedBytes) {
        AppMemory small;
        small.storageBytes.bytes.capacity = ownedBytes - 1;
        requireCodec(rp::decodeApplication(view(raw, n), small.app, small.storageBytes).scope ==
                             rp::Scope::Rejected &&
                         applicationCleared(small),
                     "actual signed byte storage capacity -1");
        small.storageBytes.bytes.capacity = ownedBytes;
        requireCodec(rp::decodeApplication(view(raw, n), small.app, small.storageBytes).scope ==
                         rp::Scope::DecodedUnauthenticated,
                     "actual signed byte storage exact capacity");
    }
    auto damaged = raw;
    damaged.resize(n);
    damaged.push_back(0);
    requireCodec(rp::decodeApplication(view(damaged, n + 1), seed.app, seed.storageBytes).scope ==
                         rp::Scope::Rejected &&
                     applicationCleared(seed),
                 "application malformed complete reuse clears earlier decoded result");
    requireCodec(rp::decodeApplication(view(raw, n), seed.app, seed.storageBytes).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "application malformed healthy reuse");
    std::vector<std::uint8_t> output(32768);
    Pool<std::uint8_t> out;
    out.data = output.data();
    out.capacity = n - 1;
    out.count = 17;
    requireCodec(rp::encodeApplication(source, out).scope == rp::Scope::Rejected && !out.count,
                 "actual signed application output capacity -1");
    out.capacity = n;
    requireCodec(rp::encodeApplication(source, out).scope == rp::Scope::EncodedUnpublished &&
                     out.count == n,
                 "actual signed application output exact capacity");
    out.capacity = output.size();
    rp::Application fabricated = source;
    fabricated.plan.receipt.input.kind = xr::InputKind::Override;
    fabricated.overrideRecord.context = fabricated.plan.context;
    fabricated.overrideRecord.program = fabricated.catalog;
    fabricated.overrideRecord.expected = fabricated.plan.expected;
    fabricated.overrideRecord.operation = fabricated.operation;
    const std::uint8_t untrustedAuthorization = 1;
    fabricated.authorization.data = &untrustedAuthorization;
    fabricated.authorization.count = 1;
    out.count = 17;
    const auto admission = rp::encodeApplication(fabricated, out);
    requireCodec(admission.scope == rp::Scope::Rejected &&
                     admission.error == rp::Error::Admission && !out.count,
                 "public raw override record can never replace private Owner admission");
    AppMemory alias;
    std::copy(raw.begin(), raw.begin() + n, alias.bytes.begin());
    alias.app.decoded = true;
    alias.app.plan.ready = true;
    alias.storageBytes.bytes.count = 17;
    auto r = rp::decodeApplication(view(alias.bytes, n), alias.app, alias.storageBytes);
    requireCodec(r.scope == rp::Scope::Rejected && r.error == rp::Error::Alias &&
                     alias.app.decoded && alias.app.plan.ready &&
                     alias.storageBytes.bytes.count == 17,
                 "application alias preflight leaves old flags/count untouched");
    if (source.outbox.count) {
        Pool<std::uint8_t> overlap;
        overlap.data = const_cast<std::uint8_t *>(source.outbox.data[0].cose.data);
        overlap.capacity = source.outbox.data[0].cose.count;
        overlap.count = 17;
        r = rp::encodeApplication(source, overlap);
        requireCodec(r.scope == rp::Scope::Rejected && r.error == rp::Error::Alias &&
                         overlap.count == 17,
                     "COSE/output alias rejected before touching source or old count");
    }
    for (std::size_t i = 0; i < source.plan.emissions.count; ++i) {
        auto &e = source.plan.emissions.data[i];
        if (e.dependencies.refs.count) {
            const auto original = e.dependencies;
            e.dependencies.refs.data = source.plan.evidence.data + source.plan.evidence.count;
            e.dependencies.refs.count = 1;
            out.count = 17;
            r = rp::encodeApplication(source, out);
            e.dependencies = original;
            requireCodec(r.scope == rp::Scope::Rejected && r.error == rp::Error::Reference &&
                             !out.count,
                         "nonowned dependency evidence view fails without partial output");
            requireCodec(rp::encodeApplication(source, out).scope == rp::Scope::EncodedUnpublished,
                         "malformed typed view healthy reuse");
            break;
        }
    }
}
void captureApplication(const xr::Program &program, const xr::Plan &plan, SignedSet &facts) {
    rp::Application app;
    app.plan = plan;
    app.operation.bytes[0] = 0x71;
    app.operation.bytes[14] = static_cast<std::uint8_t>(facts.applications >> 8);
    app.operation.bytes[15] = static_cast<std::uint8_t>(++facts.applications);
    app.request.bytes[0] = 0x72;
    std::vector<std::uint8_t> catalog(32768);
    Pool<std::uint8_t> cp;
    cp.data = catalog.data();
    cp.capacity = catalog.size();
    requireCodec(rp::encodeCatalog(program, cp).scope == rp::Scope::EncodedUnpublished,
                 "actual trace program catalog");
    std::size_t hashBytes = 0;
    requireCodec(facts.hash.begin() && facts.hash.update(catalog.data(), cp.count) &&
                     facts.hash.finish(app.catalog.bytes, hashBytes) && hashBytes == 32,
                 "actual catalog SHA binding");
    std::vector<rp::SignedFact> proofs, outbox;
    auto prove = [&](const xr::FactRef &ref) {
        for (const auto &r : proofs)
            if (sameId(r.ref.id, ref.id))
                return;
        auto saved = facts.find(ref.id);
        requireCodec(saved && modelRef(saved->ref) == modelRef(ref),
                     "actual immutable signed proof available");
        proofs.push_back(facts.projection(*saved));
    };
    for (std::size_t i = 0; i < plan.evidence.count; ++i)
        prove(plan.evidence.data[i]);
    if (plan.receipt.input.kind == xr::InputKind::Fact)
        prove(plan.receipt.input.fact.ref);
    for (std::size_t i = 0; i < plan.emissions.count; ++i) {
        const auto &e = plan.emissions.data[i];
        xr::FactRef ref;
        ref.id = e.candidate;
        ref.deployment = plan.context.deployment;
        ref.binding = plan.context.binding;
        ref.run = plan.context.run;
        ref.station = plan.context.station;
        ref.subject = program.subjects.data[e.subject - 1].binding.subject;
        ref.type = e.type;
        auto &saved = facts.freeze(ref, e.dependencies);
        outbox.push_back(facts.projection(saved));
    }
    app.proofs.data = proofs.data();
    app.proofs.count = app.proofs.capacity = proofs.size();
    app.outbox.data = outbox.data();
    app.outbox.count = app.outbox.capacity = outbox.size();
    const auto typed = modelApplication(app);
    std::vector<std::uint8_t> bytes(32768);
    Pool<std::uint8_t> output;
    output.data = bytes.data();
    output.capacity = bytes.size();
    requireCodec(rp::encodeApplication(app, output).scope == rp::Scope::EncodedUnpublished,
                 "actual planner/signed application encode");
    const auto frozen = bytes;
    const auto n = output.count;
    applicationCapacity(frozen, n, app);
    AppMemory decoded;
    requireCodec(rp::decodeApplication(view(bytes, n), decoded.app, decoded.storageBytes).scope ==
                         rp::Scope::DecodedUnauthenticated &&
                     decoded.app.decoded && !decoded.app.plan.ready,
                 "actual planner/signed application decode remains unexecutable");
    std::fill(bytes.begin(), bytes.end(), 0xa5);
    requireCodec(modelApplication(decoded.app) == typed,
                 "application evidence and original COSE owned after input destruction");
    requireCodec(
        !std::memcmp(decoded.app.plan.receipt.input.overrideAdmission.record().program.bytes,
                     xr::Digest{}.bytes, 32),
        "decode creates no override admission");
    std::cout << "{\"codecApplication\":\"" << byteHex(frozen.data(), n) << "\",\"model\":" << typed
              << ",\"catalog\":\"" << byteHex(catalog.data(), cp.count)
              << "\",\"catalogModel\":" << catalogModel(program) << ",\"writes\":" << facts.writes
              << "}\n";
}

int decodeApplicationOnly() {
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto encoded = unhex(line);
        std::vector<std::uint8_t> bytes(encoded.begin(), encoded.end());
        AppMemory inactive;
        inactive.app.decoded = true;
        inactive.app.plan.ready = true;
        const auto r =
            rp::decodeApplication(view(bytes, bytes.size()), inactive.app, inactive.storageBytes);
        std::fill(bytes.begin(), bytes.end(), 0xa5);
        std::cout << "{\"scope\":" << unsigned(r.scope) << ",\"error\":" << unsigned(r.error)
                  << ",\"decoded\":" << B(inactive.app.decoded)
                  << ",\"ready\":" << B(inactive.app.plan.ready) << ",\"admissionEmpty\":"
                  << B(xr::sameOverrideRecord(
                         inactive.app.plan.receipt.input.overrideAdmission.record(),
                         xr::OverrideRecord{}))
                  << ",\"emissions\":" << inactive.app.plan.emissions.count
                  << ",\"evidence\":" << inactive.app.plan.evidence.count
                  << ",\"proofs\":" << inactive.app.proofs.count
                  << ",\"outbox\":" << inactive.app.outbox.count
                  << ",\"bytes\":" << inactive.storageBytes.bytes.count << ",\"model\":"
                  << (r.scope == rp::Scope::DecodedUnauthenticated ? modelApplication(inactive.app)
                                                                   : "null")
                  << "}\n";
    }
    return 0;
}
