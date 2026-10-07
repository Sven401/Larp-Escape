#include "CompanionIntegration/ProfileTransferControl.h"
namespace larp {
namespace integration {
namespace {
using namespace companion;
class Call {
  public:
    explicit Call(bool &v) : v_(v) {
        v_ = true;
    }
    ~Call() {
        v_ = false;
    }

  private:
    bool &v_;
};
ProfileResponseDetail receiveDetail(ProfileReceiveOutcome v) {
    switch (v) {
    case ProfileReceiveOutcome::Busy:
        return ProfileResponseDetail::Busy;
    case ProfileReceiveOutcome::WrongRoute:
    case ProfileReceiveOutcome::NotReceiving:
        return ProfileResponseDetail::WrongTransfer;
    case ProfileReceiveOutcome::Incomplete:
        return ProfileResponseDetail::Incomplete;
    case ProfileReceiveOutcome::DigestMismatch:
        return ProfileResponseDetail::DigestMismatch;
    case ProfileReceiveOutcome::StorageFailure:
        return ProfileResponseDetail::StorageFailure;
    default:
        return ProfileResponseDetail::Failed;
    }
}
ProfileResponseDetail importDetail(ProfileImportOutcome v) {
    switch (v) {
    case ProfileImportOutcome::Busy:
        return ProfileResponseDetail::Busy;
    case ProfileImportOutcome::NotReady:
        return ProfileResponseDetail::NotReady;
    case ProfileImportOutcome::GameRunning:
        return ProfileResponseDetail::GameRunning;
    case ProfileImportOutcome::StaleRevision:
        return ProfileResponseDetail::StaleRevision;
    case ProfileImportOutcome::SourceDigestMismatch:
        return ProfileResponseDetail::DigestMismatch;
    default:
        return ProfileResponseDetail::InvalidSource;
    }
}
ProfileResponseDetail applyDetail(ProfileApplyOutcome v) {
    switch (v) {
    case ProfileApplyOutcome::Busy:
        return ProfileResponseDetail::Busy;
    case ProfileApplyOutcome::NotReady:
        return ProfileResponseDetail::NotReady;
    case ProfileApplyOutcome::GameRunning:
        return ProfileResponseDetail::GameRunning;
    case ProfileApplyOutcome::StaleRevision:
        return ProfileResponseDetail::StaleRevision;
    case ProfileApplyOutcome::InvalidReceipt:
    case ProfileApplyOutcome::WrongApplication:
        return ProfileResponseDetail::NoReceipt;
    default:
        return ProfileResponseDetail::Failed;
    }
}
} // namespace
bool ProfileTransferControl::current(const TrustedAuthority &original) noexcept {
    TrustedAuthority value;
    return original.gameMaster && original.principal && original.epoch &&
           authority_.readAuthority(value) && value == original;
}
void ProfileTransferControl::close() noexcept {
    receipt_ = ProfileImportReceipt();
    pending_ = applying_ = false;
    lastRequest_ = 0;
    session_ = TrustedAuthority();
}
ProfileResponseFrame ProfileTransferControl::reply(const ProfileControlFrame &command,
                                                   ProfileResponseCode code,
                                                   ProfileResponseDetail detail) const noexcept {
    ProfileResponseFrame value;
    value.command = command;
    value.code = code;
    value.detail = detail;
    value.draftRevision = app_.storage().draftRevision;
    value.configurationRevision = app_.activation().configurationRevision;
    return value;
}
void ProfileTransferControl::emit(const ProfileResponseFrame &value) noexcept {
    response_ = value;
    produced_ = clock_.nowMilliseconds();
    pending_ = true;
}
void ProfileTransferControl::sourceStarted() noexcept {
    if (!busy_)
        receipt_ = ProfileImportReceipt();
}
void ProfileTransferControl::service() noexcept {
    if (busy_)
        return;
    Call call(busy_);
    if (session_.epoch && !current(session_)) {
        close();
        return;
    }
    if (pending_ &&
        clock_.nowMilliseconds() - produced_ >= ProfileControlLimits::ResponseMilliseconds)
        pending_ = false;
    if (!applying_ || pending_ || app_.busy())
        return;
    const auto state = app_.snapshot();
    const auto storage = app_.storage();
    const auto activation = app_.activation();
    const bool saved =
        state.phase == devices::ApplicationPhase::Ready &&
        state.operation == devices::ApplicationOperation::Apply && state.storedMatchesActive &&
        state.hasStoredProfile &&
        activation.configurationRevision == receipt_.source().expectedConfigurationRevision + 1 &&
        storage.activeProfileRevision == targetProfile_ &&
        state.storedProfileRevision == targetProfile_ &&
        state.storedConfigurationRevision == activation.configurationRevision &&
        state.storedGeneration == targetGeneration_;
    cached_ = reply(cached_.command,
                    saved ? ProfileResponseCode::Saved : ProfileResponseCode::ApplyFailed,
                    saved ? ProfileResponseDetail::None : ProfileResponseDetail::Failed);
    applying_ = false;
    emit(cached_);
}
void ProfileTransferControl::handle(const ProfileControlFrame &input,
                                    const TrustedAuthority &original,
                                    std::uint32_t receivedAt) noexcept {
    if (!ready())
        return; // Pump never takes another job while response/transaction pending.
    Call call(busy_);
    const auto command = input;
    const auto route = original;
    if (!command.valid() || !current(route))
        return; // No reply leaks onto an unrelated connection.
    if (!(route == session_)) {
        close();
        session_ = route;
    }
    if (clock_.nowMilliseconds() - receivedAt >= ProfileControlLimits::QueueMilliseconds) {
        emit(reply(command, ProfileResponseCode::Expired));
        return;
    }
    if (command.request == lastRequest_) {
        emit(command == cached_.command ? cached_
                                        : reply(command, ProfileResponseCode::RequestConflict));
        return;
    }
    if (command.request < lastRequest_) {
        emit(reply(command, ProfileResponseCode::RequestExpired));
        return;
    }
    if (lastRequest_ == UINT32_MAX || command.request != lastRequest_ + 1) {
        emit(reply(command, ProfileResponseCode::OutOfOrder));
        return;
    }
    // Consume exactly one monotonically increasing request; never wrap/reexecute.
    lastRequest_ = command.request;
    cached_ =
        reply(command, ProfileResponseCode::ReceiveRejected, ProfileResponseDetail::WrongTransfer);
    if (!current(route)) {
        close();
        return;
    }
    if (command.operation == ProfileOperation::Abort) {
        const bool matches =
            (source_.original() == route && source_.envelope().transfer == command.transfer &&
             source_.phase() != ProfileReceivePhase::Empty) ||
            (receipt_.issued() && receipt_.original() == route &&
             receipt_.source().transfer == command.transfer);
        if (matches) {
            receipt_ = ProfileImportReceipt();
            const auto discarded = source_.discard();
            cached_ =
                reply(command,
                      discarded == ProfileReceiveOutcome::Discarded
                          ? ProfileResponseCode::Aborted
                          : ProfileResponseCode::ReceiveRejected,
                      discarded == ProfileReceiveOutcome::Discarded ? ProfileResponseDetail::None
                                                                    : receiveDetail(discarded));
        }
    } else if (command.operation == ProfileOperation::Finish) {
        const auto finished = source_.finish(route, command.transfer);
        if (finished != ProfileReceiveOutcome::Complete) {
            cached_ = reply(command,
                            finished == ProfileReceiveOutcome::Expired
                                ? ProfileResponseCode::Expired
                                : ProfileResponseCode::ReceiveRejected,
                            receiveDetail(finished));
        } else {
            const auto imported = importProfileSource(app_, authority_, source_, source_.envelope(),
                                                      route, clock_, source_.receivedAt());
            receipt_ = imported.receipt;
            const auto discarded = source_.discard();
            if (imported.outcome == ProfileImportOutcome::Ready &&
                discarded == ProfileReceiveOutcome::Discarded) {
                cached_ = reply(command, ProfileResponseCode::DraftReady);
            } else {
                receipt_ = ProfileImportReceipt();
                cached_ = reply(command,
                                discarded != ProfileReceiveOutcome::Discarded
                                    ? ProfileResponseCode::ReceiveRejected
                                : imported.outcome == ProfileImportOutcome::Expired
                                    ? ProfileResponseCode::Expired
                                    : ProfileResponseCode::ImportRejected,
                                discarded != ProfileReceiveOutcome::Discarded
                                    ? receiveDetail(discarded)
                                    : importDetail(imported.outcome));
            }
        }
    } else {
        if (receipt_.issued() && receipt_.original() == route &&
            receipt_.source().transfer == command.transfer) {
            const auto generation = app_.snapshot().storedGeneration;
            const auto profile = app_.storage().draftProfileRevision;
            const auto applied =
                applyImportedProfile(app_, authority_, receipt_, clock_.nowMilliseconds());
            if (applied.outcome == ProfileApplyOutcome::Queued) {
                applying_ = true;
                targetGeneration_ = generation + 1;
                targetProfile_ = profile;
                cached_ = reply(command, ProfileResponseCode::ApplyQueued);
            } else
                cached_ = reply(command, ProfileResponseCode::ApplyRejected,
                                applyDetail(applied.outcome));
        } else
            cached_ = reply(command, ProfileResponseCode::ApplyRejected,
                            ProfileResponseDetail::NoReceipt);
    }
    if (!current(route)) {
        close();
        return;
    }
    emit(cached_);
}
bool ProfileTransferControl::offerResponse(IProfileResponseQueue &queue) noexcept {
    if (busy_ || !pending_)
        return false;
    Call call(busy_);
    if (!current(session_)) {
        close();
        return false;
    }
    if (clock_.nowMilliseconds() - produced_ >= ProfileControlLimits::ResponseMilliseconds) {
        pending_ = false;
        return false;
    }
    RoutedProfileResponse value;
    value.original = session_;
    value.producedAt = produced_;
    value.response = response_;
    if (queue.copyResponse(value) != ProfileResponseCopy::Copied)
        return false;
    pending_ = false;
    return true;
}
bool ProfileTransferControl::takeResponse(RoutedProfileResponse &destination) noexcept {
    if (busy_ || !pending_)
        return false;
    Call call(busy_);
    if (!current(session_)) {
        close();
        return false;
    }
    if (clock_.nowMilliseconds() - produced_ >= ProfileControlLimits::ResponseMilliseconds) {
        pending_ = false;
        return false;
    }
    RoutedProfileResponse value;
    value.original = session_;
    value.producedAt = produced_;
    value.response = response_;
    destination = value;
    pending_ = false;
    return true;
}
} // namespace integration
} // namespace larp
