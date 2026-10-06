#pragma once
#include "Companion/CommandIngress.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
// Process-lifetime static queues; begin before tasks/callbacks. Zero wait,
// value copies, no ISR API. Trusted transport owner attaches principal and
// exclusively drains/routes replies by principal AND connectionEpoch.
// Application alone receive/send/publish.
class FreeRtosGameControlChannel : public IGameControlChannel, public ICommandIngress {
  public:
    FreeRtosGameControlChannel() = default;
    FreeRtosGameControlChannel(const FreeRtosGameControlChannel &) = delete;
    FreeRtosGameControlChannel &operator=(const FreeRtosGameControlChannel &) = delete;
    bool begin();
    // Single verified transport owner; observer/disconnect also advance epoch.
    AuthorityUpdate updateAuthority(const TrustedAuthority &authority);
    void suspendAuthority();
    bool readAuthority(TrustedAuthority &authority) const noexcept override;
    bool receive(TrustedGameRequest &request) noexcept override;
    bool send(const RoutedGameReply &reply) noexcept override;
    bool publish(const ConfiguredStatus &status) noexcept override;
    bool submit(const TrustedGameRequest &request);
    // Complete immutable message from a trusted authenticated transport owner.
    // principal and epoch come from that specific connection, NEVER from JSON.
    // Admission rechecks the current authority after decode. Decode success is
    // not execution; a full queue leaves the command unadmitted for exact retry.
    GameSubmission submitJson(const char *bytes, std::size_t count,
                              std::uint32_t authenticatedPrincipal,
                              std::uint32_t connectionEpoch) noexcept override;
    bool takeReply(RoutedGameReply &reply);
    bool readStatus(ConfiguredStatus &status) const;

  private:
    QueueHandle_t commands_ = nullptr, replies_ = nullptr, status_ = nullptr;
    StaticQueue_t commandControl_ = {}, replyControl_ = {}, statusControl_ = {};
    alignas(TrustedGameRequest) std::uint8_t
        commandBytes_[ControlLimits::Queue * sizeof(TrustedGameRequest)] = {};
    alignas(RoutedGameReply) std::uint8_t
        replyBytes_[ControlLimits::Queue * sizeof(RoutedGameReply)] = {};
    alignas(ConfiguredStatus) std::uint8_t statusBytes_[sizeof(ConfiguredStatus)] = {};
    mutable portMUX_TYPE authorityMutex_ = portMUX_INITIALIZER_UNLOCKED;
    AuthorityState authority_;
    bool started_ = false;
    GameSubmissionOutcome admit(const TrustedGameRequest &request);
};
} // namespace companion
} // namespace larp
