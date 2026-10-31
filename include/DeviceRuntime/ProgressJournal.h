#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace devices {

struct ProgressLimits {
    static constexpr std::size_t SlotBytes = 4096;
    static constexpr std::size_t Slots = 64;
    static constexpr std::size_t Records = Slots - 1;
    static constexpr std::size_t StorageBytes = SlotBytes * Slots;
    static constexpr std::size_t PayloadBytes = 3072;
    static constexpr std::uint8_t FactsPerCommit = 8;
};

// A sole worker owns the store and journal. These calls are synchronous and
// never belong in an ISR, radio callback or game tick. read must fill exactly
// count bytes or fail. program changes only requested erased bytes, never other
// bytes; false may mean a partial or complete write. sync confirms all
// preceding writes durable under the backend's qualified power-loss contract.
// No erase, implicit format, fallback, or partition resize is permitted.
class IProgressStore {
  public:
    virtual ~IProgressStore() = default;
    virtual std::size_t capacity() const = 0;
    virtual bool read(std::size_t offset, std::uint8_t *bytes, std::size_t count) = 0;
    virtual bool program(std::size_t offset, const std::uint8_t *bytes, std::size_t count) = 0;
    virtual bool sync() = 0;
};

struct ProgressOrigin {
    std::array<std::uint8_t, 16> event{};
    std::array<std::uint8_t, 16> box{};
    std::array<std::uint8_t, 16> epoch{};
};

enum class ProgressKind : std::uint8_t {
    Received = 1,
    Application = 2,
    EffectClaim = 3,
    EffectResult = 4,
    Control = 5
};

struct ProgressRequest {
    std::array<std::uint8_t, 16> operationId{};
    std::array<std::uint8_t, 32> requestDigest{};
    ProgressKind kind = ProgressKind::Received;
    std::uint64_t expectedRevision = 0;
    std::uint64_t expectedLastIssued = 0;
    std::uint64_t profileRevision = 0;
    std::uint64_t trustRevision = 0;
    std::uint8_t issueCount = 0;
    const std::uint8_t *payload = nullptr;
    std::size_t payloadBytes = 0;
};

struct ProgressRecord {
    std::array<std::uint8_t, 16> operationId{};
    std::array<std::uint8_t, 32> requestDigest{};
    ProgressKind kind = ProgressKind::Received;
    std::uint64_t revision = 0;
    std::uint64_t expectedRevision = 0;
    std::uint64_t expectedLastIssued = 0;
    std::uint64_t profileRevision = 0;
    std::uint64_t trustRevision = 0;
    // The assigned range is zero/zero when issueCount is zero.
    std::uint64_t firstIssued = 0;
    std::uint64_t lastIssued = 0;
    std::uint16_t payloadBytes = 0;
    std::uint8_t issueCount = 0;
    std::uint8_t index = 0;
};

// The callback receives frozen metadata after validation/copy and immediately
// before programming. It must keep rights/profile/context changes ordered until
// the outcome is resolved; WriteUncertain extends that fence beyond this call
// until genuine backend recovery. It is NOT an RTOS coordinator
// or a D0 verifier. No callback is needed for an unchanged committed replay.
class IProgressCommitFence {
  public:
    virtual ~IProgressCommitFence() = default;
    virtual bool allow(const ProgressRecord &record) = 0;
};

enum class ProgressOutcome : std::uint8_t {
    Initialized,
    Recovered,
    Committed,
    Duplicate,
    Read,
    Empty,
    NeedsRecovery,
    Busy,
    Invalid,
    WrongOrigin,
    Stale,
    Denied,
    Conflict,
    Full,
    CounterExhausted,
    Corrupt,
    Unsupported,
    IoFailure,
    WriteUncertain
};

struct ProgressResult {
    ProgressOutcome outcome = ProgressOutcome::NeedsRecovery;
    std::uint64_t revision = 0;
    std::uint64_t firstIssued = 0;
    std::uint64_t lastIssued = 0;
    std::uint8_t index = 0;
};

struct ProgressSnapshot {
    ProgressOrigin origin{};
    std::uint64_t revision = 0;
    std::uint64_t lastIssued = 0;
    std::uint8_t records = 0;
    bool ready = false;
    bool operating = false;
    bool poisoned = false;
    ProgressOutcome last = ProgressOutcome::NeedsRecovery;
};

// This is a bounded storage primitive, not the D1 semantic reducer. The caller
// supplies a COMPLETE canonical transaction (context, facts/outbox, latches,
// effects and all other deltas) and binds it to requestDigest. D0 validation,
// persistent stream conflict handling, full-plan derivation and reconstruction
// belong to the integration. Matching operation IDs compare every stored byte,
// not only the supplied digest. Different event IDs require semantic replay
// checks outside this class. Predicted outgoing IDs use expectedLastIssued + 1
// through + issueCount; they must not be published before Committed/Duplicate.
//
// 256 KiB, 63 immutable transactions, no reclaim/retirement. Separate one-shot
// header anchors reserve every record before programming it, so an erased final slot
// cannot silently remove a confirmed transaction. Any occupied torn
// or damaged slot closes recovery; there is no rollback to an older prefix.
// CRC detects accidental corruption, not authenticity or full-flash rollback.
// WriteUncertain poisons this instance permanently. A new instance may recover
// only after the backend has genuinely reopened/restarted its durable medium;
// reading a write cache in the same failed session is not a durability proof.
class ProgressJournal {
  public:
    explicit ProgressJournal(IProgressStore &store) : store_(store) {}
    ProgressJournal(const ProgressJournal &) = delete;
    ProgressJournal &operator=(const ProgressJournal &) = delete;
    ProgressResult initialize(const ProgressOrigin &origin);
    ProgressResult recover(const ProgressOrigin &origin);
    ProgressResult commit(const ProgressRequest &request, IProgressCommitFence &fence);
    // index is zero-based. On failure metadata is cleared and no payload bytes
    // are written. The consumer may act only after outcome Read.
    ProgressResult readRecord(std::uint8_t index, ProgressRecord &record, std::uint8_t *payload,
                              std::size_t capacity);
    ProgressSnapshot snapshot() const {
        return state_;
    }

  private:
    IProgressStore &store_;
    ProgressSnapshot state_{};
    // Confirmed watermark survives unsuccessful opens and explicit-create conflicts.
    bool knownState_ = false;
    std::array<std::uint8_t, ProgressLimits::SlotBytes> scratch_{};
    std::array<std::array<std::uint8_t, 16>, ProgressLimits::Records> operationIds_{};
    std::uint32_t tailCrc_ = 0;
    bool reserveRecord(std::uint8_t index);
    ProgressResult finish(ProgressOutcome outcome);
    ProgressResult finishRecord(ProgressOutcome outcome, const ProgressRecord &record);
    ProgressOutcome scan(const ProgressOrigin &origin);
    ProgressOutcome loadRecord(std::uint8_t index, ProgressRecord &record);
    bool writeSlot(std::size_t slot);
};

const char *progressOutcomeName(ProgressOutcome outcome);

} // namespace devices
} // namespace larp
