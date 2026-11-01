#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include "GameConfiguration/PackageModules.h"

namespace larp {
namespace devices {
namespace rules_bundle {
using configuration::module_ir::Pool;
using Id = std::uint16_t;
using Identity = std::array<std::uint8_t, 16>;
using Digest = std::array<std::uint8_t, 32>;
constexpr std::size_t ChunkBytes = 2048;
constexpr std::size_t MaximumBlobBytes = 32768;
constexpr std::size_t MaximumChunks = 16;
enum class Kind : std::uint8_t { Catalog = 1, Application = 2 };
enum class Phase : std::uint8_t { Chunk, Final, Auxiliary };
enum class Error : std::uint8_t {
    None,
    InvalidArgument,
    Profile,
    Capacity,
    Crypto,
    Conflict,
    Canonical,
    Context,
    MissingChunk,
    MissingCatalog,
    DigestMismatch,
    Journal,
    Fence,
    RecoveryRequired,
    Busy,
    Consumer
};
enum class Outcome : std::uint8_t {
    Rejected,
    Deferred,
    Prepared,
    Progress,
    Committed,
    Duplicate,
    Unknown,
    Recovered,
    Abandoned
};
struct Result {
    Outcome outcome = Outcome::Rejected;
    Error error = Error::None;
    ProgressResult journal;
};
// Semantic request identity is distinct from each physical record identity.
// The blob digest is independently recomputed from the actual staged bytes.
struct Key {
    Kind kind = Kind::Catalog;
    Identity operation{};
    Digest request{}, deployment{}, catalog{}, blob{};
    std::uint32_t bytes = 0;
    std::uint8_t chunks = 0;
};
struct RecordLink {
    Id bundle = 0;
    Phase phase = Phase::Chunk;
    std::uint8_t chunk = 0;
    ProgressRecord record;
    Digest actualDigest{};
};
struct Descriptor {
    Key key;
    std::uint8_t finalIndex = 0;
    bool final = false;
};
struct PhysicalRequest {
    Identity operation{};
    Digest digest{};
    std::uint64_t expectedRevision = 0, expectedLastIssued = 0;
    std::uint64_t profileRevision = 0, trustRevision = 0;
    std::uint8_t index = 0;
    bool frozen = false, committed = false;
};
struct HeaderMeta {
    std::uint64_t profileRevision = 0, trustRevision = 0;
};
struct Workspace {
    // Exactly owned by the enclosing serialized storage worker, not a borrowed
    // package/source projection. Staged bytes stay immutable until terminal.
    Pool<std::uint8_t> staged;
    Pool<std::uint8_t> recovered;
    Pool<Descriptor> bundles;
    Pool<RecordLink> records;
    std::array<std::uint8_t, ProgressLimits::PayloadBytes> recordBytes{};
    PhysicalRequest chunks[MaximumChunks];
    PhysicalRequest final;
};
class IIdentitySource {
  public:
    virtual ~IIdentitySource() = default;
    // Qualified cryptographic identity source. No counter/hash/clock fallback;
    // one failed/colliding identity rejects this preparation, no retry loop.
    virtual bool make(Identity &destination) = 0;
};
class ICommitFence {
  public:
    virtual ~ICommitFence() = default;
    // Final must recheck the ORIGINAL command/context/authority/deadline and
    // logical fences. Its own Control writes are not a rebase. Keep metadata
    // changes ordered until the outcome is resolved, including Unknown.
    virtual bool allow(const Key &, Phase, const ProgressRecord &) = 0;
};
class IRecoveryConsumer {
  public:
    virtual ~IRecoveryConsumer() = default;
    // Every callback is provisional. Byte views expire at callback return.
    // Historical mathematical checks use original saved identities/keys, never
    // today's GM role as a substitute. Only final commit publishes ALL state.
    virtual bool begin(const ProgressSnapshot &) = 0;
    virtual bool catalog(const Key &, const std::uint8_t *bytes, std::size_t count) = 0;
    virtual bool application(const Key &, const ProgressRecord &final, const std::uint8_t *bytes,
                             std::size_t count) = 0;
    // Nonbundle Received/EffectClaim/EffectResult records require their own
    // complete typed reducer. Unknown records must reject, never be skipped.
    virtual bool auxiliary(const ProgressRecord &, const std::uint8_t *, std::size_t) = 0;
    virtual bool prepare() = 0;
    virtual void commit() = 0;
    virtual void abort() = 0;
};
// Exclusive worker around the unchanged bounded D1 journal. It does not erase,
// activate programs, admit facts, sign, publish, execute intents or infer rights.
// Synchronous IO/crypto may block; never a callback/ISR/game-tick operation.
class Journal {
  public:
    Journal(ProgressJournal &journal, configuration::IPackageDigest &hash, IIdentitySource &ids,
            Workspace &workspace)
        : journal_(journal), hash_(hash), ids_(ids), work_(workspace) {}
    Journal(const Journal &) = delete;
    Journal &operator=(const Journal &) = delete;
    // Call after successful explicit initialize, or genuine backend reopen plus
    // successful low-level recover.
    // A poisoned low-level instance is never reset/revived here. All physical
    // records are validated before semantic callbacks or any publication.
    Result recover(IRecoveryConsumer &consumer);
    // Entire staged blob, all physical slots and future required intent records
    // are checked before the first write. No fact IDs are allocated by begin.
    Result begin(const Key &, const HeaderMeta &, std::uint8_t issueCount,
                 std::size_t reservedIntentRecords);
    // At most one complete physical transaction per step. Committed/Duplicate
    // are returned only for the unique Final marker. Chunks return Progress.
    Result step(ICommitFence &);
    // Abandons ONLY a prepared, physically unwritten descriptor after caller
    // detects a phase-ID collision. Never cancels chunks/Unknown or erases data.
    Result cancelPrepared();
    // One frozen non-Application record: actual Received/EffectClaim/Result,
    // or strictly recognized larp.rules.control.v1. No bundle fragments here.
    // Global outstanding future-intent slots AFTER this record MUST be supplied
    // by the sole owner; consuming one reserved claim/result releases one slot.
    // Same request identity retains original physical expectedRevision/bytes.
    // This new wrapper language binds ALL auxiliary requestDigest fields to
    // SHA256(actual raw payload), also during recovery. ProgressJournal itself
    // remains opaque; unrelated older Received protocols are not adopted here.
    Result auxiliary(const ProgressRequest &, IProgressCommitFence &, std::size_t reservedRecords);
    bool unresolved() const {
        return unresolved_;
    }
    bool active() const {
        return active_;
    }

  private:
    ProgressJournal &journal_;
    configuration::IPackageDigest &hash_;
    IIdentitySource &ids_;
    Workspace &work_;
    Key key_;
    HeaderMeta meta_;
    Id bundle_ = 0;
    std::uint8_t next_ = 0, issueCount_ = 0;
    std::size_t reservedIntentRecords_ = 0;
    bool indexed_ = false, active_ = false, unresolved_ = false;
    bool operating_ = false;
    std::uint64_t baseLastIssued_ = 0;
    std::uint64_t baseRevision_ = 0;
    std::uint8_t baseRecords_ = 0;
    std::uint64_t indexedRevision_ = 0;
    Result result_;
    bool hash(const std::uint8_t *, std::size_t, Digest &);
    bool stored(const Descriptor &, const std::uint8_t *marker, std::size_t markerBytes);
    bool workspace() const;
    Result fail(Error, bool deferred = false);
};
} // namespace rules_bundle
} // namespace devices
} // namespace larp
