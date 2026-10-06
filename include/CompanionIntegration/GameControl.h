#pragma once
#include "Companion/ConfiguredProtocol.h"
#include "DeviceRuntime/ProfileApplication.h"
namespace larp {
namespace integration {
struct ControlSnapshot {
    std::uint32_t lease = 0, nextRequest = 0, sample = 0, droppedReplies = 0, droppedStatus = 0;
    bool initialized = false;
};
// Outer integration layer, sole application context. service does NOT tick the
// application. Call application.service(now) exactly once, then this service.
// begin requires a freshly generated nonzero boot identity, stable for this boot.
// Never recreate this object or reuse a boot identity to reset replay protection.
class GameControl {
  public:
    GameControl(devices::ProfileApplication &application, companion::IGameControlChannel &channel)
        : application_(application), channel_(channel) {}
    GameControl(const GameControl &) = delete;
    GameControl &operator=(const GameControl &) = delete;
    bool begin(companion::BootIdentity boot);
    // Trusted sole-owner admission after authentication. One GM lease per boot;
    // replacing it invalidates old requests. principal is not a client role flag.
    // Returns 0 on failure. No implicit grant, no lease renewal in a callback.
    std::uint32_t grant(std::uint32_t authenticatedPrincipal);
    void revoke();
    void service(std::uint32_t now);
    ControlSnapshot snapshot() const;

  private:
    struct Receipt {
        companion::GameCommand command;
        companion::GameReply reply;
    };
    devices::ProfileApplication &application_;
    companion::IGameControlChannel &channel_;
    companion::BootIdentity boot_;
    // Sole-owner reusable publication value; channels copy before returning.
    // Trades bounded static RAM for a smaller FreeRTOS application stack frame.
    companion::ConfiguredStatus status_;
    std::array<Receipt, companion::ControlLimits::Receipts> receipts_ = {};
    std::uint32_t principal_ = 0, leaseCounter_ = 0, lease_ = 0, lastRequest_ = 0;
    std::uint32_t sample_ = 0, lastPublished_ = 0, droppedReplies_ = 0, droppedStatus_ = 0;
    bool initialized_ = false, operating_ = false, published_ = false;
    companion::GameReply dispatch(const companion::TrustedGameRequest &request, std::uint32_t now);
    void publish(std::uint32_t now);
};
} // namespace integration
} // namespace larp
