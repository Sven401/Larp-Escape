#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include "GameExecution/RulesProgressCodec.h"

namespace larp {
namespace devices {
namespace rules_progress {
namespace rules = execution::rules;
namespace codec = execution::rules_progress;
// Original trusted local clock observation, captured exactly once for this
// semantic input, not a timestamp parsed from a client or invented from Plan.
// Crash/stop time is not automatically added. A new owner boot requires an
// explicit durable lifecycle anchor before new running-clock observations.
struct ClockWitness {
    rules::Identity boot;
    std::uint64_t monotonicBefore = 0, monotonicAfter = 0;
    std::uint64_t activeBefore = 0, activeAfter = 0;
};
enum class EvidenceScope : std::uint8_t {
    Rejected,
    Unavailable,
    FrozenOriginal,
    VerifiedHistorical
};
enum class EvidenceError : std::uint8_t {
    None,
    Argument,
    Capacity,
    Crypto,
    Context,
    OriginKeyUnavailable,
    OriginalInstallUnavailable,
    Source,
    Authority,
    Signature,
    Digest,
    Range,
    Dependency,
    Clock,
    Canonical
};
struct EvidenceResult {
    EvidenceScope scope = EvidenceScope::Rejected;
    EvidenceError error = EvidenceError::None;
};
struct EvidenceStorage {
    // Exclusive bytes for authorization and all proof/outbox COSE spans. Pools
    // for SignedFact descriptors are Application.proofs/outbox. No borrowed
    // incoming transport/source pointers survive freeze/decode callback return.
    rules::Pool<std::uint8_t> bytes;
};
enum class ControlKind : std::uint8_t {
    Start = 1,
    Pause = 2,
    Resume = 3,
    Stop = 4,
    Checkpoint = 5,
    RestoreStopped = 6,
    Activation = 7
};
struct Control {
    rules::Identity operation;
    rules::Digest request, catalog;
    rules::Context context;
    rules::Fence expected, targetFence;
    ControlKind kind = ControlKind::Checkpoint;
    rules::Mode before = rules::Mode::Stopped, after = rules::Mode::Stopped;
    ClockWitness clock;
    rules::Span<std::uint8_t> authorization;
};
// Separate lifecycle/checkpoint acceptance, never an invented local sample or
// a claim of Running inferred from an Application's expected metadata.
class ISignedControlEvidence {
  public:
    virtual ~ISignedControlEvidence() = default;
    virtual EvidenceResult freezeControl(const rules::Program &, const rules::State &, Control &,
                                         EvidenceStorage &) = 0;
    virtual EvidenceResult historicalControl(const rules::Program &, const rules::State &,
                                             const Control &, const ProgressRecord &) = 0;
};
class ISignedPlanEvidence {
  public:
    virtual ~ISignedPlanEvidence() = default;
    // Called on a complete actual pure Planner result. Fills ONLY original
    // proofs/outbox/authorization and their owned storage; never changes Plan,
    // operation, request, catalog, OverrideRecord or assigned candidates.
    // Every output must be actual final writeFact/checkFact bytes with its
    // original public XY and raw payload digest. No unsigned/keyless fallback.
    // Complete proof closure includes triggering Fact even when absent from a
    // sufficient outgoing witness, and retained value/timer/latch causality.
    // Original owner/role acceptance and setup/install/key binding provenance
    // are persisted as versioned authorization, for ALL input kinds.
    virtual EvidenceResult freeze(const rules::Program &, const rules::State &before,
                                  const ClockWitness &, codec::Application &,
                                  EvidenceStorage &) = 0;
    // Verifies original saved bytes/keys/install/role acceptance and actual
    // immutable clock witness. It does NOT authorize today's GM or create a
    // live admission. All referenced original installed material must be owned
    // by the saved evidence; unavailable provenance remains closed.
    // The caller separately replays actual Planner semantics and compares the
    // ENTIRE result before any recovered state/outbox/intent publication.
    virtual EvidenceResult historical(const rules::Program &, const rules::State &before,
                                      const codec::Application &, const ProgressRecord &final,
                                      ClockWitness &originalClock) = 0;
};
} // namespace rules_progress
} // namespace devices
} // namespace larp
