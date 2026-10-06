#pragma once
#include <cstdint>
namespace larp {
namespace companion {
// Supplied only by a verified transport owner, never by client JSON.
struct TrustedAuthority {
    std::uint32_t epoch = 0, principal = 0;
    bool gameMaster = false;
    bool operator==(const TrustedAuthority &other) const {
        return epoch == other.epoch && principal == other.principal &&
               gameMaster == other.gameMaster;
    }
};
enum class AuthorityUpdate : std::uint8_t {
    Published,
    Duplicate,
    Invalid,
    Stale,
    Conflict,
    Suspended
};
// Bounded state machine. The adapter serializes update/read/admission under its
// own short lock. A fault fences the highest observed epoch until a newer valid
// snapshot arrives. There is no reset or wrap operation within a boot.
class AuthorityState {
  public:
    AuthorityUpdate update(const TrustedAuthority &authority) noexcept;
    void suspend() noexcept;
    bool read(TrustedAuthority &authority) const noexcept;
    bool admits(std::uint32_t principal, std::uint32_t epoch) const noexcept;

  private:
    TrustedAuthority current_;
    std::uint32_t highest_ = 0;
    bool available_ = false;
};
} // namespace companion
} // namespace larp
