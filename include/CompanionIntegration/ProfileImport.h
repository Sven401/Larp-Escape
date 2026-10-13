#pragma once
#include "Companion/ConfiguredProtocol.h"
#include "Companion/ProfileSourceEnvelope.h"
#include "DeviceRuntime/ProfileApplication.h"
namespace larp {
namespace integration {
enum class ProfileImportOutcome : std::uint8_t {
    Ready,
    InvalidEnvelope,
    Unauthorized,
    Expired,
    NotReady,
    Busy,
    GameRunning,
    StaleRevision,
    ImportFailed,
    SourceFailure,
    SourceLengthMismatch,
    SourceDigestMismatch
};
// Internal server-issued observation only. NEVER deserialize a client-supplied
// receipt as proof. Any later apply must recheck this original authority and both
// revisions in the sole application owner. This function never activates/saves.
struct ProfileImportResult;
class ProfileImportReceipt {
  public:
    bool issued() const noexcept {
        return issued_;
    }
    bool belongsTo(const devices::ProfileApplication &application) const noexcept {
        return issuer_ == &application;
    }
    const companion::TrustedAuthority &original() const noexcept {
        return original_;
    }
    const companion::ProfileSourceEnvelope &source() const noexcept {
        return source_;
    }
    std::uint32_t draftRevision() const noexcept {
        return draftRevision_;
    }

  private:
    friend ProfileImportResult importProfileSource(devices::ProfileApplication &,
                                                   const companion::IGameControlChannel &,
                                                   configuration::IProfileReader &,
                                                   const companion::ProfileSourceEnvelope &,
                                                   const companion::TrustedAuthority &,
                                                   companion::IProfileImportClock &, std::uint32_t);
    const devices::ProfileApplication *issuer_ = nullptr;
    companion::TrustedAuthority original_;
    companion::ProfileSourceEnvelope source_;
    std::uint32_t draftRevision_ = 0;
    bool issued_ = false;
};
struct ProfileImportResult {
    ProfileImportOutcome outcome = ProfileImportOutcome::InvalidEnvelope;
    devices::DraftResult draft;
    ProfileImportReceipt receipt;
    std::uint32_t consumed = 0;
};
// Complete, sealed, bounded source only: empty network queue is NOT EOF.
// Same sole synchronous application context, stopped game. No new Profile or
// document allocation. Partial failure may invalidate only the private draft.
// original/receivedAt are trusted immutable producer observations, not wire roles.
ProfileImportResult importProfileSource(devices::ProfileApplication &application,
                                        const companion::IGameControlChannel &authority,
                                        configuration::IProfileReader &source,
                                        const companion::ProfileSourceEnvelope &envelope,
                                        const companion::TrustedAuthority &original,
                                        companion::IProfileImportClock &clock,
                                        std::uint32_t receivedAt);
} // namespace integration
} // namespace larp
