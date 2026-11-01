#pragma once
#include "CompanionIntegration/RulesSignedEvidence.h"
#include <functional>
#include <list>
namespace sr = larp::integration::signed_rules;
namespace ev = larp::devices::rules_progress;
namespace codec = larp::execution::rules_progress;
// All acceptance ports below are bounded synthetic test policies, NOT original
// Event/GM/install/clock ownership. The cryptographic adapter remains actual.
namespace owner_review {
static const std::uint8_t programMarker[] = {0xa1, 0, 1};
static const std::uint8_t factMarker[] = {0xa1, 0, 2};
static const std::uint8_t inputMarker[] = {0xa1, 0, 3};
static const std::uint8_t controlMarker[] = {0xa1, 0, 4};
xr::Span<std::uint8_t> view(const std::uint8_t *p, std::size_t n = 3) {
    xr::Span<std::uint8_t> s;
    s.data = p;
    s.count = n;
    return s;
}
bool marker(xr::Span<std::uint8_t> s, const std::uint8_t *p) {
    return s.count == 3 && s.data && !std::memcmp(s.data, p, 3);
}
ev::EvidenceResult accepted(bool allow, bool historical = false) {
    ev::EvidenceResult out;
    out.scope = allow ? (historical ? ev::EvidenceScope::VerifiedHistorical
                                    : ev::EvidenceScope::FrozenOriginal)
                      : ev::EvidenceScope::Unavailable;
    out.error = allow ? ev::EvidenceError::None : ev::EvidenceError::Authority;
    return out;
}
struct Sources final : sr::IOriginalSources {
    SigningOwner &signing;
    std::function<void()> callback;
    bool available = false;
    unsigned locals = 0, inputs = 0, lookups = 0;
    struct Packet {
        codec::SignedFact fact;
        std::vector<std::uint8_t> bytes;
    };
    std::list<Packet> saved;
    explicit Sources(SigningOwner &s) : signing(s) {}
    sr::SourceRead local(const xr::Program &, sr::LocalOriginView &out) override {
        ++locals;
        if (!available)
            return sr::SourceRead::Missing;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        out.signer = &signing.get();
        requireCodec(out.signer->publicKey(out.publicKey) == d0::SignStatus::Success,
                     "actual synthetic test public key");
        out.installation = view(programMarker);
        return sr::SourceRead::OriginalBytes;
    }
    sr::SourceRead input(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                         const codec::Application &, xr::Span<std::uint8_t> &out) override {
        ++inputs;
        if (!available)
            return sr::SourceRead::Missing;
        out = view(inputMarker);
        return sr::SourceRead::OriginalBytes;
    }
    sr::SourceRead lookup(const xr::FactId &id, sr::FactView &out) override {
        ++lookups;
        if (!available)
            return sr::SourceRead::Missing;
        for (const auto &v : saved)
            if (!std::memcmp(id.event.bytes, v.fact.ref.id.event.bytes, 16) &&
                !std::memcmp(id.origin.bytes, v.fact.ref.id.origin.bytes, 16) &&
                !std::memcmp(id.epoch.bytes, v.fact.ref.id.epoch.bytes, 16) &&
                id.sequence == v.fact.ref.id.sequence) {
                out.fact = v.fact;
                out.fact.cose = view(v.bytes.data(), v.bytes.size());
                out.installation = view(factMarker);
                return sr::SourceRead::OriginalBytes;
            }
        return sr::SourceRead::Missing;
    }
};
struct InstallPolicy final : sr::IOriginalInstallations {
    bool enabled = false;
    unsigned currentCalls = 0, historicalCalls = 0;
    ev::EvidenceResult program(const xr::Program &, xr::Span<std::uint8_t> original,
                               sr::Use use) override {
        const bool historical = use == sr::Use::OriginalHistorical;
        (historical ? historicalCalls : currentCalls)++;
        return accepted(enabled && marker(original, programMarker), historical);
    }
    ev::EvidenceResult fact(const xr::Program &, const d0::FrozenFact &, const codec::SignedFact &,
                            xr::Span<std::uint8_t> original, sr::Use use) override {
        const bool historical = use == sr::Use::OriginalHistorical;
        (historical ? historicalCalls : currentCalls)++;
        return accepted(enabled &&
                            (marker(original, programMarker) || marker(original, factMarker)),
                        historical);
    }
};
struct InputPolicy final : sr::IOriginalInputs {
    bool enabled = false;
    unsigned currents = 0, histories = 0;
    ev::EvidenceResult current(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                               const codec::Application &,
                               xr::Span<std::uint8_t> original) override {
        ++currents;
        return accepted(enabled && marker(original, inputMarker));
    }
    ev::EvidenceResult historical(const xr::Program &, const xr::State &, const ev::ClockWitness &,
                                  const codec::Application &,
                                  xr::Span<std::uint8_t> original) override {
        ++histories;
        return accepted(enabled && marker(original, inputMarker), true);
    }
};
struct Controls final : ev::ISignedControlEvidence, ev::IControlFinalFence {
    bool evidence = false, eligibility = false;
    unsigned freezes = 0, histories = 0, preflights = 0, finals = 0;
    std::function<void()> callback;
    ev::EvidenceResult freezeControl(const xr::Program &, const xr::State &, ev::Control &c,
                                     ev::EvidenceStorage &storage) override {
        ++freezes;
        if (!evidence)
            return accepted(false);
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        if (storage.bytes.capacity < 3)
            return accepted(false);
        std::memcpy(storage.bytes.data, controlMarker, 3);
        storage.bytes.count = 3;
        c.authorization = view(storage.bytes.data);
        if (c.kind == ev::ControlKind::Activation) {
            c.targetFence.profile = c.targetFence.trust = c.targetFence.facts =
                c.targetFence.ledger = c.targetFence.resources = c.targetFence.rights =
                    c.targetFence.capacity = 1;
        }
        return accepted(true);
    }
    ev::EvidenceResult historicalControl(const xr::Program &, const xr::State &,
                                         const ev::Control &c,
                                         const larp::devices::ProgressRecord &) override {
        ++histories;
        return accepted(evidence && marker(c.authorization, controlMarker), true);
    }
    ev::ControlEligibility preflight(const xr::Program &, const xr::State &,
                                     const ev::Control &) override {
        ++preflights;
        return eligibility ? ev::ControlEligibility::Current : ev::ControlEligibility::Unavailable;
    }
    ev::ControlEligibility check(const xr::Program &, const xr::State &, const ev::Control &,
                                 const larp::devices::ProgressRecord &) override {
        ++finals;
        return eligibility ? ev::ControlEligibility::Current : ev::ControlEligibility::Unavailable;
    }
};
struct Clock final : ev::IClock, xr::IManualClock {
    xr::Identity boot;
    std::uint64_t ticks = 1000;
    bool available = true;
    bool read(ev::ClockObservation &out) override {
        out.boot = boot;
        out.monotonic = ticks;
        return available;
    }
    bool now(std::uint64_t &out) override {
        out = ticks;
        return available;
    }
};
struct Ids final : larp::devices::rules_bundle::IIdentitySource, xr::IManualIdentitySource {
    std::uint64_t next = 10000;
    bool make(xr::Identity &out) override {
        out = {};
        const auto n = ++next;
        for (unsigned i = 0; i < 8; ++i)
            out.bytes[15 - i] = static_cast<std::uint8_t>(n >> (8 * i));
        return true;
    }
    bool make(larp::devices::rules_bundle::Identity &out) override {
        xr::Identity id;
        make(id);
        std::memcpy(out.data(), id.bytes, 16);
        return true;
    }
};
struct Authority final : xr::IManualAuthority {
    bool enabled = false;
    xr::OverrideAuthority actor;
    xr::ManualAuthorityResult check(const xr::Context &, const xr::OverrideAuthority &a) override {
        return enabled && !std::memcmp(a.peer.bytes, actor.peer.bytes, 16) &&
                       a.principal == actor.principal && a.roleEpoch == actor.roleEpoch &&
                       a.connectionToken == actor.connectionToken &&
                       a.registration == actor.registration && a.lifecycle == actor.lifecycle &&
                       a.connectionHandle == actor.connectionHandle &&
                       a.registryRevision == actor.registryRevision &&
                       a.eventRevision == actor.eventRevision
                   ? xr::ManualAuthorityResult::Current
                   : xr::ManualAuthorityResult::Closed;
    }
};
struct IntentPolicy final : ev::IIntentEligibility {
    bool enabled = false;
    unsigned calls = 0;
    ev::IntentEligibility check(const xr::Program &, const xr::Program &, const xr::State &,
                                const ev::Transaction &, const codec::Application &,
                                const ev::IntentReservation &, const xr::Intent &,
                                const ev::Effect &,
                                const larp::devices::ProgressRecord &) override {
        ++calls;
        return enabled ? ev::IntentEligibility::Current : ev::IntentEligibility::Unavailable;
    }
};
struct Handler final : ev::IIntentHandler {
    unsigned calls = 0;
    ev::HandlerResult result = ev::HandlerResult::Returned;
    std::function<void()> after;
    ev::HandlerResult deliver(const xr::Context &, const xr::Identity &, std::uint16_t,
                              const xr::Intent &) override {
        ++calls;
        if (after) {
            auto fn = after;
            after = {};
            fn();
        }
        return result;
    }
};
struct Metadata final : ev::ICommitMetadata {
    const xr::Program &program;
    const ev::Owner *owner = nullptr;
    unsigned calls = 0;
    explicit Metadata(const xr::Program &p) : program(p) {}
    ev::MetadataScope read(ev::CommitMetadata &out) override {
        ++calls;
        if (!owner)
            return ev::MetadataScope::Unavailable;
        out.context = program.context;
        out.fence = owner->state().fence;
        return ev::MetadataScope::Current;
    }
};
} // namespace owner_review
