#pragma once
#include "GameExecution/RulesPlanner.h"

namespace larp {
namespace execution {
namespace rules_progress {
using rules::Pool;
using rules::Span;

// This semantic profile is independent of the package/source metadata budget.
constexpr std::size_t MaximumBytes = 32768;
constexpr std::size_t MaximumFactBytes = 2048;
constexpr std::size_t MaximumOutgoing = 8;

// Original immutable COSE bytes and the original public XY, never a lookup of
// today's key by numeric handle. Their mathematics/authority are checked by the
// actual integration consumer, not by this canonical storage codec.
struct SignedFact {
    rules::FactRef ref;
    std::uint8_t publicKey[64] = {};
    Span<std::uint8_t> cose;
};

struct Application {
    rules::Identity operation;
    rules::Digest request, catalog;
    rules::Plan plan;
    // Encoder requires equality with the actual copied admission on Override.
    // Decoder preserves the original record here but CANNOT create admission.
    // The actual owner restores historical receipts after its historical proof
    // checks; this record must never become a new live command/grant.
    rules::OverrideRecord overrideRecord;
    Pool<SignedFact> proofs, outbox;
    // Owner-specific versioned original authorization evidence. It is opaque
    // here, mandatory for Override, and must be checked by the actual owner.
    Span<std::uint8_t> authorization;
    bool decoded = false;
};

struct ApplicationStorage {
    // Plan pool buffers are supplied in Application.plan. All Evidence views
    // refer to that plan's owned evidence pool. SignedFact/authorization bytes
    // are copied into this exclusive pool, never borrowed from encoded input.
    Pool<std::uint8_t> bytes;
};

enum class Error : std::uint8_t {
    None,
    InvalidArgument,
    Alias,
    Capacity,
    Profile,
    Canonical,
    Range,
    Reference,
    InvalidProgram,
    InvalidPlan,
    Admission
};
enum class Scope : std::uint8_t { Rejected, EncodedUnpublished, DecodedUnauthenticated };
struct Result {
    Scope scope = Scope::Rejected;
    Error error = Error::None;
    std::size_t offset = 0;
};

// Synchronous pure codec: no callbacks, allocation, signing, storage, rights,
// activation or publication. All input/output/pools must be disjoint, including
// complete destination capacities. Aliased/invalid storage is rejected before
// touching any possibly shared object. After valid storage preflight, failures
// leave encoded count zero; decoded
// Program.ready/Application.decoded/Plan.ready remain false and all pool counts
// are cleared. Existing active state must never be used as destination.
// Always inspect Result.scope; unchanged flags/counts after a preflight rejection
// are not a new successful result.
Result encodeCatalog(const rules::Program &, Pool<std::uint8_t> &destination);
Result decodeCatalog(Span<std::uint8_t> source, rules::Program &inactive);
Result encodeApplication(const Application &, Pool<std::uint8_t> &destination);
Result decodeApplication(Span<std::uint8_t> source, Application &inactive,
                         ApplicationStorage &storage);

// Catalog decode runs the shared complete representation/semantic validator.
// Application decode checks the complete canonical representation only. Its
// Plan.ready remains false: the actual consumer must verify original COSE/key/
// digests, incoming/outgoing FactIDs/types/dependencies, historical authority,
// catalog/context, full derived deltas and the one Final-assigned range before
// any semantic recovery publication. DecodedUnauthenticated is no such proof.
} // namespace rules_progress
} // namespace execution
} // namespace larp
