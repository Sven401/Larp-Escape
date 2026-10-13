#pragma once
#include "Companion/AuthorityState.h"
#include "GameConfiguration/Profile.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace larp {
namespace companion {
struct BootIdentity {
    std::uint32_t high = 0, low = 0;
    bool valid() const {
        return high != 0 || low != 0;
    }
    bool operator==(const BootIdentity &other) const {
        return high == other.high && low == other.low;
    }
};
struct ControlLimits {
    static const std::size_t Receipts = 8, Drain = 4, Queue = 4;
    static const std::uint32_t PublishMilliseconds = 250;
};
enum class GameCommandKind : std::uint8_t {
    Start,
    Stop,
    Pause,
    Resume,
    ChangeState,
    TakeTransition
};
struct GameCommand {
    BootIdentity boot;
    std::uint32_t lease = 0, request = 0;
    std::uint32_t expectedConfigurationRevision = 0, expectedStateRevision = 0;
    GameCommandKind kind = GameCommandKind::Start;
    // Only ChangeState / TakeTransition use a nonzero, profile-owned ID.
    std::uint16_t target = 0;
};
// principal + connectionEpoch are verified connection metadata, NEVER client
// JSON fields. A transport attaches both from that specific verified connection.
struct TrustedGameRequest {
    std::uint32_t principal = 0, connectionEpoch = 0;
    GameCommand command;
};
enum class GameReplyOutcome : std::uint8_t {
    Executed,
    ExecutionRejected,
    InvalidRequest,
    WrongBoot,
    Unauthorized,
    StaleConfiguration,
    StaleState,
    OutOfOrder,
    ReceiptExpired,
    RequestConflict,
    NotReady
};
struct GameReply {
    BootIdentity boot;
    std::uint32_t lease = 0, request = 0, nextRequest = 1;
    std::uint32_t configurationRevision = 0, stateRevision = 0;
    GameReplyOutcome outcome = GameReplyOutcome::NotReady;
    configuration::Text<32> executionOutcome;
    bool duplicate = false, consumed = false;
};
struct RoutedGameReply {
    std::uint32_t principal = 0, connectionEpoch = 0;
    GameReply reply;
};
enum class ConfiguredPhase : std::uint8_t {
    Empty,
    PendingActivation,
    Applying,
    Saving,
    Ready,
    Fault
};
struct ConfiguredDeviceStatus {
    std::uint16_t id = 0;
    bool enabled = false, available = false, cardReadable = true, matrixReadable = true,
         pollPending = false;
    configuration::Text<16> lastOutcome;
};
struct ConfiguredStatus {
    BootIdentity boot;
    std::uint32_t sample = 0, sampledMilliseconds = 0;
    ConfiguredPhase phase = ConfiguredPhase::Empty;
    std::uint32_t configurationRevision = 0, profileRevision = 0, draftRevision = 0;
    std::uint32_t deviceGeneration = 0, storedGeneration = 0, storedProfileRevision = 0;
    bool hasActive = false, draftReady = false, hardwareReady = false, storedMatchesActive = false;
    // No principal, authentication material, raw UID or hardware address is published.
    std::uint32_t controlLease = 0, nextRequest = 0;
    bool running = false, paused = false, entryPending = false, rolePresent = false;
    std::uint16_t state = 0, lastTransition = 0, role = 0;
    std::uint32_t stateRevision = 0, stateElapsedMilliseconds = 0, roundCount = 0;
    std::uint8_t crystalCount = 0, requiredCrystals = 0, seenRoleCount = 0;
    bool audioPending = false, incorrectCrystal = false, keystoneError = false;
    std::uint32_t rejectedActions = 0, unavailableActions = 0;
    configuration::Text<32> lastTickOutcome;
    std::array<ConfiguredDeviceStatus, configuration::Limits::Components> devices = {};
    std::uint8_t deviceCount = 0;
};
// Producer's observed verified connection, not metadata attached at read time.
// Status JSON is unchanged. An old lease must never be relabeled to a new epoch.
struct RoutedConfiguredStatus {
    std::uint32_t principal = 0, connectionEpoch = 0;
    ConfiguredStatus status;
};
// Copy-only, bounded, zero-wait application ports. Sole application owner calls
// receive/send/publish. Transport tasks must use a synchronized adapter, never
// touch the controller. Route replies only to a currently matching verified
// principal AND connectionEpoch; never a recycled principal/new connection.
class IGameControlChannel {
  public:
    virtual ~IGameControlChannel() = default;
    // Latest synchronized verified transport state; false closes authority.
    virtual bool readAuthority(TrustedAuthority &authority) const noexcept = 0;
    virtual bool receive(TrustedGameRequest &request) noexcept = 0;
    virtual bool send(const RoutedGameReply &reply) noexcept = 0;
    virtual bool publish(const RoutedConfiguredStatus &status) noexcept = 0;
};
constexpr std::size_t maxConfiguredStatusJsonBytes = 6144;
constexpr std::size_t maxGameReplyJsonBytes = 512;
constexpr std::size_t maxGameCommandJsonBytes = 512; // Canonical output including NUL.
enum class ConfiguredEncodeResult : std::uint8_t { Success, InvalidValue, BufferTooSmall };
const char *configuredPhaseName(ConfiguredPhase value);
const char *gameReplyOutcomeName(GameReplyOutcome value);
const char *gameCommandKindName(GameCommandKind value);
// written excludes NUL. Any failure clears written and the first output byte.
ConfiguredEncodeResult encodeConfiguredStatus(const ConfiguredStatus &status, char *buffer,
                                              std::size_t capacity, std::size_t &written);
ConfiguredEncodeResult encodeGameCommand(const GameCommand &command, char *buffer,
                                         std::size_t capacity, std::size_t &written);
ConfiguredEncodeResult encodeGameReply(const GameReply &reply, char *buffer, std::size_t capacity,
                                       std::size_t &written);
} // namespace companion
} // namespace larp
