#include "DeviceRuntime/Runtime.h"
#include <limits>
namespace larp {
namespace devices {
namespace {
struct Guard {
    explicit Guard(bool &value) : value(value) {
        value = true;
    }
    ~Guard() {
        value = false;
    }
    bool &value;
};
void increment(std::uint32_t &value) {
    if (value != std::numeric_limits<std::uint32_t>::max())
        ++value;
}
execution::Result denied(execution::Outcome outcome) {
    execution::Result result;
    result.outcome = outcome;
    return result;
}
template <std::size_t N> const char *name(unsigned index, const char *const (&names)[N]) {
    return index < N ? names[index] : "unknown_activation_value";
}
} // namespace
const char *activationPhaseName(ActivationPhase value) {
    static const char *const names[] = {"empty", "parking", "begin", "components", "commit",
                                        "bind",  "resume",  "ready", "fault"};
    return name(static_cast<unsigned>(value), names);
}
const char *activationOutcomeName(ActivationOutcome value) {
    static const char *const names[] = {
        "queued", "busy", "game_running", "stale_revision", "invalid_profile", "counter_exhausted"};
    return name(static_cast<unsigned>(value), names);
}
const char *activationErrorName(ActivationError value) {
    static const char *const names[] = {"none",
                                        "control_rejected",
                                        "control_failed",
                                        "operation_timeout",
                                        "transaction_timeout",
                                        "barrier_failure",
                                        "broker_rejected"};
    return name(static_cast<unsigned>(value), names);
}
const char *activationFinishName(ActivationFinish value) {
    static const char *const names[] = {"none", "applied", "rolled_back", "failed"};
    return name(static_cast<unsigned>(value), names);
}
bool Runtime::busy() const {
    return status_.phase != ActivationPhase::Empty && status_.phase != ActivationPhase::Ready &&
           status_.phase != ActivationPhase::Fault;
}
ActivationResult Runtime::activate(const Profile &candidate, std::uint32_t expectedRevision,
                                   std::uint32_t now) {
    ActivationResult result;
    if (operating_ || busy()) {
        result.outcome = ActivationOutcome::Busy;
        return result;
    }
    if (session_.running()) {
        result.outcome = ActivationOutcome::GameRunning;
        return result;
    }
    if (expectedRevision != status_.configurationRevision) {
        result.outcome = ActivationOutcome::StaleRevision;
        return result;
    }
    // Reserve generations for attempted setup, rollback and final cleanup.
    // Leave enough operation IDs for all three bounded phases (36 each).
    const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max();
    if (generationCounter_ > maximum - 3 || sequence_ > maximum - 128 ||
        status_.configurationRevision == maximum) {
        result.outcome = ActivationOutcome::CounterExhausted;
        return result;
    }
    Guard guard(operating_);
    result.plan = compilePlan(candidate, candidatePlan_);
    if (!result.plan) {
        result.outcome = ActivationOutcome::InvalidProfile;
        return result;
    }
    candidate_ = &candidate;
    status_.finish = ActivationFinish::None;
    status_.error = status_.recoveryError = status_.cleanupError = ActivationError::None;
    status_.hardwareReady = false;
    status_.failureOperation = ControlTicket();
    status_.failureComponent = ComponentId();
    status_.failureDetail = 0;
    status_.recovering = status_.cleaningUp = false;
    status_.requestedProfileRevision = candidate.revision;
    status_.workingGeneration = ++generationCounter_;
    transactionStarted_ = now;
    next(ActivationPhase::Parking);
    return result;
}
const Profile &Runtime::targetProfile() const {
    return status_.recovering ? *active_ : *candidate_;
}
const Plan &Runtime::targetPlan() const {
    return status_.recovering ? activePlan_ : candidatePlan_;
}
void Runtime::next(ActivationPhase phase) {
    status_.phase = phase;
    cursor_ = 0;
    hasOperation_ = sent_ = replied_ = false;
}
void Runtime::prepareOperation(std::uint32_t now) {
    operation_ = Control();
    operation_.ticket.sequence = ++sequence_;
    operation_.ticket.generation = status_.workingGeneration;
    operation_.ticket.owner = static_cast<Owner>(cursor_);
    switch (status_.phase) {
    case ActivationPhase::Parking:
        operation_.ticket.kind = ControlKind::Park;
        break;
    case ActivationPhase::Begin:
        operation_.ticket.kind = ControlKind::Begin;
        operation_.profileRevision = targetProfile().revision;
        operation_.expectedComponents = targetPlan().ownerComponents[cursor_];
        operation_.i2c = targetPlan().i2c;
        operation_.peerChannel = targetPlan().peerChannel;
        break;
    case ActivationPhase::Components: {
        operation_.ticket.kind = ControlKind::Component;
        const std::uint8_t slot = targetPlan().initializationOrder[cursor_];
        operation_.ticket.owner = targetPlan().routes[slot].owner;
        operation_.componentSlot = slot;
        operation_.component = *targetProfile().components.get(slot);
        break;
    }
    case ActivationPhase::Commit:
        operation_.ticket.kind = ControlKind::Commit;
        break;
    case ActivationPhase::Resume:
        operation_.ticket.kind = ControlKind::Resume;
        break;
    default:
        return;
    }
    status_.operation = operation_.ticket;
    status_.component =
        operation_.ticket.kind == ControlKind::Component ? operation_.component.id : ComponentId();
    status_.detail = 0;
    operationStarted_ = now;
    hasOperation_ = true;
    sent_ = replied_ = false;
}
void Runtime::finishFault() {
    status_.phase = ActivationPhase::Fault;
    status_.finish = ActivationFinish::Failed;
    status_.hardwareReady = false;
    status_.recovering = status_.cleaningUp = false;
    candidate_ = nullptr;
    hasOperation_ = sent_ = replied_ = false;
}
void Runtime::fail(ActivationError error, std::uint32_t now) {
    if (status_.cleaningUp) {
        status_.cleanupError = error;
        finishFault();
        return;
    }
    if (status_.recovering) {
        status_.recoveryError = error;
        status_.recovering = false;
        status_.cleaningUp = true;
    } else {
        status_.error = error;
        status_.failureOperation = status_.operation;
        status_.failureComponent = status_.component;
        status_.failureDetail = status_.detail;
        status_.recovering = active_ != nullptr;
        status_.cleaningUp = active_ == nullptr;
    }
    status_.workingGeneration = ++generationCounter_;
    transactionStarted_ = now;
    next(ActivationPhase::Parking);
}
void Runtime::advance(std::uint32_t now) {
    hasOperation_ = sent_ = replied_ = false;
    increment(status_.completedOperations);
    switch (status_.phase) {
    case ActivationPhase::Parking:
        if (++cursor_ == ChannelLimits::Owners) {
            if (!channel_.quiescent()) {
                fail(ActivationError::BarrierFailure, now);
                return;
            }
            if (status_.cleaningUp) {
                finishFault();
                return;
            }
            next(ActivationPhase::Begin);
        }
        break;
    case ActivationPhase::Begin:
        if (++cursor_ == ChannelLimits::Owners)
            next(targetPlan().enabledCount ? ActivationPhase::Components : ActivationPhase::Commit);
        break;
    case ActivationPhase::Components:
        if (++cursor_ == targetPlan().enabledCount)
            next(ActivationPhase::Commit);
        break;
    case ActivationPhase::Commit:
        if (++cursor_ == ChannelLimits::Owners)
            next(ActivationPhase::Bind);
        break;
    case ActivationPhase::Resume:
        if (++cursor_ == ChannelLimits::Owners) {
            if (status_.recovering)
                status_.finish = ActivationFinish::RolledBack;
            else {
                active_ = candidate_;
                activePlan_ = candidatePlan_;
                ++status_.configurationRevision;
                status_.finish = ActivationFinish::Applied;
            }
            status_.generation = status_.workingGeneration;
            status_.profileRevision = active_->revision;
            status_.hardwareReady = true;
            status_.recovering = false;
            candidate_ = nullptr;
            next(ActivationPhase::Ready);
        }
        break;
    default:
        break;
    }
}
void Runtime::service(std::uint32_t now) {
    if (operating_)
        return;
    Guard guard(operating_);
    broker_.service();
    ControlReply reply;
    for (std::size_t count = 0;
         count < ActivationLimits::ReplyDrain && controls_.receiveControlReply(reply); ++count) {
        if (!busy() || !hasOperation_ || !sent_ || replied_ ||
            !sameControlTicket(reply.ticket, operation_.ticket)) {
            increment(status_.staleReplies);
            continue;
        }
        reply_ = reply;
        replied_ = true;
    }
    if (!busy())
        return;
    if (static_cast<std::uint32_t>(now - transactionStarted_) >=
        ActivationLimits::TransactionMilliseconds) {
        fail(ActivationError::TransactionTimeout, now);
        return;
    }
    if (status_.phase == ActivationPhase::Bind) {
        if (!broker_.configure(targetPlan(), status_.workingGeneration)) {
            fail(ActivationError::BrokerRejected, now);
            return;
        }
        next(ActivationPhase::Resume);
        return;
    }
    if (!hasOperation_)
        prepareOperation(now);
    if (replied_) {
        status_.detail = reply_.detail;
        if (static_cast<std::uint32_t>(reply_.completedMilliseconds - operationStarted_) >=
            ActivationLimits::OperationMilliseconds)
            fail(ActivationError::OperationTimeout, now);
        else if (reply_.outcome != ControlOutcome::Applied)
            fail(reply_.outcome == ControlOutcome::Rejected ? ActivationError::ControlRejected
                                                            : ActivationError::ControlFailed,
                 now);
        else
            advance(now);
        return;
    }
    if (static_cast<std::uint32_t>(now - operationStarted_) >=
        ActivationLimits::OperationMilliseconds) {
        fail(ActivationError::OperationTimeout, now);
        return;
    }
    if (!sent_) {
        sent_ = controls_.sendControl(operation_);
        if (!sent_)
            increment(status_.queueRejections);
    }
}
execution::SessionSnapshot Runtime::game() const {
    execution::SessionSnapshot value;
    const bool sameConfiguration = gameConfigurationRevision_ != 0 &&
                                   gameConfigurationRevision_ == status_.configurationRevision;
    if (sameConfiguration)
        value = session_.snapshot();
    else {
        // The stopped engine keeps its monotone revision across configuration
        // changes. Do not publish role/action/device history from another profile.
        value.execution.stateRevision = session_.execution().stateRevision;
        value.execution.profileRevision = status_.profileRevision;
    }
    if (!active_)
        return value;
    value.deviceCount = static_cast<std::uint8_t>(active_->components.size());
    for (std::size_t i = 0; i < value.deviceCount; ++i) {
        const auto &component = *active_->components.get(i);
        auto &device = value.devices[i];
        device.id = component.id;
        device.enabled = component.enabled;
        DeviceObservation observation;
        const bool observed = status_.hardwareReady &&
                              channel_.observe(static_cast<std::uint8_t>(i), observation) &&
                              observation.generation == status_.generation;
        device.available = component.enabled && observed && observation.sample.available;
        if (!value.execution.running) {
            device.pollPending = false;
            device.lastOutcome =
                observed ? observation.lastOutcome : execution::ActionOutcome::Unavailable;
        }
    }
    return value;
}
bool Runtime::gameAllowed() const {
    return !operating_ && status_.phase == ActivationPhase::Ready && status_.hardwareReady &&
           active_;
}
execution::Result Runtime::startGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    if (revision != status_.configurationRevision)
        return denied(execution::Outcome::StaleRevision);
    Guard guard(operating_);
    const auto result = session_.start(*active_, now);
    if (result)
        gameConfigurationRevision_ = status_.configurationRevision;
    return result;
}
execution::Result Runtime::tickGame(std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.tick(now);
}
execution::Result Runtime::stopGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.stop(revision, now);
}
execution::Result Runtime::pauseGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.pause(revision, now);
}
execution::Result Runtime::resumeGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.resume(revision, now);
}
execution::Result Runtime::changeState(configuration::StateId target, std::uint32_t revision,
                                       std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.changeState(target, revision, now);
}
execution::Result Runtime::takeTransition(configuration::TransitionId edge, std::uint32_t revision,
                                          std::uint32_t now) {
    if (!gameAllowed())
        return denied(execution::Outcome::Busy);
    Guard guard(operating_);
    return session_.takeTransition(edge, revision, now);
}
} // namespace devices
} // namespace larp
