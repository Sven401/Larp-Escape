#pragma once
#include "Companion/D0FactWriter.h"
#include "DeviceRuntime/RulesProgressEvidence.h"

namespace larp {
namespace integration {
namespace signed_rules {
namespace rules = execution::rules;
namespace codec = execution::rules_progress;
namespace progress = devices::rules_progress;
namespace d0 = companion::d0;
constexpr std::size_t MaximumProofs = 256;

enum class SourceRead : std::uint8_t { OriginalBytes, Missing, Unavailable, Failure };
enum class Use : std::uint8_t { CurrentPreparation, OriginalHistorical };
struct FactView {
    codec::SignedFact fact;
    rules::Span<std::uint8_t> installation;
};
struct LocalOriginView {
    // Borrowed from the actual qualified local origin-key owner, never a test
    // scalar, device display identity, setup key or later lookup of a PSA handle.
    d0::IFactSigner *signer = nullptr;
    d0::CandidatePublicKey publicKey;
    rules::Span<std::uint8_t> installation;
};
class IOriginalSources {
  public:
    virtual ~IOriginalSources() = default;
    // Raw original views ONLY: not an authority or signature assertion. Copy
    // each returned view immediately; no transport pointers survive freeze.
    // lookup includes dependency-closure IDs absent from a sufficient witness.
    virtual SourceRead lookup(const rules::FactId &, FactView &) {
        return SourceRead::Unavailable;
    }
    virtual SourceRead local(const rules::Program &, LocalOriginView &) {
        return SourceRead::Unavailable;
    }
    virtual SourceRead input(const rules::Program &, const rules::State &,
                             const progress::ClockWitness &, const codec::Application &,
                             rules::Span<std::uint8_t> &) {
        return SourceRead::Unavailable;
    }
};
class IOriginalInstallations {
  public:
    virtual ~IOriginalInstallations() = default;
    // Concrete authenticated original deployment/key/event installation owner.
    // Must check actual immutable evidence; raw hashes/SubjectBindings/public
    // XY/device-GM alone are not permission. Historical never substitutes today's
    // registry, roles or key for original saved material.
    // No positive implementation is supplied by this header. Missing actual
    // origin-key/install provenance must remain OriginalInstallUnavailable.
    virtual progress::EvidenceResult program(const rules::Program &, rules::Span<std::uint8_t>,
                                             Use);
    virtual progress::EvidenceResult fact(const rules::Program &, const d0::FrozenFact &,
                                          const codec::SignedFact &, rules::Span<std::uint8_t>,
                                          Use);
};
class IOriginalInputs {
  public:
    virtual ~IOriginalInputs() = default;
    // Actual original accepted hardware/timer/fact/control ingress. For Override
    // this includes the unchanged accepted PeerProof transcript/signature and
    // credential, original registry/event installation and owner acceptance.
    // A connection DTO or ECDSA signature alone does not certify GM/time/roles.
    virtual progress::EvidenceResult current(const rules::Program &, const rules::State &,
                                             const progress::ClockWitness &,
                                             const codec::Application &, rules::Span<std::uint8_t>);
    virtual progress::EvidenceResult historical(const rules::Program &, const rules::State &,
                                                const progress::ClockWitness &,
                                                const codec::Application &,
                                                rules::Span<std::uint8_t>);
};
struct OriginRecord {
    rules::FactRef ref;
    rules::Span<std::uint8_t> installation;
    rules::Id dependencies[8] = {};
    std::uint8_t dependencyCount = 0, color = 0;
    bool expected = false, fetched = false;
};
struct Workspace {
    // All caller-owned and disjoint. annotations copies original installation /
    // ingress bytes before callbacks. catalog is freshly encoded and hashed,
    // never a caller-claimed catalog digest. Explicit profile exhaustion closes.
    rules::Pool<std::uint8_t> annotations, catalog;
    rules::Pool<OriginRecord> origins;
    d0::FrozenFact checked;
    d0::FactWriteWorkspace writer;
};
// Actual writeFact/checkFact mathematical adapter plus complete immutable
// original evidence packing. It never manufactures missing origin/install/input
// authority; their concrete owners remain mandatory. No private-key ownership,
// journal IO, activation, admission construction, publication or intent execution.
// Exclusive synchronous storage/crypto worker; not ISR/callback/short-lock work.
// Ports must not mutate/retain any argument or workspace. Original source views
// must be disjoint from every destination/workspace capacity. Recovery never
// consults IOriginalSources; it uses only the saved complete original evidence.
// Reentry/invalid storage is rejected before touching possibly shared objects.
// After valid storage preflight, failed freeze clears its output counts/views;
// historical clears its ClockWitness output. Always inspect EvidenceResult.scope.
// State spans expose their live extent only: the enclosing coordinator must
// additionally keep the complete active/staging storage-bank capacities disjoint.
class SignedPlanEvidence final : public progress::ISignedPlanEvidence {
  public:
    SignedPlanEvidence(configuration::IPackageDigest &hash, d0::IFactCrypto &crypto,
                       IOriginalSources &sources, IOriginalInstallations &installations,
                       IOriginalInputs &inputs, Workspace &workspace)
        : hash_(hash), crypto_(crypto), sources_(sources), installations_(installations),
          inputs_(inputs), work_(workspace) {}
    SignedPlanEvidence(const SignedPlanEvidence &) = delete;
    SignedPlanEvidence &operator=(const SignedPlanEvidence &) = delete;
    progress::EvidenceResult freeze(const rules::Program &, const rules::State &,
                                    const progress::ClockWitness &, codec::Application &,
                                    progress::EvidenceStorage &) override;
    progress::EvidenceResult historical(const rules::Program &, const rules::State &,
                                        const codec::Application &, const devices::ProgressRecord &,
                                        progress::ClockWitness &) override;

  private:
    configuration::IPackageDigest &hash_;
    d0::IFactCrypto &crypto_;
    IOriginalSources &sources_;
    IOriginalInstallations &installations_;
    IOriginalInputs &inputs_;
    Workspace &work_;
    bool operating_ = false;
};
} // namespace signed_rules
} // namespace integration
} // namespace larp
