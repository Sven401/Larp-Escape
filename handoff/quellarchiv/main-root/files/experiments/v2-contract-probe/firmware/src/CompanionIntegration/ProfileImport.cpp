#include "CompanionIntegration/ProfileImport.h"
namespace larp {
namespace integration {
namespace {
using namespace companion;
using configuration::ReadStatus;
class GuardedReader : public configuration::IProfileReader {
  public:
    GuardedReader(configuration::IProfileReader &source, const IGameControlChannel &authority,
                  const ProfileSourceEnvelope &envelope, const TrustedAuthority &original,
                  IProfileImportClock &clock, std::uint32_t receivedAt)
        : source_(source), authority_(authority), envelope_(envelope), original_(original),
          clock_(clock), receivedAt_(receivedAt) {}
    bool current() {
        if (clock_.nowMilliseconds() - receivedAt_ >= ProfileSourceLimits::DeadlineMilliseconds) {
            failure = ProfileImportOutcome::Expired;
            return false;
        }
        TrustedAuthority current;
        if (!original_.gameMaster || !original_.principal || !original_.epoch ||
            !authority_.readAuthority(current) || !(current == original_)) {
            failure = ProfileImportOutcome::Unauthorized;
            return false;
        }
        return true;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (failure != ProfileImportOutcome::Ready || !current())
            return ReadStatus::Failure;
        if (sealed)
            return ReadStatus::End;
        std::uint8_t value = 0;
        const auto status = source_.readByte(value);
        if (!current())
            return ReadStatus::Failure;
        if (status == ReadStatus::Failure) {
            failure = ProfileImportOutcome::SourceFailure;
        } else if (status == ReadStatus::Byte) {
            if (consumed >= envelope_.bytes) {
                failure = ProfileImportOutcome::SourceLengthMismatch;
            } else {
                ++consumed;
                crc_ = profileSourceCrcByte(crc_, value);
                byte = value;
                return ReadStatus::Byte;
            }
        } else if (consumed != envelope_.bytes) {
            failure = ProfileImportOutcome::SourceLengthMismatch;
        } else if ((crc_ ^ 0xffffffffu) != envelope_.crc32) {
            failure = ProfileImportOutcome::SourceDigestMismatch;
        } else {
            sealed = true; // EOF, exact length, CRC, deadline and route all checked.
            return ReadStatus::End;
        }
        return ReadStatus::Failure;
    }
    ProfileImportOutcome failure = ProfileImportOutcome::Ready;
    std::uint32_t consumed = 0;
    bool sealed = false;

  private:
    configuration::IProfileReader &source_;
    const IGameControlChannel &authority_;
    const ProfileSourceEnvelope envelope_;
    const TrustedAuthority original_;
    IProfileImportClock &clock_;
    const std::uint32_t receivedAt_;
    std::uint32_t crc_ = 0xffffffffu;
};
} // namespace
ProfileImportResult importProfileSource(devices::ProfileApplication &application,
                                        const companion::IGameControlChannel &authority,
                                        configuration::IProfileReader &source,
                                        const companion::ProfileSourceEnvelope &envelope,
                                        const companion::TrustedAuthority &original,
                                        companion::IProfileImportClock &clock,
                                        std::uint32_t receivedAt) {
    ProfileImportResult result;
    // Copy immutable caller observations before the first source/authority call.
    const auto selected = envelope;
    const auto route = original;
    if (!selected.valid())
        return result;
    GuardedReader guarded(source, authority, selected, route, clock, receivedAt);
    if (!guarded.current()) {
        result.outcome = guarded.failure;
        return result;
    }
    if (application.busy()) {
        result.outcome = ProfileImportOutcome::Busy;
        return result;
    }
    if (application.snapshot().phase != devices::ApplicationPhase::Ready ||
        !application.storage().initialized || !application.storage().hasActive) {
        result.outcome = ProfileImportOutcome::NotReady;
        return result;
    }
    if (application.gameRunning()) {
        result.outcome = ProfileImportOutcome::GameRunning;
        return result;
    }
    if (application.storage().draftRevision != selected.expectedDraftRevision ||
        application.activation().configurationRevision != selected.expectedConfigurationRevision) {
        result.outcome = ProfileImportOutcome::StaleRevision;
        return result;
    }
    result.draft = application.importDraft(guarded, selected.expectedDraftRevision);
    result.consumed = guarded.consumed;
    if (!result.draft || !guarded.sealed) {
        result.outcome = guarded.failure != ProfileImportOutcome::Ready
                             ? guarded.failure
                             : ProfileImportOutcome::ImportFailed;
        return result;
    }
    // Validation may take time after EOF. Never issue a usable route receipt on
    // a late closure/timeout. A local parsed draft may remain, with no apply/save.
    if (!guarded.current()) {
        result.outcome = guarded.failure;
        return result;
    }
    result.outcome = ProfileImportOutcome::Ready;
    result.receipt.original_ = route;
    result.receipt.source_ = selected;
    result.receipt.draftRevision_ = application.storage().draftRevision;
    result.receipt.issuer_ = &application;
    result.receipt.issued_ = true;
    return result;
}
} // namespace integration
} // namespace larp
