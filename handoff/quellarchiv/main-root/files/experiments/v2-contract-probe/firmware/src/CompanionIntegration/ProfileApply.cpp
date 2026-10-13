#include "CompanionIntegration/ProfileApply.h"
namespace larp {
namespace integration {
namespace {
bool current(const companion::IGameControlChannel &channel,
             const companion::TrustedAuthority &original) {
    companion::TrustedAuthority value;
    return original.gameMaster && original.principal && original.epoch &&
           channel.readAuthority(value) && value == original;
}
} // namespace
ProfileApplyResult applyImportedProfile(devices::ProfileApplication &application,
                                        const companion::IGameControlChannel &authority,
                                        const ProfileImportReceipt &receipt, std::uint32_t now) {
    ProfileApplyResult result;
    const auto selected = receipt;
    if (!selected.issued() || !selected.source().valid() ||
        selected.draftRevision() != selected.source().expectedDraftRevision + 1)
        return result;
    if (!selected.belongsTo(application)) {
        result.outcome = ProfileApplyOutcome::WrongApplication;
        return result;
    }
    if (!current(authority, selected.original())) {
        result.outcome = ProfileApplyOutcome::Unauthorized;
        return result;
    }
    if (application.busy()) {
        result.outcome = ProfileApplyOutcome::Busy;
        return result;
    }
    if (application.snapshot().phase != devices::ApplicationPhase::Ready ||
        !application.storage().initialized || !application.storage().hasActive) {
        result.outcome = ProfileApplyOutcome::NotReady;
        return result;
    }
    if (application.gameRunning()) {
        result.outcome = ProfileApplyOutcome::GameRunning;
        return result;
    }
    if (application.storage().draftRevision != selected.draftRevision() ||
        application.activation().configurationRevision !=
            selected.source().expectedConfigurationRevision) {
        result.outcome = ProfileApplyOutcome::StaleRevision;
        return result;
    }
    // Another explicit observation after revision/admission selection. Do not
    // hold an RTOS authority lock over activation validation or device work.
    if (!current(authority, selected.original())) {
        result.outcome = ProfileApplyOutcome::Unauthorized;
        return result;
    }
    result.application = application.activateDraft(
        selected.draftRevision(), selected.source().expectedConfigurationRevision, now);
    result.outcome = result.application.outcome == devices::ApplicationOutcome::Queued
                         ? ProfileApplyOutcome::Queued
                         : ProfileApplyOutcome::Rejected;
    return result;
}
} // namespace integration
} // namespace larp
