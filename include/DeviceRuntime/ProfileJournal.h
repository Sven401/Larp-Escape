#pragma once
#include "DeviceRuntime/ProfileController.h"
namespace larp {
namespace devices {
enum class JournalOpen : std::uint8_t { Present, Missing, Failure };
enum class JournalCommit : std::uint8_t { Confirmed, Uncertain };
// Sole synchronous caller, one open stream at a time. Reads are bounded and End
// is final. beginWrite may truncate ONLY the requested slot. A failed write/open
// may have changed that slot. finishWrite closes and confirms durable bytes only
// after its backend's synchronization; any ambiguous result is Uncertain.
// Backend must keep the other slot intact, with no automatic format/erase.
class IJournalStore : public configuration::IProfileReader, public configuration::IProfileWriter {
  public:
    virtual JournalOpen openRead(std::uint8_t slot) = 0;
    virtual bool closeRead() = 0;
    virtual bool beginWrite(std::uint8_t slot) = 0;
    virtual JournalCommit finishWrite() = 0;
    virtual void abortWrite() = 0;
};
enum class JournalOutcome : std::uint8_t {
    Restored,
    Saved,
    Empty,
    Busy,
    ControllerRejected,
    NeedsRestore,
    Corrupt,
    Unsupported,
    Ambiguous,
    IoFailure,
    WriteUncertain,
    CounterExhausted
};
struct JournalResult {
    JournalOutcome outcome = JournalOutcome::Empty;
    DraftResult controller;
    explicit operator bool() const {
        return outcome == JournalOutcome::Restored || outcome == JournalOutcome::Saved;
    }
};
struct JournalSnapshot {
    std::uint32_t generation = 0, maximumGeneration = 0;
    std::uint8_t slot = 255;
    bool initialized = false, writable = false, operating = false;
    JournalOutcome last = JournalOutcome::NeedsRestore;
};
// Two durable records, independent of two RAM Profiles. No full Profile/document
// buffer or allocation. Restore imports into the private draft; it NEVER starts
// hardware/game. Save serializes only an already Applied, Ready, stopped active
// profile. Caller coordinates activation/save/start in one application context;
// no concurrent/reentrant access to this journal, controller or backend.
class ProfileJournal {
  public:
    ProfileJournal(ProfileController &controller, IJournalStore &store)
        : controller_(controller), store_(store) {}
    ProfileJournal(const ProfileJournal &) = delete;
    ProfileJournal &operator=(const ProfileJournal &) = delete;
    JournalResult restoreDraft(std::uint32_t expectedDraftRevision, bool older = false);
    JournalResult saveActive(std::uint32_t expectedConfigurationRevision);
    JournalSnapshot snapshot() const {
        return state_;
    }

  private:
    ProfileController &controller_;
    IJournalStore &store_;
    JournalSnapshot state_;
    JournalResult finish(JournalOutcome outcome, const DraftResult &result = DraftResult());
};
const char *journalOutcomeName(JournalOutcome value);
} // namespace devices
} // namespace larp
