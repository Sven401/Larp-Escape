#pragma once
#include "CompanionIntegration/ProfileImport.h"
namespace larp {
namespace integration {
enum class ProfileApplyOutcome : std::uint8_t {
    Queued,
    InvalidReceipt,
    WrongApplication,
    Unauthorized,
    NotReady,
    Busy,
    GameRunning,
    StaleRevision,
    Rejected
};
struct ProfileApplyResult {
    ProfileApplyOutcome outcome = ProfileApplyOutcome::InvalidReceipt;
    devices::ApplicationResult application;
};
// Sole application owner only. Server-owned receipt; no client deserialization.
// Reobserve original authority immediately before the actual activation request.
// Queued is not applied/saved. Once selected/queued, subsequent revocation does
// not retroactively undo a hardware transaction; no implicit game start.
ProfileApplyResult applyImportedProfile(devices::ProfileApplication &application,
                                        const companion::IGameControlChannel &authority,
                                        const ProfileImportReceipt &receipt, std::uint32_t now);
} // namespace integration
} // namespace larp
