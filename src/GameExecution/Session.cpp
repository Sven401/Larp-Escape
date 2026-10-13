#include "GameExecution/Session.h"
#include <limits>

namespace larp {
namespace execution {
using namespace configuration;
namespace {
struct Guard {
    explicit Guard(bool &busy) : busy(busy) {
        busy = true;
    }
    ~Guard() {
        busy = false;
    }
    bool &busy;
};
Result failure(Outcome outcome) {
    Result value;
    value.outcome = outcome;
    return value;
}
bool accepted(ActionOutcome outcome) {
    return outcome == ActionOutcome::Applied || outcome == ActionOutcome::Queued;
}
ActionOutcome combine(ActionOutcome first, ActionOutcome second) {
    if (first == ActionOutcome::Unavailable || second == ActionOutcome::Unavailable)
        return ActionOutcome::Unavailable;
    if (first == ActionOutcome::Rejected || second == ActionOutcome::Rejected)
        return ActionOutcome::Rejected;
    if (first == ActionOutcome::Queued || second == ActionOutcome::Queued)
        return ActionOutcome::Queued;
    if (first == ActionOutcome::Applied || second == ActionOutcome::Applied)
        return ActionOutcome::Applied;
    return ActionOutcome::Deferred;
}
} // namespace
Session::Session(IProfileDevices &devices) : devices_(devices), engine_(*this, *this) {}
std::uint8_t Session::componentIndex(ComponentId id) const {
    for (std::uint8_t index = 0; index < componentCount_; ++index)
        if (deviceStates_[index].status.id == id)
            return index;
    return Missing;
}
std::uint8_t Session::actionIndex(ActionId id) const {
    for (std::size_t index = 0; index < profile_->actions.size(); ++index)
        if (profile_->actions.get(index)->id == id)
            return static_cast<std::uint8_t>(index);
    return Missing;
}
Result Session::start(const Profile &profile, std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    const Result result = engine_.start(profile, now);
    if (!result)
        return result;
    profile_ = &profile;
    componentCount_ = static_cast<std::uint8_t>(profile.components.size());
    for (std::size_t index = 0; index < componentCount_; ++index) {
        deviceStates_[index] = {};
        deviceStates_[index].status.id = profile.components.get(index)->id;
        deviceStates_[index].status.enabled = profile.components.get(index)->enabled;
    }
    clocks_.fill({});
    roleIndex_ = Missing;
    roleId_ = RoleId();
    requiredCrystals_ = 0;
    lastUid_ = {};
    lastDecision_ = domain::KeystoneTransition::EmptyRead;
    resetGame();
    return result;
}
Result Session::tick(std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    if (profile_ == nullptr)
        return failure(Outcome::NotRunning);
    Guard guard(operating_);
    for (PollClock &clock : clocks_)
        clock.checked = false;
    for (std::size_t index = 0; index < componentCount_; ++index) {
        DeviceState &device = deviceStates_[index];
        device.status.roleAccepted = false; // ReadRole is a one-tick observation.
        DeviceSample value;
        const Component &component = *profile_->components.get(index);
        if (!component.enabled || !devices_.sample(component, value))
            value.available = false;
        device.sample = value;
        device.status.available = value.available;
        device.status.pollPending = false;
    }
    return engine_.tick(now);
}
Result Session::stop(std::uint32_t expectedRevision, std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    const Result result = engine_.stop(expectedRevision, now);
    if (result) {
        profile_ = nullptr;
        for (PollClock &clock : clocks_)
            clock.pending = false;
        for (std::size_t index = 0; index < componentCount_; ++index)
            deviceStates_[index].status.pollPending = false;
    }
    return result;
}
Result Session::pause(std::uint32_t expectedRevision, std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    return engine_.pause(expectedRevision, now);
}
Result Session::resume(std::uint32_t expectedRevision, std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    return engine_.resume(expectedRevision, now);
}
Result Session::changeState(StateId target, std::uint32_t expectedRevision, std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    return engine_.changeState(target, expectedRevision, now);
}
Result Session::takeTransition(TransitionId transition, std::uint32_t expectedRevision,
                               std::uint32_t now) {
    if (operating_)
        return failure(Outcome::Busy);
    Guard guard(operating_);
    return engine_.takeTransition(transition, expectedRevision, now);
}
void Session::resetRound() {
    crystalCount_ = 0;
    for (std::size_t index = 0; index < componentCount_; ++index) {
        DeviceState &device = deviceStates_[index];
        device.retry = device.misses = 0;
        device.status.crystalAvailable = device.status.incorrectCrystal = false;
    }
}
void Session::resetGame() {
    resetRound();
    rounds_ = 0;
    currentUid_ = {};
    seen_.fill(false);
    seenCount_ = 0;
    for (std::size_t index = 0; index < componentCount_; ++index)
        deviceStates_[index].status.keystoneError = false;
    // Match legacy reset: recognized role metadata and last UID survive.
    // A new profile/start separately clears both to prevent cross-profile identity.
}
bool Session::pendingAudio() const {
    for (std::size_t index = 0; index < crystalCount_; ++index)
        if (!crystals_[index].audioRequested)
            return true;
    return false;
}
bool Session::read(Field field, ComponentId component, std::uint32_t &value) noexcept {
    const std::uint8_t index = componentIndex(component);
    if (component.value != 0 && (index == Missing || !deviceStates_[index].status.available))
        return false;
    switch (field) {
    case Field::AudioBusy:
        value = deviceStates_[index].sample.audioBusy ? 1 : 0;
        break;
    case Field::RoleAccepted:
        if (!deviceStates_[index].status.cardReadable)
            return false;
        value = deviceStates_[index].status.roleAccepted ? 1 : 0;
        break;
    case Field::CrystalAvailable:
    case Field::IncorrectCrystal:
        if (!deviceStates_[index].status.matrixReadable)
            return false;
        value = (field == Field::CrystalAvailable ? deviceStates_[index].status.crystalAvailable
                                                  : deviceStates_[index].status.incorrectCrystal)
                    ? 1
                    : 0;
        break;
    case Field::KeystoneError:
        if (!deviceStates_[index].status.cardReadable)
            return false;
        value = deviceStates_[index].status.keystoneError ? 1 : 0;
        break;
    case Field::AllWakeActive: {
        value = 1;
        const Component &matrix = *profile_->components.get(index);
        for (std::size_t wake = 0; wake < matrix.wakeInputs.size(); ++wake)
            if (!deviceStates_[index].sample.wakes[wake])
                value = 0;
        break;
    }
    case Field::CrystalAudioPending:
        value = pendingAudio() ? 1 : 0;
        break;
    case Field::PuzzleComplete:
        value = roleIndex_ != Missing && crystalCount_ == requiredCrystals_ ? 1 : 0;
        break;
    case Field::SeenRoleCount:
        value = seenCount_;
        break;
    case Field::RoundCount:
        value = rounds_;
        break;
    default:
        return false; // Time fields belong to Engine.
    }
    return true;
}
bool Session::beginPoll(std::uint8_t index, const Action &action, const ActionContext &context,
                        PollRequest &request) {
    PollClock &clock = clocks_[index];
    if (clock.checked)
        return false;
    clock.checked = true;
    const std::uint32_t revision = engine_.snapshot().stateRevision;
    if (clock.pending && clock.pendingRevision != revision)
        clock.pending = false;
    if (!clock.pending) {
        if (!clock.started) {
            clock.started = true;
            clock.last = context.nowMilliseconds;
            return false;
        }
        if (static_cast<std::uint32_t>(context.nowMilliseconds - clock.last) < action.value)
            return false;
        clock.last = context.nowMilliseconds;
    }
    request.component = action.component;
    request.requester = action.id;
    request.profileRevision = profile_->revision;
    request.stateRevision = revision;
    request.nowMilliseconds = context.nowMilliseconds;
    request.timeoutMilliseconds = profile_->inputs.cardTimeoutMilliseconds;
    return true;
}
ActionOutcome Session::pollResult(std::uint8_t action, std::uint8_t component, PollOutcome outcome,
                                  std::uint32_t revision) {
    PollClock &clock = clocks_[action];
    clock.pending = outcome == PollOutcome::Pending || outcome == PollOutcome::Queued;
    clock.pendingRevision = revision;
    deviceStates_[component].status.pollPending =
        deviceStates_[component].status.pollPending || clock.pending;
    const ActionOutcome result = outcome == PollOutcome::Fresh      ? ActionOutcome::Applied
                                 : outcome == PollOutcome::Queued   ? ActionOutcome::Queued
                                 : outcome == PollOutcome::Pending  ? ActionOutcome::Deferred
                                 : outcome == PollOutcome::Rejected ? ActionOutcome::Rejected
                                                                    : ActionOutcome::Unavailable;
    return remember(deviceStates_[component].status.id, result);
}
ActionOutcome Session::remember(ComponentId component, ActionOutcome result) {
    const std::uint8_t index = componentIndex(component);
    if (index != Missing)
        deviceStates_[index].status.lastOutcome = result;
    return result;
}
ActionOutcome Session::readRole(const Action &action, const ActionContext &context) {
    const std::uint8_t slot = componentIndex(action.component), poll = actionIndex(action.id);
    PollRequest request;
    if (!beginPoll(poll, action, context, request))
        return ActionOutcome::Deferred;
    domain::CardUid candidate = {};
    const PollOutcome outcome = devices_.pollCard(request, candidate);
    const ActionOutcome result = pollResult(poll, slot, outcome, request.stateRevision);
    if (outcome == PollOutcome::Unavailable)
        deviceStates_[slot].status.cardReadable = false;
    if (outcome != PollOutcome::Fresh)
        return result;
    DeviceState &device = deviceStates_[slot];
    device.status.cardReadable = true;
    bool known = false;
    if (candidate != domain::CardUid{} && lastUid_ == domain::CardUid{}) {
        std::uint8_t found = Missing;
        for (std::size_t index = 0; index < profile_->roles.size(); ++index)
            if (profile_->roles.get(index)->uid == candidate) {
                found = static_cast<std::uint8_t>(index);
                break;
            }
        if (found != roleIndex_) {
            // Recorded indices belong to the recognized role. Do not reset NFC
            // retry/miss counters here: a nonempty read preserves legacy retries.
            crystalCount_ = 0;
            for (std::size_t index = 0; index < componentCount_; ++index) {
                deviceStates_[index].status.crystalAvailable = false;
                deviceStates_[index].status.incorrectCrystal = false;
            }
        }
        roleIndex_ = found;
        roleId_ = found == Missing ? RoleId() : profile_->roles.get(found)->id;
        requiredCrystals_ =
            found == Missing
                ? 0
                : static_cast<std::uint8_t>(profile_->roles.get(found)->crystals.size());
        known = found != Missing;
    }
    lastDecision_ = domain::classifyKeystoneTransition(candidate, lastUid_, known);
    if (lastDecision_ == domain::KeystoneTransition::EmptyRead) {
        ++device.retry;
        if (device.retry >= profile_->inputs.emptyReadsBeforeClear) {
            lastUid_ = currentUid_;
            currentUid_ = candidate;
            device.retry = 0;
        }
        return result;
    }
    if (profile_->inputs.consecutiveEmptyReads)
        device.retry = 0;
    if (lastDecision_ == domain::KeystoneTransition::RepeatedPreviousKeystone)
        return result;
    lastUid_ = currentUid_;
    currentUid_ = candidate;
    device.status.roleAccepted = lastDecision_ == domain::KeystoneTransition::RoleAccepted;
    return result;
}
ActionOutcome Session::applyCrystalPalette(const Action &action, const Crystal &crystal,
                                           const ActionContext &context) {
    if (!crystal.changesPalette)
        return ActionOutcome::Applied;
    Effect effect;
    effect.kind = EffectKind::Palette;
    if (action.effect.value != 0) {
        for (std::size_t index = 0; index < profile_->effects.size(); ++index)
            if (profile_->effects.get(index)->id == action.effect) {
                effect = *profile_->effects.get(index);
                break;
            }
    } else {
        for (std::size_t index = 0; index < profile_->components.size(); ++index) {
            const Component &component = *profile_->components.get(index);
            if (component.enabled && component.kind == ComponentKind::LedStrip) {
                effect.component = component.id;
                break;
            }
        }
    }
    effect.palette = crystal.palette;
    return remember(effect.component, devices_.applyEffect(effect, action.id, context));
}
ActionOutcome Session::readCrystals(const Action &action, const ActionContext &context) {
    const std::uint8_t slot = componentIndex(action.component), poll = actionIndex(action.id);
    PollRequest request;
    if (!beginPoll(poll, action, context, request))
        return ActionOutcome::Deferred;
    ports::CrystalInputBatch batch;
    const PollOutcome outcome = devices_.pollCrystals(request, batch);
    const ActionOutcome result = pollResult(poll, slot, outcome, request.stateRevision);
    if (outcome == PollOutcome::Unavailable)
        deviceStates_[slot].status.matrixReadable = false;
    if (outcome != PollOutcome::Fresh)
        return result;
    DeviceState &device = deviceStates_[slot];
    device.status.matrixReadable = true;
    if (roleIndex_ == Missing)
        return result;
    const Role &role = *profile_->roles.get(roleIndex_);
    for (const domain::GridPosition &position : batch) {
        std::uint8_t found = Missing;
        for (std::size_t index = 0; index < role.crystals.size(); ++index) {
            const Crystal &crystal = *role.crystals.get(index);
            if (crystal.row == position.row &&
                crystal.column == static_cast<std::uint8_t>(position.column)) {
                found = static_cast<std::uint8_t>(index);
                break;
            }
        }
        if (found == Missing) {
            device.status.crystalAvailable = false;
            device.status.incorrectCrystal = true;
            return result;
        }
        bool duplicate = false;
        for (std::size_t index = 0; index < crystalCount_; ++index)
            if (crystals_[index].definition == found)
                duplicate = true;
        if (duplicate || crystalCount_ == requiredCrystals_)
            continue;
        crystals_[crystalCount_].definition = found;
        crystals_[crystalCount_++].audioRequested = false;
        device.status.crystalAvailable = true;
        device.status.incorrectCrystal = false;
        return combine(result, applyCrystalPalette(action, *role.crystals.get(found), context));
    }
    return result;
}
ActionOutcome Session::observeKeystone(const Action &action, const ActionContext &context) {
    const std::uint8_t slot = componentIndex(action.component), poll = actionIndex(action.id);
    PollRequest request;
    if (!beginPoll(poll, action, context, request))
        return ActionOutcome::Deferred;
    domain::CardUid candidate = {};
    const PollOutcome outcome = devices_.pollCard(request, candidate);
    const ActionOutcome result = pollResult(poll, slot, outcome, request.stateRevision);
    if (outcome == PollOutcome::Unavailable)
        deviceStates_[slot].status.cardReadable = false;
    if (outcome != PollOutcome::Fresh)
        return result;
    DeviceState &device = deviceStates_[slot];
    device.status.cardReadable = true;
    if (candidate != domain::CardUid{} && candidate == currentUid_) {
        device.misses = 0;
        device.status.keystoneError = false;
    } else if (++device.misses >= profile_->inputs.keystoneFailureThreshold) {
        device.misses = 0;
        device.status.keystoneError = true;
    }
    return result;
}
ActionOutcome Session::execute(const Profile &profile, const Action &action,
                               const ActionContext &context) noexcept {
    if (&profile != profile_ || profile_ == nullptr)
        return ActionOutcome::Unavailable;
    switch (action.kind) {
    case ActionKind::ApplyEffect:
        for (std::size_t index = 0; index < profile.effects.size(); ++index) {
            const Effect &effect = *profile.effects.get(index);
            if (effect.id == action.effect)
                return remember(effect.component, devices_.applyEffect(effect, action.id, context));
        }
        return ActionOutcome::Unavailable;
    case ActionKind::PlayTrack:
    case ActionKind::PlayRoleTrack:
    case ActionKind::PlayCrystalTrack: {
        std::uint16_t track = static_cast<std::uint16_t>(action.value);
        if (action.kind != ActionKind::PlayTrack) {
            if (roleIndex_ == Missing)
                return ActionOutcome::Unavailable;
            const Role &role = *profile.roles.get(roleIndex_);
            if (action.kind == ActionKind::PlayRoleTrack)
                track = role.audioTrack;
            else {
                if (action.value >= crystalCount_)
                    return ActionOutcome::Unavailable;
                RecordedCrystal &record = crystals_[action.value];
                track = role.crystals.get(record.definition)->audioTrack;
                // Legacy consumes an audio attempt even when the device rejects it.
                // This flag is not a claim that the track physically played.
                record.audioRequested = true;
            }
        }
        const ActionOutcome result =
            remember(action.component, devices_.playTrack(action.component, track, context));
        if (accepted(result))
            deviceStates_[componentIndex(action.component)].sample.audioBusy = true;
        return result;
    }
    case ActionKind::WakeAudio:
        return remember(action.component, devices_.wakeAudio(action.component, context));
    case ActionKind::SendActivation:
        return remember(action.component, devices_.sendActivation(action.component, context));
    case ActionKind::ReadRole:
        return readRole(action, context);
    case ActionKind::ReadCrystals:
        return readCrystals(action, context);
    case ActionKind::ObserveKeystone:
        return observeKeystone(action, context);
    case ActionKind::ResetRound:
        resetRound();
        break;
    case ActionKind::ResetGame:
        resetGame();
        break;
    case ActionKind::ResetCardRetry:
        for (std::size_t index = 0; index < componentCount_; ++index)
            deviceStates_[index].retry = 0;
        break;
    case ActionKind::ConsumeCrystal:
        for (std::size_t index = 0; index < componentCount_; ++index)
            deviceStates_[index].status.crystalAvailable = false;
        break;
    case ActionKind::ConsumeError: {
        bool wrong = false;
        for (std::size_t index = 0; index < componentCount_; ++index)
            wrong = wrong || deviceStates_[index].status.incorrectCrystal;
        for (std::size_t index = 0; index < componentCount_; ++index) {
            if (wrong)
                deviceStates_[index].status.incorrectCrystal = false;
            else
                deviceStates_[index].status.keystoneError = false;
        }
        break;
    }
    case ActionKind::CompleteRound:
        if (roleIndex_ == Missing)
            return ActionOutcome::Unavailable;
        if (!seen_[roleIndex_]) {
            seen_[roleIndex_] = true;
            ++seenCount_;
        }
        if (rounds_ != std::numeric_limits<std::uint32_t>::max())
            ++rounds_;
        lastUid_ = currentUid_;
        break;
    case ActionKind::AnimateWakeInputs: {
        const std::uint8_t source = componentIndex(action.source);
        if (!deviceStates_[source].status.available)
            return ActionOutcome::Unavailable;
        ActionOutcome result = ActionOutcome::Deferred;
        const Component &matrix = *profile.components.get(source);
        for (std::size_t index = 0; index < matrix.wakeInputs.size(); ++index) {
            if (!deviceStates_[source].sample.wakes[index])
                continue;
            Effect effect;
            effect.component = action.component;
            effect.kind = EffectKind::Ripple;
            effect.frames = profile.inputs.wakeRippleFrames;
            effect.rippleIndex = static_cast<std::uint8_t>(index);
            effect.origin = profile.inputs.wakeRippleOrigins[index];
            result = combine(result, devices_.applyEffect(effect, action.id, context));
        }
        return remember(action.component, result);
    }
    }
    return ActionOutcome::Applied;
}
SessionSnapshot Session::snapshot() const {
    SessionSnapshot value;
    value.execution = engine_.snapshot();
    value.rolePresent = roleIndex_ != Missing;
    value.role = roleId_;
    value.crystalCount = crystalCount_;
    value.requiredCrystals = requiredCrystals_;
    value.seenRoleCount = seenCount_;
    value.roundCount = rounds_;
    value.crystalAudioPending = pendingAudio();
    value.lastCardDecision = lastDecision_;
    value.deviceCount = componentCount_;
    for (std::size_t index = 0; index < componentCount_; ++index) {
        value.devices[index] = deviceStates_[index].status;
        value.incorrectCrystal =
            value.incorrectCrystal || deviceStates_[index].status.incorrectCrystal;
        value.keystoneError = value.keystoneError || deviceStates_[index].status.keystoneError;
    }
    return value;
}
} // namespace execution
} // namespace larp
