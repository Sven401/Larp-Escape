#include "FreeRtosGameControlChannel.h"
#include <type_traits>
namespace larp {
namespace companion {
static_assert(std::is_trivially_copyable<TrustedGameRequest>::value,
              "Queue requires copied command values");
static_assert(std::is_trivially_copyable<RoutedGameReply>::value,
              "Queue requires copied reply values");
static_assert(std::is_trivially_copyable<RoutedConfiguredStatus>::value,
              "Queue requires copied status values");
bool FreeRtosGameControlChannel::begin() {
    if (started_)
        return false;
    commands_ = xQueueCreateStatic(ControlLimits::Queue, sizeof(TrustedGameRequest), commandBytes_,
                                   &commandControl_);
    replies_ = xQueueCreateStatic(ControlLimits::Queue, sizeof(RoutedGameReply), replyBytes_,
                                  &replyControl_);
    status_ = xQueueCreateStatic(1, sizeof(RoutedConfiguredStatus), statusBytes_, &statusControl_);
    started_ = commands_ && replies_ && status_;
    return started_;
}
bool FreeRtosGameControlChannel::receive(TrustedGameRequest &request) noexcept {
    return started_ && xQueueReceive(commands_, &request, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::send(const RoutedGameReply &reply) noexcept {
    return started_ && xQueueSendToBack(replies_, &reply, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::publish(const RoutedConfiguredStatus &status) noexcept {
    return started_ && xQueueOverwrite(status_, &status) == pdTRUE;
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
    portENTER_CRITICAL(&authorityMutex_);
    const auto result = authority_.advanceIfCurrent(expected, principal, gameMaster, published);
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
    // No wait/device calls. Updates and copied admission share this short lock.
    // Consumer queue operations finish before it reads authority: no reverse lock.
    portENTER_CRITICAL(&authorityMutex_);
    const auto outcome = !authority_.admits(request.principal, request.connectionEpoch)
                             ? GameSubmissionOutcome::Unauthorized
                         : xQueueSendToBack(commands_, &request, 0) == pdTRUE
                             ? GameSubmissionOutcome::Queued
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
    return started_ && xQueueReceive(replies_, &reply, 0) == pdTRUE;
}
bool FreeRtosGameControlChannel::takeCurrentReply(RoutedGameReply &reply) {
    if (!started_)
        return false;
    bool selected = false;
    RoutedGameReply candidate;
    // Queue operations on the application side release their lock before an
    // authority read. No network call, wait or reverse lock order inside here.
    portENTER_CRITICAL(&authorityMutex_);
    for (std::size_t i = 0; i < ControlLimits::Queue; ++i) {
        if (xQueueReceive(replies_, &candidate, 0) != pdTRUE)
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
    RoutedConfiguredStatus copied;
    if (xQueuePeek(status_, &copied, 0) != pdTRUE)
        return false;
    status = copied.status;
    return true;
}
bool FreeRtosGameControlChannel::readCurrentStatus(RoutedConfiguredStatus &status) const {
    if (!started_)
        return false;
    RoutedConfiguredStatus copied;
    bool selected = false;
    portENTER_CRITICAL(&authorityMutex_);
    if (xQueuePeek(status_, &copied, 0) == pdTRUE &&
        authority_.admits(copied.principal, copied.connectionEpoch)) {
        status = copied;
        selected = true;
    }
    portEXIT_CRITICAL(&authorityMutex_);
    return selected;
}
} // namespace companion
} // namespace larp
