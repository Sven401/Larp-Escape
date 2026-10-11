#pragma once
#include "Companion/IdentityAccess.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace larp {
namespace diagnostic {
// Internal v1 values only; no wire decoder or trusted metadata in client JSON.
struct Uuid {
    std::array<std::uint8_t, 16> bytes = {};
    bool valid() const noexcept;
    bool operator==(const Uuid &other) const noexcept {
        return bytes == other.bytes;
    }
};
enum class Operation : std::uint8_t { WakeCapture, LedSingle };
enum class Outcome : std::uint8_t { Completed, Failed, Cancelled, Rejected };
enum class Reason : std::uint8_t {
    None,
    OwnerError,
    Unsupported,
    PermissionDenied,
    Expired,
    Busy,
    Cancelled,
    Overflow
};
struct Request {
    std::uint8_t schemaVersion = 1;
    Uuid runId, grantId;
    companion::Identity deviceId;
    std::uint64_t bootId = 0;
    std::uint32_t stepId = 0, timeoutMs = 1000;
    Operation operation = Operation::WakeCapture;
    std::uint8_t channel = 1, index = 0;
    std::array<std::uint8_t, 3> rgb = {{16, 0, 0}};
    std::uint16_t holdMs = 500;
};
struct Edge {
    std::uint64_t deviceMs = 0;
    std::uint8_t channel = 0;
    bool rawHigh = false, active = false;
};
struct Reply {
    Request request;
    std::uint64_t startedDeviceMs = 0, endedDeviceMs = 0;
    Outcome outcome = Outcome::Rejected;
    Reason reason = Reason::None;
    std::array<Edge, 64> edges = {};
    std::size_t edgeCount = 0;
    bool overflow = false, activeHigh = true, restoredOff = false;
    // Counts Control calls; an independent timer callback is not included.
    std::uint8_t renderCalls = 0;
    // Supply voltage is unavailable in this narrow port, mapped to JSON null.
};
struct Grant {
    Uuid runId, grantId;
    std::uint64_t issuedDeviceMs = 0, expiresDeviceMs = 0;
};
// Fake/component owner interface: acquire must atomically require idle game and
// exclusive components, block game start and swallow test input until release.
// grantId is freshly minted by the device, never selected by the client.
class IPort {
  public:
    virtual ~IPort() = default;
    virtual bool acquire(Uuid &grantId, bool &activeHigh) noexcept = 0;
    // Arm before render. The sole LED owner must independently force off no
    // later than holdMs after this call, even if Control never ticks again.
    // A late/in-flight render must not relight after expiry. A failed callback
    // must latch fault; restoreOff then returns false.
    virtual bool armLedOffTimer(std::uint16_t holdMs) noexcept = 0;
    virtual bool render(std::uint8_t index, const std::array<std::uint8_t, 3> &rgb) noexcept = 0;
    // True only after the independent timer has forced and confirmed off
    // within its deadline. A late Control.tick cannot substitute for this.
    virtual bool timedOffConfirmed() noexcept = 0;
    // Confirm both off and no prior timed-off failure. Also disarm the timer.
    virtual bool restoreOff() noexcept = 0;
    virtual void release() noexcept = 0;
};
enum class Admission : std::uint8_t {
    Accepted,
    PermissionDenied,
    Invalid,
    Busy,
    Consumed,
    Expired,
    OwnerError,
    Capacity
};
// One boot-lifetime owner. Caller MUST serialize this owner, IdentityAccess and
// all relevant lifecycle changes with component execution; route() alone is not
// a cross-thread fence. No production adapter is installed by this package.
class Control {
  public:
    Control(companion::IdentityAccess &access, IPort &port, const companion::Identity &device,
            std::uint64_t bootId) noexcept;
    Control(const Control &) = delete;
    Control &operator=(const Control &) = delete;
    ~Control();
    Admission authorize(const Uuid &run, const companion::BoundConnection &, bool explicitConsent,
                        std::uint32_t ttlMs, std::uint64_t now, Grant &destination) noexcept;
    Admission submit(const Request &, const companion::BoundConnection &,
                     std::uint64_t now) noexcept;
    // Sole component owner supplies raw edges, including all foreign channels.
    void edge(std::uint8_t channel, bool rawHigh, std::uint64_t now) noexcept;
    void finishWake(std::uint64_t now) noexcept;
    void tick(std::uint64_t now) noexcept;
    void cancel(const companion::BoundConnection &, std::uint64_t now) noexcept;
    void disconnect(const companion::BoundConnection &, std::uint64_t now) noexcept;
    void retire(std::uint64_t now) noexcept;
    // A retained reply is never delivered to a replacement connection. Transport
    // must still fence its original authority before sending; lost link stays unclear.
    bool takeReply(const companion::BoundConnection &, Reply &destination) noexcept;
    bool granted() const noexcept {
        return granted_;
    }
    bool pending() const noexcept {
        return pending_;
    }

  private:
    static const std::size_t Runs = 128;
    companion::IdentityAccess &access_;
    IPort &port_;
    companion::Identity device_;
    std::uint64_t bootId_, lastNow_ = 0, deadline_ = 0;
    companion::BoundConnection binding_, replyBinding_;
    companion::TrustedAuthority authority_;
    Grant grant_;
    Reply result_;
    std::array<Uuid, Runs> runs_ = {}, grants_ = {};
    std::size_t used_ = 0;
    std::uint32_t lastStep_ = 0;
    bool granted_ = false, pending_ = false, replyReady_ = false, ledAttempted_ = false;
    bool owned_ = false, clockFault_ = false, cleanupFault_ = false;
    bool current() const noexcept;
    bool clock(std::uint64_t now) noexcept;
    bool off() noexcept;
    void finish(Outcome, Reason, std::uint64_t now) noexcept;
    void close(Reason, std::uint64_t now) noexcept;
};
} // namespace diagnostic
} // namespace larp
