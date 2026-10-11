#pragma once
#include "Companion/IdentityAccess.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace diagnostic {
// v1 attribute UUIDs in canonical text order. The legacy game service remains
// separate. Registration of this service is deliberately disabled in product.
struct GattsRouteV1 {
    static const std::uint8_t Version = 1;
    static const std::size_t AttValueBytes = 128;
    static const std::uint16_t MinimumMtu = 131; // ATT value = MTU - 3.
    static const std::size_t ControlRequestBytes = 69, ControlReplyBytes = 88;
    static const std::size_t StepRequestBytes = 80, StepReplyFrameBytes = 128;
    static std::array<std::uint8_t, 16> service() noexcept;
    static std::array<std::uint8_t, 16> controlWrite() noexcept;
    static std::array<std::uint8_t, 16> controlReply() noexcept;
    static std::array<std::uint8_t, 16> stepWrite() noexcept;
    static std::array<std::uint8_t, 16> stepReply() noexcept;
    static bool mtuSufficient(std::uint16_t mtu) noexcept { return mtu >= MinimumMtu; }
};

// Exactly one original connection birth and full role epoch copied by the
// trusted SDK actor. A raw conn_id, bond, CCCD subscription or GATT write is
// never an authority source.
struct GattsRouteOrigin {
    companion::BoundConnection connection;
    companion::TrustedAuthority authority;
    std::uint32_t sdkGeneration = 0;
    std::uint8_t interface = 0xff;
    std::array<std::uint8_t, 6> peerAddress = {};
};

class IOriginalGattsNotify {
  public:
    virtual ~IOriginalGattsNotify() = default;
    virtual bool originalLive(const GattsRouteOrigin &) const noexcept = 0;
    // Recheck the same origin at the actual SDK submission boundary. This is
    // local posting, never peer delivery or a physical test result.
    virtual bool notifyOriginal(const GattsRouteOrigin &, std::uint16_t attribute,
                                const std::uint8_t *, std::size_t) noexcept = 0;
};

// Caller serializes IdentityAccess, diagnostic owner and SDK lifecycle. No
// product caller or bearer exists yet; this path cannot grant a session.
class DiagnosticGattsRoute {
  public:
    DiagnosticGattsRoute(companion::IdentityAccess &access, IOriginalGattsNotify &bearer)
        : access_(access), bearer_(bearer) {}
    bool notify(const GattsRouteOrigin &, std::uint16_t attribute,
                const std::uint8_t *, std::size_t count, std::uint16_t mtu) noexcept;

  private:
    companion::IdentityAccess &access_;
    IOriginalGattsNotify &bearer_;
};
} // namespace diagnostic
} // namespace larp
