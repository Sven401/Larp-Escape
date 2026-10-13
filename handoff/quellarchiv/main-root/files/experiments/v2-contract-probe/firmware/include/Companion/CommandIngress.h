#pragma once
#include "Companion/GameCommandReader.h"
namespace larp {
namespace companion {
enum class GameSubmissionOutcome : std::uint8_t {
    Queued,
    Uninitialized,
    MissingPrincipal,
    MissingEpoch,
    Unauthorized,
    DecodeRejected,
    QueueFull
};
struct GameSubmission {
    GameSubmissionOutcome outcome = GameSubmissionOutcome::Uninitialized;
    // Only meaningful if parsing was reached.
    CommandDecodeResult decode;
    explicit operator bool() const {
        return outcome == GameSubmissionOutcome::Queued;
    }
};
// Trusted single transport owner. No client-provided identity or role.
class ICommandIngress {
  public:
    virtual ~ICommandIngress() = default;
    virtual bool readAuthority(TrustedAuthority &authority) const noexcept = 0;
    virtual GameSubmission submitJson(const char *bytes, std::size_t count, std::uint32_t principal,
                                      std::uint32_t epoch) noexcept = 0;
};
} // namespace companion
} // namespace larp
