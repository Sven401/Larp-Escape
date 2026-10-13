#include "CompanionIntegration/GameControl.h"
#include <limits>
namespace larp {
namespace integration {
namespace {
void increment(std::uint32_t &value) {
    if (value != std::numeric_limits<std::uint32_t>::max())
        ++value;
}
bool same(const companion::GameCommand &a, const companion::GameCommand &b) {
    return a.boot == b.boot && a.lease == b.lease && a.request == b.request &&
           a.expectedConfigurationRevision == b.expectedConfigurationRevision &&
           a.expectedStateRevision == b.expectedStateRevision && a.kind == b.kind &&
           a.target == b.target;
}
companion::ConfiguredPhase phase(devices::ApplicationPhase value) {
    using companion::ConfiguredPhase;
    using devices::ApplicationPhase;
    switch (value) {
    case ApplicationPhase::Empty:
        return ConfiguredPhase::Empty;
    case ApplicationPhase::PendingActivation:
        return ConfiguredPhase::PendingActivation;
    case ApplicationPhase::Applying:
        return ConfiguredPhase::Applying;
    case ApplicationPhase::Saving:
        return ConfiguredPhase::Saving;
    case ApplicationPhase::Ready:
        return ConfiguredPhase::Ready;
    case ApplicationPhase::Fault:
        return ConfiguredPhase::Fault;
    }
    return ConfiguredPhase::Fault;
}
} // namespace
bool GameControl::begin(companion::BootIdentity boot) {
    if (initialized_ || operating_ || !boot.valid())
        return false;
    boot_ = boot;
    initialized_ = true;
    return true;
}
bool GameControl::synchronizeAuthority() {
    companion::TrustedAuthority incoming;
    if (!channel_.readAuthority(incoming)) {
        authority_.suspend();
        const bool changed = lease_ != 0;
        revoke();
        return changed;
    }
    const auto outcome = authority_.update(incoming);
    if (outcome == companion::AuthorityUpdate::Duplicate)
        return false;
    const bool changed = lease_ != 0 || outcome == companion::AuthorityUpdate::Published;
    revoke();
    if (outcome == companion::AuthorityUpdate::Published) {
        authorityEpoch_ = incoming.epoch;
        if (incoming.gameMaster && leaseCounter_ != std::numeric_limits<std::uint32_t>::max() &&
            sample_ != std::numeric_limits<std::uint32_t>::max()) {
            principal_ = incoming.principal;
            lease_ = ++leaseCounter_;
        }
    }
    return changed;
}
void GameControl::revoke() {
    // Faulted/no-authority service is constant work after the first invalidation.
    if (!principal_ && !lease_)
        return;
    principal_ = lease_ = 0;
    lastRequest_ = 0;
    receipts_.fill(Receipt());
}
ControlSnapshot GameControl::snapshot() const {
    ControlSnapshot s;
    s.lease = lease_;
    s.authorityEpoch = authorityEpoch_;
    s.nextRequest =
        lease_ && lastRequest_ != std::numeric_limits<std::uint32_t>::max() ? lastRequest_ + 1 : 0;
    s.sample = sample_;
    s.droppedReplies = droppedReplies_;
    s.droppedStatus = droppedStatus_;
    s.initialized = initialized_;
    return s;
}
companion::GameReply GameControl::dispatch(const companion::TrustedGameRequest &request,
                                           std::uint32_t now) {
    using namespace companion;
    const auto &command = request.command;
    GameReply reply;
    reply.boot = boot_;
    reply.lease = command.lease;
    reply.request = command.request;
    reply.nextRequest = snapshot().nextRequest;
    reply.configurationRevision = application_.activation().configurationRevision;
    reply.stateRevision = application_.execution().stateRevision;
    // Authorization precedes cache lookup: another client cannot read receipts.
    if (!principal_ || request.principal != principal_ ||
        request.connectionEpoch != authorityEpoch_ || command.lease != lease_ || !command.lease) {
        reply.outcome = GameReplyOutcome::Unauthorized;
        return reply;
    }
    if (!(command.boot == boot_)) {
        reply.outcome = GameReplyOutcome::WrongBoot;
        return reply;
    }
    if (!command.request) {
        reply.outcome = GameReplyOutcome::InvalidRequest;
        return reply;
    }
    if (command.request <= lastRequest_) {
        const auto &saved = receipts_[(command.request - 1) % ControlLimits::Receipts];
        if (saved.command.request != command.request)
            reply.outcome = GameReplyOutcome::ReceiptExpired;
        else if (!same(saved.command, command))
            reply.outcome = GameReplyOutcome::RequestConflict;
        else {
            reply = saved.reply;
            reply.duplicate = true;
        }
        return reply;
    }
    if (lastRequest_ == std::numeric_limits<std::uint32_t>::max() ||
        command.request != lastRequest_ + 1) {
        reply.outcome = GameReplyOutcome::OutOfOrder;
        return reply;
    }
    const bool targetCommand = command.kind == GameCommandKind::ChangeState ||
                               command.kind == GameCommandKind::TakeTransition;
    if (static_cast<unsigned>(command.kind) >
            static_cast<unsigned>(GameCommandKind::TakeTransition) ||
        (targetCommand ? !command.target : command.target != 0))
        reply.outcome = GameReplyOutcome::InvalidRequest;
    else if (command.expectedConfigurationRevision != reply.configurationRevision)
        reply.outcome = GameReplyOutcome::StaleConfiguration;
    else if (command.expectedStateRevision != reply.stateRevision)
        reply.outcome = GameReplyOutcome::StaleState;
    else {
        const auto application = application_.snapshot();
        if (application.phase != devices::ApplicationPhase::Ready ||
            !application.storedMatchesActive || !application_.activation().hardwareReady ||
            sample_ == std::numeric_limits<std::uint32_t>::max())
            reply.outcome = GameReplyOutcome::NotReady;
        else {
            execution::Result result;
            switch (command.kind) {
            case GameCommandKind::Start:
                result = application_.startGame(command.expectedConfigurationRevision, now);
                break;
            case GameCommandKind::Stop:
                result = application_.stopGame(command.expectedStateRevision, now);
                break;
            case GameCommandKind::Pause:
                result = application_.pauseGame(command.expectedStateRevision, now);
                break;
            case GameCommandKind::Resume:
                result = application_.resumeGame(command.expectedStateRevision, now);
                break;
            case GameCommandKind::ChangeState:
                result = application_.changeState(configuration::StateId(command.target),
                                                  command.expectedStateRevision, now);
                break;
            case GameCommandKind::TakeTransition:
                result = application_.takeTransition(configuration::TransitionId(command.target),
                                                     command.expectedStateRevision, now);
                break;
            }
            reply.outcome =
                result ? GameReplyOutcome::Executed : GameReplyOutcome::ExecutionRejected;
            reply.executionOutcome.assign(execution::outcomeName(result.outcome));
            reply.configurationRevision = application_.activation().configurationRevision;
            reply.stateRevision = application_.execution().stateRevision;
        }
    }
    // Every authorized in-order attempt is consumed, including semantic/stale/
    // readiness errors. Store BEFORE delivery; a full reply queue cannot reexecute.
    lastRequest_ = command.request;
    reply.consumed = true;
    reply.nextRequest = snapshot().nextRequest;
    receipts_[(command.request - 1) % ControlLimits::Receipts] = {command, reply};
    return reply;
}
void GameControl::publish(std::uint32_t now) {
    if (sample_ == std::numeric_limits<std::uint32_t>::max())
        return;
    const auto a = application_.snapshot();
    const auto activation = application_.activation();
    const auto storage = application_.storage();
    const auto game = application_.game();
    const auto &execution = game.execution;
    companion::TrustedAuthority producer;
    status_.principal = status_.connectionEpoch = 0;
    if (authority_.read(producer)) {
        status_.principal = producer.principal;
        status_.connectionEpoch = producer.epoch;
    }
    auto &s = status_.status;
    s.boot = boot_;
    s.sample = ++sample_;
    s.sampledMilliseconds = now;
    s.phase = phase(a.phase);
    s.configurationRevision = activation.configurationRevision;
    s.profileRevision = storage.activeProfileRevision;
    s.draftRevision = storage.draftRevision;
    s.deviceGeneration = activation.generation;
    s.storedGeneration = a.storedGeneration;
    s.storedProfileRevision = a.storedProfileRevision;
    s.hasActive = storage.hasActive;
    s.draftReady = storage.draftReady;
    s.hardwareReady = activation.hardwareReady;
    s.storedMatchesActive = a.storedMatchesActive;
    s.controlLease = lease_;
    s.nextRequest = snapshot().nextRequest;
    s.running = execution.running;
    s.paused = execution.paused;
    s.entryPending = execution.entryPending;
    s.state = execution.state.value;
    s.lastTransition = execution.lastTransition.value;
    s.stateRevision = execution.stateRevision;
    s.stateElapsedMilliseconds = execution.stateElapsedMilliseconds;
    s.rolePresent = game.rolePresent;
    s.role = game.role.value;
    s.crystalCount = game.crystalCount;
    s.requiredCrystals = game.requiredCrystals;
    s.seenRoleCount = game.seenRoleCount;
    s.roundCount = game.roundCount;
    s.audioPending = game.crystalAudioPending;
    s.incorrectCrystal = game.incorrectCrystal;
    s.keystoneError = game.keystoneError;
    s.rejectedActions = execution.rejectedActions;
    s.unavailableActions = execution.unavailableActions;
    s.lastTickOutcome.assign(execution::outcomeName(execution.lastTickResult.outcome));
    s.deviceCount = game.deviceCount;
    for (std::size_t i = 0; i < s.deviceCount && i < s.devices.size(); ++i) {
        const auto &source = game.devices[i];
        auto &target = s.devices[i];
        target.id = source.id.value;
        target.enabled = source.enabled;
        target.available = source.available;
        target.cardReadable = source.cardReadable;
        target.matrixReadable = source.matrixReadable;
        target.pollPending = source.pollPending;
        target.lastOutcome.assign(execution::actionOutcomeName(source.lastOutcome));
    }
    // Do not retain old component metadata outside the current counted range.
    for (std::size_t i = s.deviceCount; i < s.devices.size(); ++i)
        s.devices[i] = companion::ConfiguredDeviceStatus();
    if (!channel_.publish(status_))
        increment(droppedStatus_);
    lastPublished_ = now;
    published_ = true;
}
void GameControl::service(std::uint32_t now) {
    if (!initialized_ || operating_)
        return;
    operating_ = true;
    bool changed = synchronizeAuthority();
    for (std::size_t i = 0; i < companion::ControlLimits::Drain; ++i) {
        companion::TrustedGameRequest request;
        if (!channel_.receive(request))
            break;
        // Authorization point: the last synchronized observation before dispatch.
        // A later revoke does not retroactively cancel this claimed operation.
        changed = synchronizeAuthority() || changed;
        companion::RoutedGameReply reply;
        reply.principal = request.principal;
        reply.connectionEpoch = request.connectionEpoch;
        reply.reply = dispatch(request, now);
        if (!channel_.send(reply))
            increment(droppedReplies_);
        changed = true;
    }
    if (changed || !published_ ||
        static_cast<std::uint32_t>(now - lastPublished_) >=
            companion::ControlLimits::PublishMilliseconds)
        publish(now);
    operating_ = false;
}
} // namespace integration
} // namespace larp
