#pragma once
#include "DeviceRuntime/Plan.h"
#include "GameExecution/Session.h"
#include <type_traits>
namespace larp {
namespace devices {
using execution::ActionOutcome;
using execution::PollOutcome;
enum class CommandKind : std::uint8_t {
    ReadCard,
    ReadCrystals,
    PlayTrack,
    WakeAudio,
    ApplyEffect,
    SendActivation
};
struct Ticket {
    std::uint32_t generation = 0, sequence = 0, profileRevision = 0, stateRevision = 0;
    ComponentId component;
    configuration::ActionId requester;
};
bool sameTicket(const Ticket &first, const Ticket &second);
struct Command {
    Ticket ticket;
    CommandKind kind = CommandKind::ReadCard;
    execution::PollRequest poll;
    execution::ActionContext context;
    configuration::Effect effect; // Resolved value copy, including all 16 colors.
    std::uint16_t track = 0;
};
struct Reply {
    Ticket ticket;
    CommandKind kind = CommandKind::ReadCard;
    // Owner replies are terminal only: Fresh, Rejected, Unavailable.
    PollOutcome outcome = PollOutcome::Unavailable;
    // Owner monotonic wall clock when the bounded operation completed.
    std::uint32_t completedMilliseconds = 0;
    domain::CardUid card = {};
    ports::CrystalInputBatch crystals;
};
struct DeviceObservation {
    execution::DeviceSample sample;
    std::uint32_t generation = 0, processedSequence = 0;
    ActionOutcome lastOutcome = ActionOutcome::Unavailable;
};
static_assert(std::is_trivially_copyable<Command>::value, "Queue command must be a value copy");
static_assert(std::is_trivially_copyable<Reply>::value, "Queue reply must be a value copy");
static_assert(std::is_trivially_copyable<DeviceObservation>::value, "Mailbox must be a value copy");
class IChannel {
  public:
    virtual ~IChannel() = default;
    // Application producer, nonblocking. Exactly one FIFO per owner.
    // Implementations copy values and never call back into Broker/Session.
    virtual bool send(Owner owner, const Command &command) noexcept = 0;
    virtual bool receiveReply(Reply &reply) noexcept = 0;
    virtual bool observe(std::uint8_t componentSlot, DeviceObservation &value) noexcept = 0;
    // True only when no queued OR executing job remains. Empty queues alone
    // are insufficient. Producers/session must already be stopped for rebind.
    virtual bool quiescent() const noexcept = 0;
};
} // namespace devices
} // namespace larp
