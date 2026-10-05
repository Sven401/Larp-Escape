#include "GameExecution/Engine.h"
#include <limits>

namespace larp {
namespace execution {
using namespace configuration;
namespace {
static_assert(Limits::States < 255 && Limits::Transitions < 255 && Limits::Conditions < 255 &&
                  Limits::Actions < 255 && Limits::Components < 255,
              "Compiled indices require a reserved byte");
struct OperationGuard {
    explicit OperationGuard(bool &busy) : busy(busy) {
        busy = true;
    }
    ~OperationGuard() {
        busy = false;
    }
    bool &busy;
};
Result result(Outcome outcome) {
    Result value;
    value.outcome = outcome;
    return value;
}
std::uint32_t saturatedAdd(std::uint32_t value, std::uint32_t delta, std::uint32_t limit) {
    return delta > limit - value ? limit : value + delta;
}
void increment(std::uint32_t &value) {
    if (value != std::numeric_limits<std::uint32_t>::max())
        ++value;
}
template <typename T, std::size_t N, typename Key>
std::uint8_t indexOf(const List<T, N> &items, Key key) {
    for (std::size_t index = 0; index < items.size(); ++index)
        if (items.get(index)->id == key)
            return static_cast<std::uint8_t>(index);
    return 255;
}
bool compare(std::uint32_t value, Compare operation, std::uint32_t operand) {
    switch (operation) {
    case Compare::Equal:
        return value == operand;
    case Compare::NotEqual:
        return value != operand;
    case Compare::Less:
        return value < operand;
    case Compare::LessEqual:
        return value <= operand;
    case Compare::Greater:
        return value > operand;
    case Compare::GreaterEqual:
        return value >= operand;
    }
    return false;
}
} // namespace
const char *outcomeName(Outcome value) {
    static const char *const names[] = {"ok",
                                        "busy",
                                        "invalid_profile",
                                        "already_running",
                                        "not_running",
                                        "unknown_state",
                                        "unknown_transition",
                                        "manual_denied",
                                        "stale_revision",
                                        "revision_exhausted",
                                        "input_unavailable",
                                        "invalid_input_value",
                                        "clock_discontinuity"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_outcome";
}
Engine::Engine(IExecutionInputs &inputs, IExecutionActions &actions)
    : inputs_(inputs), actions_(actions) {}
template <std::size_t N>
void Engine::compileActions(const List<ActionId, N> &source, ActionList &target) {
    static_assert(N <= Limits::StateActions, "Compiled action storage too small");
    target.count = static_cast<std::uint8_t>(source.size());
    for (std::size_t index = 0; index < source.size(); ++index)
        target.indices[index] = indexOf(profile_->actions, *source.get(index));
}
void Engine::compile() {
    for (std::size_t index = 0; index < profile_->states.size(); ++index) {
        const State &state = *profile_->states.get(index);
        compileActions(state.entry, states_[index].entry);
        compileActions(state.tick, states_[index].tick);
        compileActions(state.exit, states_[index].exit);
    }
    for (std::size_t index = 0; index < profile_->conditions.size(); ++index) {
        const Condition &node = *profile_->conditions.get(index);
        conditions_[index].first = indexOf(profile_->conditions, node.first);
        conditions_[index].second = indexOf(profile_->conditions, node.second);
        conditions_[index].componentSlot =
            node.component.value == 0
                ? 0
                : static_cast<std::uint8_t>(indexOf(profile_->components, node.component) + 1);
    }
    for (std::size_t index = 0; index < profile_->transitions.size(); ++index) {
        const Transition &transition = *profile_->transitions.get(index);
        transitions_[index].source = indexOf(profile_->states, transition.source);
        transitions_[index].target = indexOf(profile_->states, transition.target);
        transitions_[index].condition = indexOf(profile_->conditions, transition.condition);
        compileActions(transition.actions, transitions_[index].actions);
        // Stable insertion order by priority. Filtering by source at runtime
        // retains the unique per-source order independent of JSON table order.
        std::size_t position = index;
        while (position > 0 &&
               profile_->transitions.get(order_[position - 1])->priority > transition.priority) {
            order_[position] = order_[position - 1];
            --position;
        }
        order_[position] = static_cast<std::uint8_t>(index);
    }
}
Result Engine::start(const Profile &profile, std::uint32_t now) {
    if (operating_)
        return result(Outcome::Busy);
    if (state_ != Missing)
        return result(Outcome::AlreadyRunning);
    if (revision_ == std::numeric_limits<std::uint32_t>::max())
        return result(Outcome::RevisionExhausted);
    Result value;
    value.validation = validateProfile(profile);
    if (!value.validation) {
        value.outcome = Outcome::InvalidProfile;
        return value;
    }
    OperationGuard guard(operating_);
    profile_ = &profile;
    profileRevision_ = profile.revision;
    compile();
    state_ = indexOf(profile.states, profile.initialState);
    ++revision_;
    lastNow_ = now;
    paused_ = false;
    entryPending_ = true;
    lastTransition_ = TransitionId();
    requests_ = queued_ = rejected_ = unavailable_ = 0;
    lastFailedAction_ = ActionId();
    lastFailure_ = ActionOutcome::Applied;
    tickResult_ = {};
    resetTimers();
    return value;
}
void Engine::resetTimers() {
    stateAge_ = 0;
    for (Clock &clock : elapsed_) {
        clock.age = 0;
        clock.observed = false;
    }
    for (Clock &clock : waits_) {
        clock.age = 0;
        clock.observed = false;
    }
}
Result Engine::advanceClock(std::uint32_t now) {
    const std::uint32_t delta = now - lastNow_;
    if (delta > 0x7FFFFFFFU)
        return result(Outcome::ClockDiscontinuity);
    lastNow_ = now;
    if (paused_ || entryPending_)
        return {};
    stateAge_ = saturatedAdd(stateAge_, delta, std::numeric_limits<std::uint32_t>::max());
    for (Clock &clock : elapsed_)
        if (clock.observed)
            clock.age = saturatedAdd(clock.age, delta, std::numeric_limits<std::uint32_t>::max());
    for (Clock &clock : waits_)
        if (clock.observed)
            clock.age = saturatedAdd(clock.age, delta, std::numeric_limits<std::uint32_t>::max());
    return {};
}
void Engine::dispatch(const ActionList &list, ActionPhase phase, std::uint32_t now,
                      TransitionId transition) {
    ActionContext context;
    context.state = profile_->states.get(state_)->id;
    context.phase = phase;
    context.nowMilliseconds = now;
    context.transition = transition;
    for (std::size_t index = 0; index < list.count; ++index) {
        const Action &action = *profile_->actions.get(list.indices[index]);
        increment(requests_);
        const ActionOutcome outcome = actions_.execute(*profile_, action, context);
        if (outcome == ActionOutcome::Queued)
            increment(queued_);
        else if (outcome != ActionOutcome::Applied) {
            if (outcome == ActionOutcome::Rejected)
                increment(rejected_);
            else
                increment(unavailable_);
            lastFailedAction_ = action.id;
            lastFailure_ =
                outcome == ActionOutcome::Rejected ? outcome : ActionOutcome::Unavailable;
        }
    }
}
bool Engine::sample(const Condition &condition, std::uint8_t index, std::uint8_t transition,
                    std::uint32_t &value) {
    if (condition.field == Field::StateElapsedMilliseconds) {
        value = stateAge_;
        return true;
    }
    if (condition.field == Field::TransitionElapsedMilliseconds) {
        Clock &clock = elapsed_[transition];
        if (!clock.observed) {
            clock.observed = true;
            clock.age = 0;
        }
        value = clock.age;
        return true;
    }
    const std::size_t slot =
        conditions_[index].componentSlot * FieldCount + static_cast<std::size_t>(condition.field);
    if (sampleStatus_[slot] == 0) {
        if (!inputs_.read(condition.field, condition.component, sampleValues_[slot]))
            sampleStatus_[slot] = 2;
        else if (condition.field <= Field::AllWakeActive && sampleValues_[slot] > 1)
            sampleStatus_[slot] = 3;
        else
            sampleStatus_[slot] = 1;
    }
    if (sampleStatus_[slot] != 1) {
        tickResult_.outcome =
            sampleStatus_[slot] == 2 ? Outcome::InputUnavailable : Outcome::InvalidInputValue;
        tickResult_.condition = condition.id;
        tickResult_.field = condition.field;
        tickResult_.component = condition.component;
        return false;
    }
    value = sampleValues_[slot];
    return true;
}
bool Engine::evaluate(std::uint8_t index, std::uint8_t transition, std::size_t depth) {
    if (index >= profile_->conditions.size() || depth == Limits::ConditionDepth) {
        tickResult_.outcome = Outcome::InvalidProfile;
        return false;
    }
    if (conditionCache_[index] != 0)
        return conditionCache_[index] == 2;
    const Condition &node = *profile_->conditions.get(index);
    bool matched = false;
    switch (node.kind) {
    case ConditionKind::Predicate: {
        std::uint32_t value = 0;
        if (sample(node, index, transition, value))
            matched = compare(value, node.comparison, node.operand);
        break;
    }
    case ConditionKind::Wait: {
        Clock &clock = waits_[transition];
        if (!clock.observed) {
            clock.observed = true;
            clock.age = 0;
        } else if (clock.age >= node.operand) {
            clock.observed = false;
            clock.age = 0;
            matched = true;
        }
        break;
    }
    case ConditionKind::All:
        matched = evaluate(conditions_[index].first, transition, depth + 1);
        if (tickResult_ && matched)
            matched = evaluate(conditions_[index].second, transition, depth + 1);
        break;
    case ConditionKind::Any:
        matched = evaluate(conditions_[index].first, transition, depth + 1);
        if (tickResult_ && !matched)
            matched = evaluate(conditions_[index].second, transition, depth + 1);
        break;
    case ConditionKind::Not:
        matched = !evaluate(conditions_[index].first, transition, depth + 1);
        break;
    }
    conditionCache_[index] = matched ? 2 : 1;
    return matched;
}
void Engine::select(std::uint8_t target, std::uint32_t now, std::uint8_t transition) {
    const TransitionId edge =
        transition == Missing ? TransitionId() : profile_->transitions.get(transition)->id;
    if (!entryPending_)
        dispatch(states_[state_].exit, ActionPhase::Exit, now, edge);
    if (transition != Missing)
        dispatch(transitions_[transition].actions, ActionPhase::Transition, now, edge);
    state_ = target;
    ++revision_;
    entryPending_ = true; // One state's work per tick; target entry runs next tick.
    lastTransition_ = edge;
    resetTimers();
}
Result Engine::tick(std::uint32_t now) {
    if (operating_)
        return result(Outcome::Busy);
    if (state_ == Missing)
        return result(Outcome::NotRunning);
    OperationGuard guard(operating_);
    tickResult_ = {};
    if (paused_) {
        lastNow_ = now;
        return tickResult_;
    }
    tickResult_ = advanceClock(now);
    if (!tickResult_)
        return tickResult_;
    if (entryPending_) {
        entryPending_ = false;
        stateAge_ = 0;
        dispatch(states_[state_].entry, ActionPhase::Entry, now, lastTransition_);
    }
    dispatch(states_[state_].tick, ActionPhase::Tick, now);
    sampleStatus_.fill(0);
    for (std::size_t order = 0; order < profile_->transitions.size(); ++order) {
        const std::uint8_t index = order_[order];
        if (transitions_[index].source != state_)
            continue;
        conditionCache_.fill(0);
        const bool matched = evaluate(transitions_[index].condition, index);
        if (!tickResult_)
            return tickResult_;
        if (matched) {
            if (revision_ == std::numeric_limits<std::uint32_t>::max()) {
                tickResult_.outcome = Outcome::RevisionExhausted;
                return tickResult_;
            }
            select(transitions_[index].target, now, index);
            break;
        }
    }
    return tickResult_;
}
Result Engine::requestCheck(std::uint32_t expectedRevision) const {
    if (operating_)
        return result(Outcome::Busy);
    if (state_ == Missing)
        return result(Outcome::NotRunning);
    if (expectedRevision != revision_)
        return result(Outcome::StaleRevision);
    if (revision_ == std::numeric_limits<std::uint32_t>::max())
        return result(Outcome::RevisionExhausted);
    return {};
}
Result Engine::stop(std::uint32_t expectedRevision, std::uint32_t now) {
    const Result check = requestCheck(expectedRevision);
    if (!check)
        return check;
    OperationGuard guard(operating_);
    if (!entryPending_)
        dispatch(states_[state_].exit, ActionPhase::Exit, now);
    state_ = Missing;
    profile_ = nullptr;
    ++revision_;
    paused_ = entryPending_ = false;
    resetTimers();
    return {};
}
Result Engine::pause(std::uint32_t expectedRevision, std::uint32_t now) {
    const Result check = requestCheck(expectedRevision);
    if (!check)
        return check;
    if (paused_)
        return {};
    const Result clock = advanceClock(now);
    if (!clock)
        return clock;
    paused_ = true;
    ++revision_;
    return {};
}
Result Engine::resume(std::uint32_t expectedRevision, std::uint32_t now) {
    const Result check = requestCheck(expectedRevision);
    if (!check)
        return check;
    if (!paused_)
        return {};
    paused_ = false;
    lastNow_ = now;
    ++revision_;
    return {};
}
Result Engine::changeState(StateId target, std::uint32_t expectedRevision, std::uint32_t now) {
    const Result check = requestCheck(expectedRevision);
    if (!check)
        return check;
    const std::uint8_t index = indexOf(profile_->states, target);
    if (index == Missing)
        return result(Outcome::UnknownState);
    if (!profile_->states.get(index)->manualAllowed)
        return result(Outcome::ManualDenied);
    const Result clock = advanceClock(now);
    if (!clock)
        return clock;
    OperationGuard guard(operating_);
    select(index, now);
    return {};
}
Result Engine::takeTransition(TransitionId transition, std::uint32_t expectedRevision,
                              std::uint32_t now) {
    const Result check = requestCheck(expectedRevision);
    if (!check)
        return check;
    const std::uint8_t index = indexOf(profile_->transitions, transition);
    if (index == Missing)
        return result(Outcome::UnknownTransition);
    if (transitions_[index].source != state_ || !profile_->transitions.get(index)->manualAllowed ||
        !profile_->states.get(transitions_[index].target)->manualAllowed)
        return result(Outcome::ManualDenied);
    const Result clock = advanceClock(now);
    if (!clock)
        return clock;
    OperationGuard guard(operating_);
    select(transitions_[index].target, now, index);
    return {};
}
ExecutionSnapshot Engine::snapshot() const {
    ExecutionSnapshot value;
    value.running = state_ != Missing;
    value.paused = paused_;
    value.entryPending = entryPending_;
    value.state = value.running ? profile_->states.get(state_)->id : StateId();
    value.lastTransition = lastTransition_;
    value.profileRevision = profileRevision_;
    value.stateRevision = revision_;
    value.stateElapsedMilliseconds = stateAge_;
    value.actionRequests = requests_;
    value.queuedActions = queued_;
    value.rejectedActions = rejected_;
    value.unavailableActions = unavailable_;
    value.lastFailedAction = lastFailedAction_;
    value.lastActionFailure = lastFailure_;
    value.lastTickResult = tickResult_;
    return value;
}
} // namespace execution
} // namespace larp
