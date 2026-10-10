#pragma once
#include "CompanionIntegration/DiagnosticControl.h"
namespace larp {
namespace diagnostic {
enum class ControlCommand : std::uint8_t { Capabilities = 1, Grant = 2, Cancel = 3 };
struct ControlRequest {
    ControlCommand command = ControlCommand::Capabilities;
    std::uint32_t message = 0;
    companion::Identity device;
    std::uint64_t boot = 0;
    Uuid run, grant;
    std::uint32_t ttlMs = 0;
    bool consent = false;
};
struct TransportLimits {
    static const std::size_t RequestBytes = 69, ReplyBytes = 88, FrameBytes = 128;
    static const std::uint64_t QueueMs = 2000;
};
// Experimental DG control wire format, independent of GameCommand v2. Exact
// lengths, no fragments, no heap. Failed decode preserves destination.
bool decodeControlRequest(const std::uint8_t *, std::size_t, ControlRequest &) noexcept;
enum class TransportResult : std::uint8_t { Ready, Invalid, Unauthorized, Expired, Busy };
// Host adapter only. All calls, IdentityAccess, Control and component lifecycle
// MUST share one serialized owner. No SDK callback may call these concurrently.
// Metadata below is captured by the trusted adapter, never parsed from bytes.
class ControlTransport {
  public:
    ControlTransport(companion::IdentityAccess &access, Control &control,
                     const companion::Identity &device, std::uint64_t boot) noexcept
        : access_(access), control_(control), device_(device), boot_(boot) {}
    ControlTransport(const ControlTransport &) = delete;
    ControlTransport &operator=(const ControlTransport &) = delete;
    TransportResult receive(const std::uint8_t *, std::size_t,
                            const companion::BoundConnection &original,
                            const companion::TrustedAuthority &captured, std::uint64_t receivedAt,
                            std::uint64_t now) noexcept;
    // Sole original transport owner sends immediately in this same serialization
    // turn. Success consumes the single reply; failure preserves output bytes.
    bool takeReply(const companion::BoundConnection &, std::uint64_t now, std::uint8_t *,
                   std::size_t capacity, std::size_t &written) noexcept;
    void tick(std::uint64_t now) noexcept;
    void disconnect(const companion::BoundConnection &, std::uint64_t now) noexcept;

  private:
    companion::IdentityAccess &access_;
    Control &control_;
    companion::Identity device_;
    std::uint64_t boot_, replyAt_ = 0;
    companion::BoundConnection binding_, grantBinding_;
    companion::TrustedAuthority authority_;
    ControlRequest request_;
    Grant grant_, responseGrant_;
    Admission result_ = Admission::Invalid;
    bool ready_ = false, grantKnown_ = false;
    bool current(const companion::BoundConnection &,
                 const companion::TrustedAuthority &) const noexcept;
};
} // namespace diagnostic
} // namespace larp
