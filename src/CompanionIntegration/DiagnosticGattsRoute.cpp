#include "CompanionIntegration/DiagnosticGattsRoute.h"
namespace larp {
namespace diagnostic {
namespace {
std::array<std::uint8_t, 16> uuid(std::uint8_t last) noexcept {
    return {{0x40, 0x7e, 0x6a, 0x1d, 0x1c, 0xc2, 0x43, 0xfd,
             0x85, 0x6b, 0x4a, 0xff, 0x85, 0xa2, 0x00, last}};
}
} // namespace
std::array<std::uint8_t, 16> GattsRouteV1::service() noexcept { return uuid(0x10); }
std::array<std::uint8_t, 16> GattsRouteV1::controlWrite() noexcept { return uuid(0x11); }
std::array<std::uint8_t, 16> GattsRouteV1::controlReply() noexcept { return uuid(0x12); }
std::array<std::uint8_t, 16> GattsRouteV1::stepWrite() noexcept { return uuid(0x13); }
std::array<std::uint8_t, 16> GattsRouteV1::stepReply() noexcept { return uuid(0x14); }
bool DiagnosticGattsRoute::notify(const GattsRouteOrigin &origin, std::uint16_t attribute,
                                  const std::uint8_t *bytes, std::size_t count,
                                  std::uint16_t mtu) noexcept {
    if (!attribute || !bytes || !count || count > GattsRouteV1::AttValueBytes ||
        count > static_cast<std::size_t>(mtu >= 3 ? mtu - 3 : 0) ||
        !GattsRouteV1::mtuSufficient(mtu) || !origin.sdkGeneration ||
        origin.interface == 0xff || !origin.connection.token || !origin.authority.epoch ||
        !origin.authority.principal || !origin.authority.gameMaster ||
        !bearer_.originalLive(origin))
        return false;
    companion::TrustedAuthority current;
    return access_.route(origin.connection, current) && current == origin.authority &&
           bearer_.notifyOriginal(origin, attribute, bytes, count);
}
} // namespace diagnostic
} // namespace larp
