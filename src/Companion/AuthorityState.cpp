#include "Companion/AuthorityState.h"
namespace larp {
namespace companion {
AuthorityUpdate AuthorityState::update(const TrustedAuthority &authority) noexcept {
    const auto previous = highest_;
    if (authority.epoch > highest_)
        highest_ = authority.epoch;
    if (!authority.epoch || (authority.gameMaster && !authority.principal)) {
        suspend();
        return AuthorityUpdate::Invalid;
    }
    if (authority.epoch < previous) {
        suspend();
        return AuthorityUpdate::Stale;
    }
    if (authority.epoch == previous) {
        if (!available_)
            return AuthorityUpdate::Suspended;
        if (authority == current_)
            return AuthorityUpdate::Duplicate;
        suspend();
        return AuthorityUpdate::Conflict;
    }
    current_ = authority;
    available_ = true;
    return AuthorityUpdate::Published;
}
AuthorityUpdate AuthorityState::advance(std::uint32_t principal, bool gameMaster,
                                        TrustedAuthority &published) noexcept {
    if (highest_ == UINT32_MAX) {
        suspend();
        return AuthorityUpdate::Suspended;
    }
    TrustedAuthority next;
    next.epoch = highest_ + 1;
    next.principal = principal;
    next.gameMaster = gameMaster;
    const auto result = update(next);
    if (result == AuthorityUpdate::Published)
        published = next;
    return result;
}
AuthorityUpdate AuthorityState::advanceIfCurrent(const TrustedAuthority &expected,
                                                 std::uint32_t principal, bool gameMaster,
                                                 TrustedAuthority &published) noexcept {
    if (!available_)
        return AuthorityUpdate::Suspended;
    if (!(expected == current_)) {
        suspend();
        return AuthorityUpdate::Conflict;
    }
    return advance(principal, gameMaster, published);
}
void AuthorityState::suspend() noexcept {
    available_ = false;
}
bool AuthorityState::read(TrustedAuthority &authority) const noexcept {
    if (!available_)
        return false;
    authority = current_;
    return true;
}
bool AuthorityState::admits(std::uint32_t principal, std::uint32_t epoch) const noexcept {
    return available_ && current_.gameMaster && principal != 0 && epoch != 0 &&
           principal == current_.principal && epoch == current_.epoch;
}
} // namespace companion
} // namespace larp
