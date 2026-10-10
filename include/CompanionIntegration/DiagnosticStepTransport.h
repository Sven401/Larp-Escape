#pragma once
#include "CompanionIntegration/DiagnosticControl.h"
namespace larp {
namespace diagnostic {
struct StepLimits {
    static const std::size_t RequestBytes = 80, ReplyHeader = 104, EdgeBytes = 10;
    static const std::size_t ReplyBytes = ReplyHeader + 64 * EdgeBytes;
    static const std::size_t FrameHeader = 12, Payload = 116, FrameBytes = 128;
};
bool encodeStep(std::uint32_t message, const Request &, std::uint8_t *, std::size_t) noexcept;
bool decodeStep(const std::uint8_t *, std::size_t, std::uint32_t &, Request &) noexcept;
bool encodeStepReply(std::uint32_t message, const Reply &, std::uint8_t *, std::size_t,
                     std::size_t &written) noexcept;
bool decodeStepReply(const std::uint8_t *, std::size_t, std::uint32_t &, Reply &) noexcept;
// Single serialized owner with IdentityAccess/Control/lifecycle, never SDK callbacks.
// Only synthetic host integration is installed. No implicit execution retries.
class StepTransport {
  public:
    StepTransport(companion::IdentityAccess &access, Control &control)
        : access_(access), control_(control) {}
    StepTransport(const StepTransport &) = delete;
    StepTransport &operator=(const StepTransport &) = delete;
    Admission receive(const std::uint8_t *, std::size_t, const companion::BoundConnection &,
                      const companion::TrustedAuthority &, std::uint64_t receivedAt,
                      std::uint64_t now) noexcept;
    void tick(std::uint64_t now) noexcept;
    void disconnect(const companion::BoundConnection &, std::uint64_t now) noexcept;
    // Emit at most one frame directly on the original transport owner turn.
    // Capacity controls payload size, enabling even ATT payload 20 (8 body bytes).
    bool nextFrame(const companion::BoundConnection &, std::uint64_t now, std::uint8_t *,
                   std::size_t capacity, std::size_t &written) noexcept;

  private:
    companion::IdentityAccess &access_;
    Control &control_;
    companion::BoundConnection binding_;
    companion::TrustedAuthority authority_;
    Request request_;
    std::array<std::uint8_t, StepLimits::ReplyBytes> bytes_ = {};
    std::uint32_t message_ = 0;
    std::uint64_t deadline_ = 0, lastNow_ = 0;
    std::size_t total_ = 0, offset_ = 0;
    bool active_ = false;
    void discard() noexcept;
};
enum class AssemblyResult : std::uint8_t {
    Receiving,
    Duplicate,
    Complete,
    Rejected,
    Expired,
    Closed
};
// Host client reference receiver. The real Android adapter must supply equally
// trusted original-binding/authority state. No JSON or unbounded allocations.
class ReplyAssembly {
  public:
    explicit ReplyAssembly(companion::IdentityAccess &access) : access_(access) {}
    ReplyAssembly(const ReplyAssembly &) = delete;
    ReplyAssembly &operator=(const ReplyAssembly &) = delete;
    bool begin(std::uint32_t message, const Request &, const companion::BoundConnection &,
               const companion::TrustedAuthority &, std::uint64_t now,
               std::uint64_t absoluteDeadline) noexcept;
    AssemblyResult receive(const std::uint8_t *, std::size_t, const companion::BoundConnection &,
                           const companion::TrustedAuthority &, std::uint64_t now) noexcept;
    bool take(std::uint64_t now, Reply &) noexcept;
    void retire() noexcept;

  private:
    companion::IdentityAccess &access_;
    companion::BoundConnection binding_;
    companion::TrustedAuthority authority_;
    Request request_;
    std::array<std::uint8_t, StepLimits::ReplyBytes> bytes_ = {};
    std::uint32_t message_ = 0;
    std::uint64_t lastNow_ = 0, deadline_ = 0;
    std::size_t total_ = 0, received_ = 0;
    bool active_ = false, complete_ = false;
    bool live(std::uint64_t now) noexcept;
};
} // namespace diagnostic
} // namespace larp
