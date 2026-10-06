#include "DeviceRuntime/ProfileController.h"
#include <limits>
#include <new>
namespace larp {
namespace devices {
namespace {
struct Guard {
    explicit Guard(bool &flag) : flag(flag) {
        flag = true;
    }
    ~Guard() {
        flag = false;
    }
    bool &flag;
};
DraftResult denied(DraftOutcome outcome) {
    DraftResult result;
    result.outcome = outcome;
    return result;
}
execution::Result busyGame() {
    execution::Result result;
    result.outcome = execution::Outcome::Busy;
    return result;
}
} // namespace
const char *draftOutcomeName(DraftOutcome value) {
    static const char *const names[] = {"ready",
                                        "queued",
                                        "busy",
                                        "uninitialized",
                                        "game_running",
                                        "stale_revision",
                                        "invalid_source",
                                        "import_failed",
                                        "invalid_profile",
                                        "counter_exhausted",
                                        "no_draft",
                                        "no_active",
                                        "output_failed",
                                        "activation_rejected"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_draft_outcome";
}
bool ProfileController::beginStorage() {
    if (operating_ || initialized_ || runtime_.busy())
        return false;
    Guard guard(operating_);
    profiles_[0].reset(new (std::nothrow) Profile());
    profiles_[1].reset(new (std::nothrow) Profile());
    if (!profiles_[0] || !profiles_[1]) {
        profiles_[0].reset();
        profiles_[1].reset();
        return false;
    }
    initialized_ = true;
    return true;
}
DraftOutcome ProfileController::admission() const {
    if (operating_ || applying_ || runtime_.busy())
        return DraftOutcome::Busy;
    if (!initialized_)
        return DraftOutcome::Uninitialized;
    if (runtime_.gameRunning())
        return DraftOutcome::GameRunning;
    return DraftOutcome::Ready;
}
DraftOutcome ProfileController::editAdmission(std::uint32_t expectedRevision) const {
    const auto check = admission();
    if (check != DraftOutcome::Ready)
        return check;
    if (expectedRevision != draftRevision_)
        return DraftOutcome::StaleRevision;
    return draftRevision_ == std::numeric_limits<std::uint32_t>::max()
               ? DraftOutcome::CounterExhausted
               : DraftOutcome::Ready;
}
DraftResult ProfileController::createDraft(FactoryProfile source, std::uint32_t expectedRevision) {
    const auto check = editAdmission(expectedRevision);
    if (check != DraftOutcome::Ready)
        return denied(check);
    if (source != FactoryProfile::Standard && source != FactoryProfile::Hint)
        return denied(DraftOutcome::InvalidSource);
    Guard guard(operating_);
    ++draftRevision_;
    draftReady_ = false;
    lastImport_ = configuration::ImportResult();
    DraftResult result;
    const bool created = source == FactoryProfile::Standard
                             ? configuration::makeStandardProfile(*profiles_[draft_])
                             : configuration::makeHintExampleProfile(*profiles_[draft_]);
    result.validation = configuration::validateProfile(*profiles_[draft_]);
    draftReady_ = created && static_cast<bool>(result.validation);
    if (!draftReady_)
        result.outcome = DraftOutcome::InvalidProfile;
    return result;
}
DraftResult ProfileController::importDraft(configuration::IProfileReader &reader,
                                           std::uint32_t expectedRevision) {
    const auto check = editAdmission(expectedRevision);
    if (check != DraftOutcome::Ready)
        return denied(check);
    Guard guard(operating_);
    ++draftRevision_;
    draftReady_ = false;
    DraftResult result;
    result.import = configuration::readDraftProfile(reader, *profiles_[draft_]);
    lastImport_ = result.import;
    result.validation = result.import.validation;
    draftReady_ = static_cast<bool>(result.import);
    if (!draftReady_)
        result.outcome = DraftOutcome::ImportFailed;
    return result;
}
DraftResult ProfileController::activateDraft(std::uint32_t expectedDraft,
                                             std::uint32_t expectedConfiguration,
                                             std::uint32_t now) {
    const auto check = editAdmission(expectedDraft);
    if (check != DraftOutcome::Ready)
        return denied(check);
    if (!draftReady_)
        return denied(DraftOutcome::NoDraft);
    Guard guard(operating_);
    DraftResult result;
    result.activation = runtime_.activate(*profiles_[draft_], expectedConfiguration, now);
    if (!result.activation) {
        result.outcome = DraftOutcome::ActivationRejected;
        return result;
    }
    applying_ = true;
    result.outcome = DraftOutcome::Queued;
    return result;
}
DraftResult ProfileController::writeDraft(configuration::IProfileWriter &writer,
                                          std::uint32_t expectedRevision) {
    const auto check = admission();
    if (check != DraftOutcome::Ready)
        return denied(check);
    if (expectedRevision != draftRevision_)
        return denied(DraftOutcome::StaleRevision);
    if (!draftReady_)
        return denied(DraftOutcome::NoDraft);
    Guard guard(operating_);
    return configuration::writeProfile(*profiles_[draft_], writer)
               ? DraftResult()
               : denied(DraftOutcome::OutputFailed);
}
DraftResult ProfileController::writeActive(configuration::IProfileWriter &writer,
                                           std::uint32_t expectedRevision) {
    const auto check = admission();
    if (check != DraftOutcome::Ready)
        return denied(check);
    if (expectedRevision != runtime_.activation().configurationRevision)
        return denied(DraftOutcome::StaleRevision);
    if (active_ == Missing)
        return denied(DraftOutcome::NoActive);
    Guard guard(operating_);
    return configuration::writeProfile(*profiles_[active_], writer)
               ? DraftResult()
               : denied(DraftOutcome::OutputFailed);
}
void ProfileController::service(std::uint32_t now) {
    if (operating_)
        return;
    Guard guard(operating_);
    runtime_.service(now);
    if (!applying_ || runtime_.busy())
        return;
    const auto result = runtime_.activation().finish;
    if (result == ActivationFinish::None)
        return;
    if (result == ActivationFinish::Applied) {
        active_ = draft_;
        draft_ = static_cast<std::uint8_t>(1 - draft_);
        draftReady_ = false;
    }
    // RolledBack/Failed retains the old active slot and the now-unlocked draft.
    // Runtime has released its candidate. Completion consumes the reserved ID.
    ++draftRevision_;
    applying_ = false;
}
ProfileStorageSnapshot ProfileController::storage() const {
    ProfileStorageSnapshot result;
    result.initialized = initialized_;
    result.hasActive = active_ != Missing;
    result.draftReady = draftReady_;
    result.applying = applying_;
    result.editing = operating_;
    result.draftRevision = draftRevision_;
    result.activeProfileRevision = active_ == Missing ? 0 : profiles_[active_]->revision;
    result.draftProfileRevision = draftReady_ ? profiles_[draft_]->revision : 0;
    result.lastImport = lastImport_;
    return result;
}
execution::Result ProfileController::startGame(std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.startGame(revision, now);
}
execution::Result ProfileController::tickGame(std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.tickGame(now);
}
execution::Result ProfileController::stopGame(std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.stopGame(revision, now);
}
execution::Result ProfileController::pauseGame(std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.pauseGame(revision, now);
}
execution::Result ProfileController::resumeGame(std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.resumeGame(revision, now);
}
execution::Result ProfileController::changeState(configuration::StateId target,
                                                 std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.changeState(target, revision, now);
}
execution::Result ProfileController::takeTransition(configuration::TransitionId edge,
                                                    std::uint32_t revision, std::uint32_t now) {
    if (operating_ || applying_)
        return busyGame();
    Guard guard(operating_);
    return runtime_.takeTransition(edge, revision, now);
}
} // namespace devices
} // namespace larp
