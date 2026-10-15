#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace ledwire {
using Nonce = std::array<std::uint8_t, 16>;
using Secret = std::array<std::uint8_t, 32>;
struct Message {
    std::array<std::uint8_t, 128> bytes = {};
    std::uint16_t size = 0;
};
enum class Operation : std::uint8_t { Start = 1, Stop = 2 };
enum class Effect : std::uint8_t { Off = 0, Twinkle = 1, Burst = 2, Ripple = 3 };
enum class Mode : std::uint8_t { LedOnly = 0, Stopped = 1, Running = 2, Paused = 3, Unknown = 4 };
enum Flags : std::uint8_t { Ready = 1, Pending = 2, Off = 4, Completed = 8, Fault = 16 };
enum class Outcome : std::uint8_t {
    Applied = 0, GameActive = 1, StaleRevision = 2, InvalidCommand = 3,
    Unavailable = 4, Conflict = 5, OutOfOrder = 6, Exhausted = 7,
    Expired = 8, Busy = 9, DriverFault = 10, Superseded = 11
};
struct Snapshot {
    std::uint32_t revision = 0, appliedRevision = 0;
    Effect effect = Effect::Off;
    std::uint8_t flags = Off;
    Mode mode = Mode::Unknown;
    // Wake1..4 are raw HIGH on MCP B7/B6/B4/B3. Unknown never carries a mask.
    bool wakeSupported = false, wakeValid = false;
    std::uint8_t wakeMask = 0;
};
class IHmac256 {
  public:
    virtual ~IHmac256() = default;
    virtual bool calculate(const Secret &, const std::uint8_t *, std::size_t,
                           Secret &result) noexcept = 0;
};
class IControl {
  public:
    virtual ~IControl() = default;
    virtual Snapshot snapshot() const noexcept = 0;
    // Sole LED owner only. Acceptance returns its exact newly requested revision.
    // Queue admission is not Applied; Session waits for the driver observation.
    virtual bool submit(Operation, Effect, std::uint32_t expectedRevision,
                        std::uint32_t &targetRevision) noexcept = 0;
    virtual void stopOnDisconnect() noexcept = 0;
};
enum class SessionResult : std::uint8_t { Ignored, Reply, Accepted, Rejected };
class Session {
  public:
    Session(IHmac256 &crypto, IControl &control) : crypto_(crypto), control_(control) {}
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    bool configure(const Nonce &device, const Secret &secret) noexcept;
    // Cryptographically random nonzero boot and connection nonces are supplied
    // by the platform. A fresh connection nonce is required for EVERY BLE link.
    bool beginConnection(const Nonce &boot, const Nonce &connection,
                         std::uint32_t now) noexcept;
    void disconnect() noexcept;
    SessionResult receive(const Message &, std::uint32_t now, Message &reply) noexcept;
    bool service(std::uint32_t now, Message &reply) noexcept;
    bool publishStatus(std::uint32_t now, Message &reply) noexcept;
    bool authenticated() const noexcept { return phase_ == Phase::Authenticated; }
    bool connected() const noexcept { return phase_ != Phase::Closed; }
    bool pending() const noexcept { return pending_; }
    std::uint32_t nextRequest() const noexcept { return nextRequest_; }

  private:
    enum class Phase : std::uint8_t { Closed, Hello, Proof, Authenticated };
    IHmac256 &crypto_;
    IControl &control_;
    Secret secret_ = {};
    Nonce device_ = {}, boot_ = {}, connection_ = {}, client_ = {}, session_ = {};
    std::array<std::uint8_t, 64> context_ = {};
    Message challenge_, lastCommand_, lastReply_, pendingCommand_;
    Phase phase_ = Phase::Closed;
    bool configured_ = false, pending_ = false;
    std::uint32_t connectedAt_ = 0, nextRequest_ = 1, statusSequence_ = 0;
    std::uint32_t pendingRequest_ = 0, pendingRevision_ = 0, pendingAt_ = 0, lastRequest_ = 0;
    bool tag(const char *, const std::uint8_t *, std::size_t, Secret &) noexcept;
    bool verify(const char *, const Message &, std::size_t) noexcept;
    bool status(std::uint8_t type, std::uint32_t now, Message &) noexcept;
    bool result(Outcome, std::uint32_t request, std::uint32_t now, Message &) noexcept;
    bool finish(Outcome, const Message &, std::uint32_t now, Message &) noexcept;
    SessionResult reject(Message &) noexcept;
};
enum class FrameResult : std::uint8_t { Receiving, Complete, Duplicate, Rejected };
class Assembler {
  public:
    void reset() noexcept;
    FrameResult accept(const std::uint8_t *, std::size_t, std::uint32_t now,
                       Message &) noexcept;
  private:
    Message current_;
    std::uint16_t transfer_ = 0, completed_ = 0, received_ = 0;
    std::uint16_t previousOffset_ = 0, previousSize_ = 0;
    std::uint32_t started_ = 0;
};
// Sequential nonwrapping transfer IDs belong to one BLE connection. The sender
// must serialize all fragments of each immutable message before the next one.
bool encodeFrame(const Message &, std::uint16_t transfer, std::uint16_t offset,
                 std::array<std::uint8_t, 20> &, std::size_t &written) noexcept;
std::uint32_t read32(const std::uint8_t *) noexcept;
void write32(std::uint8_t *, std::uint32_t) noexcept;
} // namespace ledwire
} // namespace larp
