#pragma once
#include "Companion/GameCommandReader.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
// Process-lifetime static queues; begin before tasks/callbacks. Zero wait,
// value copies, no ISR API. Trusted transport owner attaches principal and
// exclusively drains/routes replies. Application alone receive/send/publish.
enum class GameSubmissionOutcome : std::uint8_t {
    Queued,
    Uninitialized,
    MissingPrincipal,
    DecodeRejected,
    QueueFull
};
struct GameSubmission {
    GameSubmissionOutcome outcome = GameSubmissionOutcome::Uninitialized;
    // Meaningful only after parsing; Uninitialized/MissingPrincipal skip it.
    CommandDecodeResult decode;
    explicit operator bool() const {
        return outcome == GameSubmissionOutcome::Queued;
    }
};
class FreeRtosGameControlChannel : public IGameControlChannel {
  public:
    FreeRtosGameControlChannel() = default;
    FreeRtosGameControlChannel(const FreeRtosGameControlChannel &) = delete;
    FreeRtosGameControlChannel &operator=(const FreeRtosGameControlChannel &) = delete;
    bool begin();
    bool receive(TrustedGameRequest &request) noexcept override;
    bool send(const RoutedGameReply &reply) noexcept override;
    bool publish(const ConfiguredStatus &status) noexcept override;
    bool submit(const TrustedGameRequest &request);
    // Complete immutable message from a trusted authenticated transport owner.
    // principal comes from that connection, NEVER from JSON. Decode success is
    // not execution; a full queue leaves the command unadmitted for exact retry.
    GameSubmission submitJson(const char *bytes, std::size_t count,
                              std::uint32_t authenticatedPrincipal);
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
    bool started_ = false;
};
} // namespace companion
} // namespace larp
