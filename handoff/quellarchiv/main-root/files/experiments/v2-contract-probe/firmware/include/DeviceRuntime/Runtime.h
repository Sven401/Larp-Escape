#pragma once
#include "DeviceRuntime/Broker.h"
#include "DeviceRuntime/Lifecycle.h"
namespace larp {
namespace devices {
enum class ActivationPhase : std::uint8_t {
    Empty,
    Parking,
    Begin,
    Components,
    Commit,
    Bind,
    Resume,
    Ready,
    Fault
};
enum class ActivationOutcome : std::uint8_t {
    Queued,
    Busy,
    GameRunning,
    StaleRevision,
    InvalidProfile,
    CounterExhausted
};
enum class ActivationError : std::uint8_t {
    None,
    ControlRejected,
    ControlFailed,
    OperationTimeout,
    TransactionTimeout,
    BarrierFailure,
    BrokerRejected
};
enum class ActivationFinish : std::uint8_t { None, Applied, RolledBack, Failed };
struct ActivationLimits {
    static const std::uint32_t OperationMilliseconds = 5000;
    static const std::uint32_t TransactionMilliseconds = 60000;
    static const std::size_t ReplyDrain = ChannelLimits::Owners;
};
struct ActivationResult {
    ActivationOutcome outcome = ActivationOutcome::Queued;
    PlanResult plan;
    explicit operator bool() const {
        return outcome == ActivationOutcome::Queued;
    }
};
struct ActivationSnapshot {
    ActivationPhase phase = ActivationPhase::Empty;
    ActivationFinish finish = ActivationFinish::None;
    ActivationError error = ActivationError::None, recoveryError = ActivationError::None,
                    cleanupError = ActivationError::None;
    std::uint32_t configurationRevision = 0, generation = 0, workingGeneration = 0;
    std::uint32_t profileRevision = 0, requestedProfileRevision = 0;
    std::uint32_t staleReplies = 0, queueRejections = 0, completedOperations = 0;
    ControlTicket operation, failureOperation;
    ComponentId component, failureComponent;
    std::uint16_t detail = 0, failureDetail = 0;
    bool hardwareReady = false, recovering = false, cleaningUp = false;
};
// Sole application owner of Broker/Session and profile activation. Profiles are
// borrowed immutable storage. Keep the active profile alive until replacement;
// keep the candidate alive until activation terminates. No Profile crosses a task.
// Hardware changes require an already stopped game. Game control is gated while
// switching/faulted; each service processes bounded copied control responses.
class Runtime {
  public:
    Runtime(IChannel &channel, IControlChannel &controls)
        : channel_(channel), controls_(controls), broker_(channel), session_(broker_) {}
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    ActivationResult activate(const Profile &candidate, std::uint32_t expectedRevision,
                              std::uint32_t now);
    void service(std::uint32_t now);
    bool busy() const;
    ActivationSnapshot activation() const {
        return status_;
    }
    // Current configuration inventory, generation-checked copied observations.
    // Old Session metadata is hidden after replacement; stateRevision survives.
    execution::SessionSnapshot game() const;
    execution::ExecutionSnapshot execution() const {
        return session_.execution();
    }
    bool gameRunning() const {
        return session_.running();
    }
    BrokerSnapshot traffic() const {
        return broker_.snapshot();
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
    IChannel &channel_;
    IControlChannel &controls_;
    Broker broker_;
    execution::Session session_;
    const Profile *active_ = nullptr, *candidate_ = nullptr;
    Plan activePlan_, candidatePlan_;
    ActivationSnapshot status_;
    Control operation_;
    ControlReply reply_;
    std::uint32_t gameConfigurationRevision_ = 0;
    std::uint32_t generationCounter_ = 0, sequence_ = 0, transactionStarted_ = 0,
                  operationStarted_ = 0;
    std::uint8_t cursor_ = 0;
    bool hasOperation_ = false, sent_ = false, replied_ = false, operating_ = false;
    const Profile &targetProfile() const;
    const Plan &targetPlan() const;
    void prepareOperation(std::uint32_t now);
    void advance(std::uint32_t now);
    void fail(ActivationError error, std::uint32_t now);
    void finishFault();
    void next(ActivationPhase phase);
    bool gameAllowed() const;
};
const char *activationPhaseName(ActivationPhase value);
const char *activationOutcomeName(ActivationOutcome value);
const char *activationErrorName(ActivationError value);
const char *activationFinishName(ActivationFinish value);
} // namespace devices
} // namespace larp
