#include "DeviceRuntime/ProfileApplication.h"
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
DraftResult deniedDraft(DraftOutcome outcome = DraftOutcome::Busy) {
    DraftResult result;
    result.outcome = outcome;
    return result;
}
ApplicationResult busyResult() {
    ApplicationResult result;
    result.outcome = ApplicationOutcome::Busy;
    result.controller = deniedDraft();
    return result;
}
execution::Result deniedGame() {
    execution::Result result;
    result.outcome = execution::Outcome::Busy;
    return result;
}
} // namespace
const char *applicationPhaseName(ApplicationPhase value) {
    static const char *const names[] = {
        "empty", "pending_activation", "applying", "saving", "ready", "fault"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_application_phase";
}
const char *applicationOutcomeName(ApplicationOutcome value) {
    static const char *const names[] = {
        "queued", "ready", "busy", "not_ready", "controller_rejected", "storage_rejected"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_application_outcome";
}
bool ProfileApplication::busy() const {
    return operating_ || controller_.busy() ||
           state_.phase == ApplicationPhase::PendingActivation ||
           state_.phase == ApplicationPhase::Applying || state_.phase == ApplicationPhase::Saving;
}
ApplicationResult ProfileApplication::result(ApplicationOutcome outcome,
                                             const DraftResult &controller) {
    state_.outcome = outcome;
    state_.controller = controller.outcome;
    ApplicationResult value;
    value.outcome = outcome;
    value.controller = controller;
    value.journal = state_.journal;
    return value;
}
ApplicationSnapshot ProfileApplication::snapshot() const {
    auto value = state_;
    value.storedMatchesActive =
        state_.hasStoredProfile && controller_.storage().hasActive &&
        state_.storedConfigurationRevision == controller_.activation().configurationRevision;
    return value;
}
void ProfileApplication::prepareStored(bool forceSave) {
    const auto value = journal_.snapshot();
    loadedGeneration_ = value.generation;
    loadedProfileRevision_ = controller_.storage().draftProfileRevision;
    // Reading after a failed synchronization is not confirmation. Journal keeps
    // its protected old slot; reactivation must be saved again without replacing it.
    needsSave_ = forceSave || value.writeUncertain || value.selectedUnconfirmed;
    state_.phase = ApplicationPhase::PendingActivation;
}
ApplicationResult ProfileApplication::begin(BootPolicy policy, FactoryProfile factory,
                                            configuration::IProfileReader *initial) {
    if (operating_ || initialized_ || state_.phase != ApplicationPhase::Empty)
        return busyResult();
    Guard guard(operating_);
    state_.operation = ApplicationOperation::Boot;
    if (!controller_.beginStorage()) {
        state_.phase = ApplicationPhase::Fault;
        return result(ApplicationOutcome::ControllerRejected,
                      deniedDraft(DraftOutcome::Uninitialized));
    }
    initialized_ = true;
    const auto restored = journal_.restoreDraft(0);
    state_.journal = restored.outcome;
    if (restored.outcome == JournalOutcome::Restored) {
        prepareStored();
        bootFallback_ = true;
        return result(ApplicationOutcome::Queued, restored.controller);
    }
    if (restored.outcome != JournalOutcome::Empty || policy != BootPolicy::InitializeEmpty) {
        state_.phase = ApplicationPhase::Fault;
        return result(ApplicationOutcome::StorageRejected, restored.controller);
    }
    const auto created =
        initial ? controller_.importDraft(*initial, 0) : controller_.createDraft(factory, 0);
    if (!created) {
        state_.phase = ApplicationPhase::Fault;
        return result(ApplicationOutcome::ControllerRejected, created);
    }
    needsSave_ = true;
    state_.phase = ApplicationPhase::PendingActivation;
    return result(ApplicationOutcome::Queued, created);
}
DraftResult ProfileApplication::createDraft(FactoryProfile factory, std::uint32_t expected) {
    if (busy())
        return deniedDraft();
    Guard guard(operating_);
    return controller_.createDraft(factory, expected);
}
DraftResult ProfileApplication::importDraft(configuration::IProfileReader &reader,
                                            std::uint32_t expected) {
    if (busy())
        return deniedDraft();
    Guard guard(operating_);
    return controller_.importDraft(reader, expected);
}
DraftResult ProfileApplication::writeDraft(configuration::IProfileWriter &writer,
                                           std::uint32_t expected) {
    if (busy())
        return deniedDraft();
    Guard guard(operating_);
    return controller_.writeDraft(writer, expected);
}
DraftResult ProfileApplication::writeActive(configuration::IProfileWriter &writer,
                                            std::uint32_t expected) {
    if (busy())
        return deniedDraft();
    Guard guard(operating_);
    return controller_.writeActive(writer, expected);
}
ApplicationResult ProfileApplication::activateDraft(std::uint32_t draft,
                                                    std::uint32_t configuration,
                                                    std::uint32_t now) {
    if (busy())
        return busyResult();
    Guard guard(operating_);
    const auto requested = controller_.activateDraft(draft, configuration, now);
    if (!requested)
        return result(ApplicationOutcome::ControllerRejected, requested);
    state_.operation = ApplicationOperation::Apply;
    state_.phase = ApplicationPhase::Applying;
    state_.fallbackAttempted = false;
    needsSave_ = true;
    bootFallback_ = false;
    return result(ApplicationOutcome::Queued, requested);
}
ApplicationResult ProfileApplication::restoreStored(std::uint32_t draft,
                                                    std::uint32_t configuration, bool older) {
    if (busy())
        return busyResult();
    Guard guard(operating_);
    if (controller_.gameRunning())
        return result(ApplicationOutcome::ControllerRejected,
                      deniedDraft(DraftOutcome::GameRunning));
    if (configuration != controller_.activation().configurationRevision)
        return result(ApplicationOutcome::ControllerRejected,
                      deniedDraft(DraftOutcome::StaleRevision));
    if (draft != controller_.storage().draftRevision)
        return result(ApplicationOutcome::ControllerRejected,
                      deniedDraft(DraftOutcome::StaleRevision));
    const auto restored = journal_.restoreDraft(draft, older);
    state_.journal = restored.outcome;
    if (!restored) {
        state_.phase = ApplicationPhase::Fault;
        return result(ApplicationOutcome::StorageRejected, restored.controller);
    }
    state_.operation = ApplicationOperation::Restore;
    state_.fallbackAttempted = false;
    bootFallback_ = false;
    prepareStored(older);
    return result(ApplicationOutcome::Queued, restored.controller);
}
void ProfileApplication::storedReady() {
    state_.storedGeneration = loadedGeneration_;
    state_.storedProfileRevision = loadedProfileRevision_;
    state_.storedConfigurationRevision = controller_.activation().configurationRevision;
    state_.hasStoredProfile = true;
    state_.phase = ApplicationPhase::Ready;
    state_.outcome = ApplicationOutcome::Ready;
}
void ProfileApplication::service(std::uint32_t now) {
    if (operating_ || !initialized_)
        return;
    Guard guard(operating_);
    if (state_.phase == ApplicationPhase::PendingActivation) {
        const auto requested =
            controller_.activateDraft(controller_.storage().draftRevision,
                                      controller_.activation().configurationRevision, now);
        state_.controller = requested.outcome;
        if (!requested) {
            state_.phase = ApplicationPhase::Fault;
            state_.outcome = ApplicationOutcome::ControllerRejected;
        } else
            state_.phase = ApplicationPhase::Applying;
        return;
    }
    controller_.service(now);
    if (state_.phase == ApplicationPhase::Applying && !controller_.busy()) {
        const auto activation = controller_.activation();
        if (activation.finish == ActivationFinish::Applied) {
            if (needsSave_)
                state_.phase = ApplicationPhase::Saving;
            else
                storedReady();
            return;
        }
        if (activation.finish == ActivationFinish::Failed && bootFallback_ &&
            !state_.fallbackAttempted) {
            state_.fallbackAttempted = true;
            const auto older = journal_.restoreDraft(controller_.storage().draftRevision, true);
            state_.journal = older.outcome;
            state_.controller = older.controller.outcome;
            if (older) {
                prepareStored(true);
                return;
            }
        }
        state_.outcome = ApplicationOutcome::ControllerRejected;
        // Explicit game-master restart remains possible after confirmed rollback
        // of the prior stored RAM profile. No automatic start on failure.
        const bool usable = activation.finish == ActivationFinish::RolledBack &&
                            activation.hardwareReady && snapshot().storedMatchesActive;
        state_.phase = usable ? ApplicationPhase::Ready : ApplicationPhase::Fault;
        return;
    }
    if (state_.phase == ApplicationPhase::Saving) {
        const auto saved = journal_.saveActive(controller_.activation().configurationRevision);
        state_.journal = saved.outcome;
        state_.controller = saved.controller.outcome;
        if (!saved) {
            state_.phase = ApplicationPhase::Fault;
            state_.outcome = ApplicationOutcome::StorageRejected;
            return;
        }
        loadedGeneration_ = journal_.snapshot().generation;
        loadedProfileRevision_ = controller_.storage().activeProfileRevision;
        needsSave_ = false;
        storedReady();
        return;
    }
    if (state_.phase == ApplicationPhase::Ready && controller_.gameRunning())
        controller_.tickGame(now);
}
bool ProfileApplication::gameAllowed() const {
    return !operating_ && state_.phase == ApplicationPhase::Ready &&
           snapshot().storedMatchesActive && controller_.activation().hardwareReady;
}
execution::Result ProfileApplication::startGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.startGame(revision, now);
}
execution::Result ProfileApplication::stopGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.stopGame(revision, now);
}
execution::Result ProfileApplication::pauseGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.pauseGame(revision, now);
}
execution::Result ProfileApplication::resumeGame(std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.resumeGame(revision, now);
}
execution::Result ProfileApplication::changeState(configuration::StateId target,
                                                  std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.changeState(target, revision, now);
}
execution::Result ProfileApplication::takeTransition(configuration::TransitionId edge,
                                                     std::uint32_t revision, std::uint32_t now) {
    if (!gameAllowed())
        return deniedGame();
    Guard guard(operating_);
    return controller_.takeTransition(edge, revision, now);
}
} // namespace devices
} // namespace larp
