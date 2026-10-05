#pragma once
#include "GameDomain/GameDefinitions.h"
#include "GameDomain/KeystoneTransition.h"
#include "GameExecution/Engine.h"
#include "GamePorts/CrystalInputBatch.h"

namespace larp {
namespace execution {
struct DeviceSample {
    bool available = false;
    // Includes accepted/queued playback until completion, not just a raw BUSY pin.
    bool audioBusy = false;
    std::array<bool, Limits::WakeInputs> wakes = {};
};
struct PollRequest {
    ComponentId component;
    ActionId requester;
    std::uint32_t profileRevision = 0;
    std::uint32_t stateRevision = 0;
    std::uint32_t nowMilliseconds = 0;
    std::uint16_t timeoutMilliseconds = 0;
};
enum class PollOutcome : std::uint8_t { Fresh, Pending, Queued, Rejected, Unavailable };
class IProfileDevices {
  public:
    virtual ~IProfileDevices() = default;
    // All methods are bounded/nonblocking. Read synchronized owner snapshots;
    // issue copied requests to device owners. Never hold these borrowed values.
    virtual bool sample(const configuration::Component &component,
                        DeviceSample &value) noexcept = 0;
    // Fresh consumes one correlated answer. Queued means a NEW accepted request;
    // Pending means that request still runs. Match profile/state revision and
    // requester; discard late answers from a previous state or profile.
    virtual PollOutcome pollCard(const PollRequest &request, domain::CardUid &uid) noexcept = 0;
    virtual PollOutcome pollCrystals(const PollRequest &request,
                                     ports::CrystalInputBatch &batch) noexcept = 0;
    virtual ActionOutcome playTrack(ComponentId component, std::uint16_t track,
                                    const ActionContext &context) noexcept = 0;
    virtual ActionOutcome wakeAudio(ComponentId component,
                                    const ActionContext &context) noexcept = 0;
    virtual ActionOutcome applyEffect(const configuration::Effect &effect, ActionId binding,
                                      const ActionContext &context) noexcept = 0;
    virtual ActionOutcome sendActivation(ComponentId component,
                                         const ActionContext &context) noexcept = 0;
};
struct SessionDeviceSnapshot {
    ComponentId id;
    bool enabled = false, available = false;
    bool cardReadable = true, matrixReadable = true, pollPending = false;
    bool roleAccepted = false, crystalAvailable = false, incorrectCrystal = false,
         keystoneError = false;
    ActionOutcome lastOutcome = ActionOutcome::Applied;
};
struct SessionSnapshot {
    ExecutionSnapshot execution;
    bool rolePresent = false; // Recognized profile, not proof that a card is present.
    configuration::RoleId role;
    std::uint8_t crystalCount = 0, requiredCrystals = 0, seenRoleCount = 0;
    std::uint32_t roundCount = 0;
    bool crystalAudioPending = false, incorrectCrystal = false, keystoneError = false;
    domain::KeystoneTransition lastCardDecision = domain::KeystoneTransition::EmptyRead;
    std::array<SessionDeviceSnapshot, Limits::Components> devices = {};
    std::uint8_t deviceCount = 0;
};
// One logical game with profile-owned IDs/contents; no legacy role enum casts.
// Same ownership/lifetime rules as Engine. Private ports prevent outside dispatch.
class Session : private IExecutionInputs, private IExecutionActions {
  public:
    explicit Session(IProfileDevices &devices);
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Result start(const Profile &profile, std::uint32_t now);
    Result tick(std::uint32_t now);
    Result stop(std::uint32_t expectedRevision, std::uint32_t now);
    Result pause(std::uint32_t expectedRevision, std::uint32_t now);
    Result resume(std::uint32_t expectedRevision, std::uint32_t now);
    Result changeState(StateId target, std::uint32_t expectedRevision, std::uint32_t now);
    Result takeTransition(TransitionId transition, std::uint32_t expectedRevision,
                          std::uint32_t now);
    SessionSnapshot snapshot() const;

  private:
    static const std::uint8_t Missing = 255;
    struct DeviceState {
        SessionDeviceSnapshot status;
        DeviceSample sample;
        std::uint8_t retry = 0, misses = 0;
    };
    struct PollClock {
        std::uint32_t last = 0, pendingRevision = 0;
        bool started = false, pending = false, checked = false;
    };
    struct RecordedCrystal {
        std::uint8_t definition = Missing;
        bool audioRequested = false;
    };
    IProfileDevices &devices_;
    Engine engine_;
    const Profile *profile_ = nullptr;
    std::array<DeviceState, Limits::Components> deviceStates_ = {};
    std::array<PollClock, Limits::Actions> clocks_ = {};
    std::array<RecordedCrystal, Limits::CrystalsPerRole> crystals_ = {};
    std::array<bool, Limits::Roles> seen_ = {};
    std::uint8_t componentCount_ = 0, roleIndex_ = Missing, crystalCount_ = 0,
                 requiredCrystals_ = 0, seenCount_ = 0;
    configuration::RoleId roleId_;
    std::uint32_t rounds_ = 0;
    domain::CardUid currentUid_ = {}, lastUid_ = {};
    domain::KeystoneTransition lastDecision_ = domain::KeystoneTransition::EmptyRead;
    bool operating_ = false;
    bool read(Field field, ComponentId component, std::uint32_t &value) noexcept override;
    ActionOutcome execute(const Profile &profile, const Action &action,
                          const ActionContext &context) noexcept override;
    std::uint8_t componentIndex(ComponentId id) const;
    std::uint8_t actionIndex(ActionId id) const;
    void resetRound();
    void resetGame();
    bool pendingAudio() const;
    bool beginPoll(std::uint8_t index, const Action &action, const ActionContext &context,
                   PollRequest &request);
    ActionOutcome pollResult(std::uint8_t action, std::uint8_t component, PollOutcome outcome,
                             std::uint32_t revision);
    ActionOutcome readRole(const Action &action, const ActionContext &context);
    ActionOutcome readCrystals(const Action &action, const ActionContext &context);
    ActionOutcome observeKeystone(const Action &action, const ActionContext &context);
    ActionOutcome applyCrystalPalette(const Action &action, const configuration::Crystal &crystal,
                                      const ActionContext &context);
    ActionOutcome remember(ComponentId component, ActionOutcome outcome);
};
} // namespace execution
} // namespace larp
