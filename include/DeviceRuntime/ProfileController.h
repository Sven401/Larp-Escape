#pragma once
#include "DeviceRuntime/Runtime.h"
#include "GameConfiguration/ProfileReader.h"
#include "GameConfiguration/ProfileWriter.h"
#include <memory>
namespace larp {
namespace devices {
enum class FactoryProfile : std::uint8_t { Standard, Hint };
enum class DraftOutcome : std::uint8_t {
    Ready,
    Queued,
    Busy,
    Uninitialized,
    GameRunning,
    StaleRevision,
    InvalidSource,
    ImportFailed,
    InvalidProfile,
    CounterExhausted,
    NoDraft,
    NoActive,
    OutputFailed,
    ActivationRejected
};
struct DraftResult {
    DraftOutcome outcome = DraftOutcome::Ready;
    configuration::ImportResult import;
    configuration::ValidationResult validation;
    ActivationResult activation;
    explicit operator bool() const {
        return outcome == DraftOutcome::Ready || outcome == DraftOutcome::Queued;
    }
};
struct ProfileStorageSnapshot {
    std::uint32_t draftRevision = 0, activeProfileRevision = 0, draftProfileRevision = 0;
    bool initialized = false, hasActive = false, draftReady = false, applying = false,
         editing = false;
    configuration::ImportResult lastImport;
};
// Process-lifetime sole application owner. Two full Profiles are allocated once
// by beginStorage() before workers; no recurring allocation, exposed pointers or
// mutable references. Runtime borrows only locked owned storage. Large synchronous
// import/export/factory operations require a stopped game; transport sources/sinks
// must be bounded. Not a cross-task API or an incremental network parser.
class ProfileController {
  public:
    ProfileController(IChannel &channel, IControlChannel &controls) : runtime_(channel, controls) {}
    ProfileController(const ProfileController &) = delete;
    ProfileController &operator=(const ProfileController &) = delete;
    bool beginStorage();
    DraftResult createDraft(FactoryProfile source, std::uint32_t expectedDraftRevision);
    DraftResult importDraft(configuration::IProfileReader &reader,
                            std::uint32_t expectedDraftRevision);
    DraftResult activateDraft(std::uint32_t expectedDraftRevision,
                              std::uint32_t expectedConfigurationRevision, std::uint32_t now);
    DraftResult writeDraft(configuration::IProfileWriter &writer,
                           std::uint32_t expectedDraftRevision);
    DraftResult writeActive(configuration::IProfileWriter &writer,
                            std::uint32_t expectedConfigurationRevision);
    void service(std::uint32_t now);
    ProfileStorageSnapshot storage() const;
    ActivationSnapshot activation() const {
        return runtime_.activation();
    }
    execution::SessionSnapshot game() const {
        return runtime_.game();
    }
    bool gameRunning() const {
        return runtime_.gameRunning();
    }
    BrokerSnapshot traffic() const {
        return runtime_.traffic();
    }
    bool busy() const {
        return operating_ || applying_ || runtime_.busy();
    }
    execution::Result startGame(std::uint32_t expectedConfigurationRevision, std::uint32_t now);
    execution::Result tickGame(std::uint32_t now);
    execution::Result stopGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result pauseGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result resumeGame(std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result changeState(configuration::StateId target,
                                  std::uint32_t expectedStateRevision, std::uint32_t now);
    execution::Result takeTransition(configuration::TransitionId edge,
                                     std::uint32_t expectedStateRevision, std::uint32_t now);

  private:
    static const std::uint8_t Missing = 255;
    std::array<std::unique_ptr<Profile>, 2> profiles_;
    Runtime runtime_;
    std::uint32_t draftRevision_ = 0;
    configuration::ImportResult lastImport_;
    std::uint8_t active_ = Missing, draft_ = 0;
    bool initialized_ = false, draftReady_ = false, applying_ = false, operating_ = false;
    DraftOutcome admission() const;
    DraftOutcome editAdmission(std::uint32_t expectedRevision) const;
};
const char *draftOutcomeName(DraftOutcome value);
} // namespace devices
} // namespace larp
