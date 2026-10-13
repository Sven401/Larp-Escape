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
// Only a serialized trusted lifecycle owner may advance this state.
// Epoch allocation is inside the synchronized channel, never client metadata.
class IAuthorityPublisher {
  public:
    virtual ~IAuthorityPublisher() = default;
    virtual AuthorityUpdate advanceAuthority(std::uint32_t principal, bool gameMaster,
                                             TrustedAuthority &published) noexcept = 0;
    // Check the original observation and grant in ONE adapter lock. A later
    // lifecycle closure must defeat a delayed verification/role grant. Conflict
    // preserves the replacement producer and destination; the old owner retires.
    virtual AuthorityUpdate advanceAuthorityIfCurrent(const TrustedAuthority &expected,
                                                      std::uint32_t principal, bool gameMaster,
                                                      TrustedAuthority &published) noexcept = 0;
    // Retire only this full original authority in one lock. A replacement is
    // preserved on Conflict; an old grant or retirement cannot close it.
    // Success allocates a new closed epoch; false preserves the destination.
    virtual AuthorityUpdate closeAuthorityIfCurrent(const TrustedAuthority &expected,
                                                    TrustedAuthority &published) noexcept = 0;
    virtual void suspendAuthority() noexcept = 0;
    virtual bool readAuthority(TrustedAuthority &authority) const noexcept = 0;
};
// Bounded state machine. The adapter serializes update/read/admission under its
// own short lock. A fault fences the highest observed epoch until a newer valid
// snapshot arrives. There is no reset or wrap operation within a boot.
class AuthorityState {
  public:
    AuthorityUpdate update(const TrustedAuthority &authority) noexcept;
    // Allocate strictly above the highest observed epoch, including fault fences.
    // Failure closes authority and preserves destination; maximum never wraps.
    AuthorityUpdate advance(std::uint32_t principal, bool gameMaster,
                            TrustedAuthority &published) noexcept;
    AuthorityUpdate advanceIfCurrent(const TrustedAuthority &expected, std::uint32_t principal,
                                     bool gameMaster, TrustedAuthority &published) noexcept;
    AuthorityUpdate closeIfCurrent(const TrustedAuthority &expected,
                                   TrustedAuthority &published) noexcept;
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
