#include "FreeRtosGameControlChannel.h"
#include <type_traits>
namespace larp {
namespace companion {
static_assert(ControlLimits::Queue > 0, "Value FIFO requires nonzero capacity");
static_assert(std::is_trivially_copyable<TrustedGameRequest>::value,
              "Queue requires copied command values");
static_assert(std::is_trivially_copyable<RoutedGameReply>::value,
              "Queue requires copied reply values");
static_assert(std::is_trivially_copyable<RoutedConfiguredStatus>::value,
              "Queue requires copied status values");
bool FreeRtosGameControlChannel::begin() {
    if (started_)
        return false;
    started_ = true; // Initialization completes before task/callback publication.
    return true;
}
bool FreeRtosGameControlChannel::receive(TrustedGameRequest &request) noexcept {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    const bool received = commands_.pop(request);
    portEXIT_CRITICAL(&authorityMutex_);
    return received;
}
bool FreeRtosGameControlChannel::send(const RoutedGameReply &reply) noexcept {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    const bool sent = replies_.push(reply);
    portEXIT_CRITICAL(&authorityMutex_);
    return sent;
}
bool FreeRtosGameControlChannel::publish(const RoutedConfiguredStatus &status) noexcept {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    status_ = status;
    statusAvailable_ = true;
    portEXIT_CRITICAL(&authorityMutex_);
    return true;
}
AuthorityUpdate FreeRtosGameControlChannel::updateAuthority(const TrustedAuthority &authority) {
    if (!started_)
        return AuthorityUpdate::Suspended;
    portENTER_CRITICAL(&authorityMutex_);
    const auto result = authority_.update(authority);
    portEXIT_CRITICAL(&authorityMutex_);
    return result;
}
AuthorityUpdate FreeRtosGameControlChannel::advanceAuthority(std::uint32_t principal,
                                                             bool gameMaster,
                                                             TrustedAuthority &published) noexcept {
    if (!started_)
        return AuthorityUpdate::Suspended;
    portENTER_CRITICAL(&authorityMutex_);
    const auto result = authority_.advance(principal, gameMaster, published);
    portEXIT_CRITICAL(&authorityMutex_);
    return result;
}
AuthorityUpdate
FreeRtosGameControlChannel::advanceAuthorityIfCurrent(const TrustedAuthority &expected,
                                                      std::uint32_t principal, bool gameMaster,
                                                      TrustedAuthority &published) noexcept {
    if (!started_)
        return AuthorityUpdate::Suspended;
    const auto original = expected; // Freeze before lock acquisition can observe external work.
    portENTER_CRITICAL(&authorityMutex_);
    const auto result = authority_.advanceIfCurrent(original, principal, gameMaster, published);
    portEXIT_CRITICAL(&authorityMutex_);
    return result;
}
AuthorityUpdate
FreeRtosGameControlChannel::closeAuthorityIfCurrent(const TrustedAuthority &expected,
                                                    TrustedAuthority &published) noexcept {
    if (!started_)
        return AuthorityUpdate::Suspended;
    // Freeze before the first external observation, including lock acquisition.
    const auto original = expected;
    portENTER_CRITICAL(&authorityMutex_);
    const auto result = authority_.closeIfCurrent(original, published);
    portEXIT_CRITICAL(&authorityMutex_);
    return result;
}
void FreeRtosGameControlChannel::suspendAuthority() noexcept {
    portENTER_CRITICAL(&authorityMutex_);
    authority_.suspend();
    portEXIT_CRITICAL(&authorityMutex_);
}
bool FreeRtosGameControlChannel::readAuthority(TrustedAuthority &authority) const noexcept {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    const bool available = authority_.read(authority);
    portEXIT_CRITICAL(&authorityMutex_);
    return available;
}
GameSubmissionOutcome FreeRtosGameControlChannel::admit(const TrustedGameRequest &request) {
    if (!started_)
        return GameSubmissionOutcome::Uninitialized;
    if (!request.principal)
        return GameSubmissionOutcome::MissingPrincipal;
    if (!request.connectionEpoch)
        return GameSubmissionOutcome::MissingEpoch;
    // Authority observation and copied admission share one short lock.
    // Only bounded value storage is accessed; no RTOS/device call or second lock.
    portENTER_CRITICAL(&authorityMutex_);
    const auto outcome = !authority_.admits(request.principal, request.connectionEpoch)
                             ? GameSubmissionOutcome::Unauthorized
                         : commands_.push(request) ? GameSubmissionOutcome::Queued
                                                   : GameSubmissionOutcome::QueueFull;
    portEXIT_CRITICAL(&authorityMutex_);
    return outcome;
}
bool FreeRtosGameControlChannel::submit(const TrustedGameRequest &request) {
    return admit(request) == GameSubmissionOutcome::Queued;
}
GameSubmission FreeRtosGameControlChannel::submitJson(const char *bytes, std::size_t count,
                                                      std::uint32_t authenticatedPrincipal,
                                                      std::uint32_t connectionEpoch) noexcept {
    GameSubmission result;
    if (!started_)
        return result;
    if (!authenticatedPrincipal) {
        result.outcome = GameSubmissionOutcome::MissingPrincipal;
        return result;
    }
    if (!connectionEpoch) {
        result.outcome = GameSubmissionOutcome::MissingEpoch;
        return result;
    }
    TrustedGameRequest request;
    request.principal = authenticatedPrincipal;
    request.connectionEpoch = connectionEpoch;
    result.decode = decodeGameCommand(bytes, count, request.command);
    if (!result.decode) {
        result.outcome = GameSubmissionOutcome::DecodeRejected;
        return result;
    }
    result.outcome = admit(request);
    return result;
}
bool FreeRtosGameControlChannel::takeReply(RoutedGameReply &reply) {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    const bool received = replies_.pop(reply);
    portEXIT_CRITICAL(&authorityMutex_);
    return received;
}
bool FreeRtosGameControlChannel::takeCurrentReply(RoutedGameReply &reply) {
    if (!started_)
        return false;
    bool selected = false;
    RoutedGameReply candidate;
    // Select against the same authority observation as the copied FIFO removal.
    // No network call, RTOS API or nested lock is needed.
    portENTER_CRITICAL(&authorityMutex_);
    for (std::size_t i = 0; i < ControlLimits::Queue; ++i) {
        if (!replies_.pop(candidate))
            break;
        if (authority_.admits(candidate.principal, candidate.connectionEpoch)) {
            reply = candidate;
            selected = true;
            break;
        }
    }
    portEXIT_CRITICAL(&authorityMutex_);
    return selected;
}
bool FreeRtosGameControlChannel::readStatus(ConfiguredStatus &status) const {
    if (!started_)
        return false;
    portENTER_CRITICAL(&authorityMutex_);
    const bool available = statusAvailable_;
    if (available)
        status = status_.status;
    portEXIT_CRITICAL(&authorityMutex_);
    return available;
}
bool FreeRtosGameControlChannel::readCurrentStatus(RoutedConfiguredStatus &status) const {
    if (!started_)
        return false;
    bool selected = false;
    portENTER_CRITICAL(&authorityMutex_);
    if (statusAvailable_ && authority_.admits(status_.principal, status_.connectionEpoch)) {
        status = status_;
        selected = true;
    }
    portEXIT_CRITICAL(&authorityMutex_);
    return selected;
}
} // namespace companion
} // namespace larp
