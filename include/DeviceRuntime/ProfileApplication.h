#pragma once
#include "DeviceRuntime/ProfileJournal.h"
namespace larp {
namespace devices {
enum class ApplicationPhase : std::uint8_t {
    Empty,
    PendingActivation,
    Applying,
    Saving,
    Ready,
    Fault
};
enum class ApplicationOperation : std::uint8_t { None, Boot, Apply, Restore };
enum class ApplicationOutcome : std::uint8_t {
    Queued,
    Ready,
    Busy,
    NotReady,
    ControllerRejected,
    StorageRejected
};
enum class BootPolicy : std::uint8_t { ExistingOnly, InitializeEmpty };
struct ApplicationResult {
    ApplicationOutcome outcome = ApplicationOutcome::NotReady;
    DraftResult controller;
    JournalOutcome journal = JournalOutcome::NeedsRestore;
    explicit operator bool() const {
        return outcome == ApplicationOutcome::Queued || outcome == ApplicationOutcome::Ready;
    }
};
struct ApplicationSnapshot {
    ApplicationPhase phase = ApplicationPhase::Empty;
    ApplicationOperation operation = ApplicationOperation::None;
    ApplicationOutcome outcome = ApplicationOutcome::NotReady;
    DraftOutcome controller = DraftOutcome::Ready;
    JournalOutcome journal = JournalOutcome::NeedsRestore;
    std::uint32_t storedGeneration = 0, storedProfileRevision = 0, storedConfigurationRevision = 0;
    bool hasStoredProfile = false, storedMatchesActive = false, fallbackAttempted = false;
};
// Process-lifetime sole application context. Owns Controller + Journal; no
// mutable/raw owner access. Backend must already be mounted/opened. Synchronous
// bounded profile IO is stopped-game work, not a callback/task API. begin/restore
// prepare a locked draft; the NEXT service(now) queues activation with fresh time.
// service drains runtime replies, advances setup/save and ticks a Ready game.
// Never auto-starts. Start requires confirmed storage matching active RAM.
class ProfileApplication {
  public:
    ProfileApplication(IChannel &channel, IControlChannel &controls, IJournalStore &store)
        : controller_(channel, controls), journal_(controller_, store) {}
    ProfileApplication(const ProfileApplication &) = delete;
    ProfileApplication &operator=(const ProfileApplication &) = delete;
    ApplicationResult begin(BootPolicy policy = BootPolicy::InitializeEmpty,
                            FactoryProfile factory = FactoryProfile::Standard,
                            configuration::IProfileReader *initial = nullptr);
    DraftResult createDraft(FactoryProfile factory, std::uint32_t expectedDraftRevision);
    DraftResult importDraft(configuration::IProfileReader &reader,
                            std::uint32_t expectedDraftRevision);
    DraftResult writeDraft(configuration::IProfileWriter &writer,
                           std::uint32_t expectedDraftRevision);
    DraftResult writeActive(configuration::IProfileWriter &writer,
                            std::uint32_t expectedConfigurationRevision);
    ApplicationResult activateDraft(std::uint32_t expectedDraftRevision,
                                    std::uint32_t expectedConfigurationRevision, std::uint32_t now);
    ApplicationResult restoreStored(std::uint32_t expectedDraftRevision,
                                    std::uint32_t expectedConfigurationRevision,
                                    bool older = false);
    void service(std::uint32_t now);
    bool busy() const;
    execution::ExecutionSnapshot execution() const {
        return controller_.execution();
    }
    bool gameRunning() const {
        return controller_.gameRunning();
    }
    ApplicationSnapshot snapshot() const;
    ProfileStorageSnapshot storage() const {
        return controller_.storage();
    }
    ActivationSnapshot activation() const {
        return controller_.activation();
    }
    execution::SessionSnapshot game() const {
        return controller_.game();
    }
    BrokerSnapshot traffic() const {
        return controller_.traffic();
    }
    JournalSnapshot persistence() const {
        return journal_.snapshot();
    }
    execution::Result startGame(std::uint32_t expectedConfigurationRevision, std::uint32_t now);
    execution::Result stopGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result pauseGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result resumeGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result changeState(configuration::StateId target,
                                  std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result takeTransition(configuration::TransitionId edge,
                                     std::uint32_t expectedStateRevision, std::uint32_t now);

  private:
    ProfileController controller_;
    ProfileJournal journal_;
    ApplicationSnapshot state_;
    std::uint32_t loadedGeneration_ = 0, loadedProfileRevision_ = 0;
    bool operating_ = false, initialized_ = false, needsSave_ = false, bootFallback_ = false;
    void prepareStored(bool forceSave = false);
    void storedReady();
    bool gameAllowed() const;
    ApplicationResult result(ApplicationOutcome outcome,
                             const DraftResult &controller = DraftResult());
};
const char *applicationPhaseName(ApplicationPhase value);
const char *applicationOutcomeName(ApplicationOutcome value);
} // namespace devices
} // namespace larp
