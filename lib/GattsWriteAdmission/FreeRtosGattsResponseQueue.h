#pragma once
#include "GattsWriteAdmission.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
struct GattsResponseLimits {
    static const std::size_t Depth = 4;
    // Local selection policy, not an ATT timeout or measured SDK/RF latency.
    static const std::uint32_t MaxAgeMilliseconds = 1000;
};
class IGattsResponseContext {
  public:
    virtual ~IGattsResponseContext() = default;
    virtual std::uint32_t nowMilliseconds() const noexcept = 0;
    // Bounded zero-wait synchronized original lifecycle observation: SDK
    // generation, connection, address, interface and selected attribute. No
    // current-handle lookup, credential inference, or GM requirement for errors.
    virtual bool originalLive(const GattsWriteBinding &) const noexcept = 0;
};
class IGattsResponseSubmitter {
  public:
    virtual ~IGattsResponseSubmitter() = default;
    // Worker context only. Recheck original lifecycle and age at the actual
    // submission boundary. Future SDK port must prove serialization through
    // posting/consumption; conn_id alone cannot convey our generation token.
    // This call may wait/allocate: it must never run in raw callback/game owner.
    // True means local posting only, not ATT delivery or command execution.
    virtual bool submitOriginal(const RoutedWriteResponse &) noexcept = 0;
};
class GattsResponseDispatcher;
// Process-lifetime static queue. begin before workers, sole callback producer
// and sole worker consumer; no heap, retry, SDK call, game tick, file or parser.
class FreeRtosGattsResponseQueue : public IWriteResponseBearer {
  public:
    explicit FreeRtosGattsResponseQueue(const IGattsResponseContext &context) : context_(context) {}
    FreeRtosGattsResponseQueue(const FreeRtosGattsResponseQueue &) = delete;
    FreeRtosGattsResponseQueue &operator=(const FreeRtosGattsResponseQueue &) = delete;
    bool begin() noexcept;
    bool copyWriteResponse(const RoutedWriteResponse &) noexcept override;
    // At most Depth stale values + one current return, false preserves output.
    bool takeCurrent(RoutedWriteResponse &) noexcept;

  private:
    friend class GattsResponseDispatcher;
    const IGattsResponseContext &context_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(RoutedWriteResponse)
        std::uint8_t storage_[GattsResponseLimits::Depth * sizeof(RoutedWriteResponse)] = {};
    bool current(const RoutedWriteResponse &) const noexcept;
};
enum class GattsResponseDispatch : std::uint8_t { Idle, Busy, Rejected, Submitted };
// One selection and at most one submitter call per worker invocation. Failed
// posts are consumed without retry; the already copied input is unchanged.
class GattsResponseDispatcher {
  public:
    GattsResponseDispatcher(FreeRtosGattsResponseQueue &queue, IGattsResponseSubmitter &submitter)
        : queue_(queue), submitter_(submitter) {}
    GattsResponseDispatcher(const GattsResponseDispatcher &) = delete;
    GattsResponseDispatcher &operator=(const GattsResponseDispatcher &) = delete;
    GattsResponseDispatch service() noexcept;

  private:
    FreeRtosGattsResponseQueue &queue_;
    IGattsResponseSubmitter &submitter_;
    bool busy_ = false;
};
} // namespace companion
} // namespace larp
