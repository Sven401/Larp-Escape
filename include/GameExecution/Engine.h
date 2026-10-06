#pragma once
#include "GameConfiguration/Profile.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace execution {
using configuration::Action;
using configuration::ActionId;
using configuration::ComponentId;
using configuration::Condition;
using configuration::ConditionId;
using configuration::EffectId;
using configuration::Field;
using configuration::Limits;
using configuration::List;
using configuration::Profile;
using configuration::StateId;
using configuration::TransitionId;
using configuration::ValidationResult;
enum class ActionPhase : std::uint8_t { Entry, Tick, Exit, Transition };
struct ActionContext {
    StateId state;
    TransitionId transition;
    ActionPhase phase = ActionPhase::Tick;
    std::uint32_t nowMilliseconds = 0;
};
enum class ActionOutcome : std::uint8_t { Applied, Queued, Rejected, Unavailable, Deferred };
class IExecutionInputs {
  public:
    virtual ~IExecutionInputs() = default;
    // Read an already sampled value; no device I/O or event consumption here.
    // The engine caches each (field, component) once per tick. False = unavailable.
    virtual bool read(Field field, ComponentId component, std::uint32_t &value) noexcept = 0;
};
class IExecutionActions {
  public:
    virtual ~IExecutionActions() = default;
    // Synchronous dispatch from the sole application owner. Borrowed references
    // expire at return; asynchronous device owners must copy their parameters.
    // Queued is acceptance, not proof of a physical effect. Rejections are visible
    // and nonfatal; the remaining configured actions still execute in order.
    virtual ActionOutcome execute(const Profile &profile, const Action &action,
                                  const ActionContext &context) noexcept = 0;
};
enum class Outcome : std::uint8_t {
    Ok,
    Busy,
    InvalidProfile,
    AlreadyRunning,
    NotRunning,
    UnknownState,
    UnknownTransition,
    ManualDenied,
    StaleRevision,
    RevisionExhausted,
    InputUnavailable,
    InvalidInputValue,
    ClockDiscontinuity
};
struct Result {
    Outcome outcome = Outcome::Ok;
    ConditionId condition;
    Field field = Field::AudioBusy;
    ComponentId component;
    ValidationResult validation;
    explicit operator bool() const {
        return outcome == Outcome::Ok;
    }
};
struct ExecutionSnapshot {
    bool running = false;
    bool paused = false;
    bool entryPending = false;
    StateId state;
    TransitionId lastTransition;
    std::uint32_t profileRevision = 0;
    std::uint32_t stateRevision = 0; // State/control changes, not every sampled input.
    std::uint32_t stateElapsedMilliseconds = 0;
    std::uint32_t actionRequests = 0;
    std::uint32_t queuedActions = 0;
    std::uint32_t deferredActions = 0;
    std::uint32_t rejectedActions = 0;
    std::uint32_t unavailableActions = 0;
    ActionId lastFailedAction;
    ActionOutcome lastActionFailure = ActionOutcome::Applied;
    Result lastTickResult;
};
const char *outcomeName(Outcome value);
const char *actionOutcomeName(ActionOutcome value);
// Hardware-free execution kernel. All calls belong to one application owner;
// snapshot() is a value copy, NOT intertask synchronization. No heap allocation.
// The borrowed profile must remain alive and unchanged until stop() returns.
// start() requires a stopped engine; stopped snapshots retain copied metadata.
// The engine and profiles need caller-owned storage.
class Engine {
  public:
    Engine(IExecutionInputs &inputs, IExecutionActions &actions);
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Result start(const Profile &profile, std::uint32_t now);
    Result tick(std::uint32_t now);
    Result stop(std::uint32_t expectedRevision, std::uint32_t now);
    Result pause(std::uint32_t expectedRevision, std::uint32_t now);
    Result resume(std::uint32_t expectedRevision, std::uint32_t now);
    Result changeState(StateId target, std::uint32_t expectedRevision, std::uint32_t now);
    Result takeTransition(TransitionId transition, std::uint32_t expectedRevision,
                          std::uint32_t now);
    ExecutionSnapshot snapshot() const;
    // Sole-owner query; paused games remain running. No snapshot copy or intertask synchronization.
    bool running() const {
        return state_ != Missing;
    }

  private:
    static const std::uint8_t Missing = 255;
    static const std::size_t FieldCount = static_cast<std::size_t>(Field::RoundCount) + 1;
    static const std::size_t SampleCount = (Limits::Components + 1) * FieldCount;
    struct ActionList {
        std::array<std::uint8_t, Limits::StateActions> indices = {};
        std::uint8_t count = 0;
    };
    struct CompiledState {
        ActionList entry, tick, exit;
    };
    struct CompiledTransition {
        std::uint8_t source = Missing, target = Missing, condition = Missing;
        ActionList actions;
    };
    struct CompiledCondition {
        std::uint8_t first = Missing, second = Missing, componentSlot = 0;
    };
    struct Clock {
        std::uint32_t age = 0;
        bool observed = false;
    };
    IExecutionInputs &inputs_;
    IExecutionActions &actions_;
    const Profile *profile_ = nullptr;
    std::array<CompiledState, Limits::States> states_ = {};
    std::array<CompiledTransition, Limits::Transitions> transitions_ = {};
    std::array<CompiledCondition, Limits::Conditions> conditions_ = {};
    std::array<std::uint8_t, Limits::Transitions> order_ = {};
    std::array<Clock, Limits::Transitions> elapsed_ = {}, waits_ = {};
    std::array<std::uint8_t, Limits::Conditions> conditionCache_ = {};
    std::array<std::uint32_t, SampleCount> sampleValues_ = {};
    std::array<std::uint8_t, SampleCount> sampleStatus_ = {};
    std::uint8_t state_ = Missing;
    std::uint32_t revision_ = 0, profileRevision_ = 0, lastNow_ = 0, stateAge_ = 0;
    bool paused_ = false, entryPending_ = false, operating_ = false;
    TransitionId lastTransition_;
    std::uint32_t requests_ = 0, queued_ = 0, deferred_ = 0, rejected_ = 0, unavailable_ = 0;
    ActionId lastFailedAction_;
    ActionOutcome lastFailure_ = ActionOutcome::Applied;
    Result tickResult_;
    void compile();
    template <std::size_t N>
    void compileActions(const List<ActionId, N> &source, ActionList &target);
    void dispatch(const ActionList &list, ActionPhase phase, std::uint32_t now,
                  TransitionId transition = TransitionId());
    bool evaluate(std::uint8_t condition, std::uint8_t transition, std::size_t depth = 0);
    bool sample(const Condition &condition, std::uint8_t index, std::uint8_t transition,
                std::uint32_t &value);
    Result requestCheck(std::uint32_t expectedRevision) const;
    Result advanceClock(std::uint32_t now);
    void select(std::uint8_t target, std::uint32_t now, std::uint8_t transition = Missing);
    void resetTimers();
};
} // namespace execution
} // namespace larp
