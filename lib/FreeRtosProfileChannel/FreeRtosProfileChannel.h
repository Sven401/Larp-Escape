#pragma once
#include "DeviceRuntime/Channel.h"
#include "DeviceRuntime/Lifecycle.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace larp {
namespace devices {
enum class Completion : std::uint8_t { Invalid, Completed, ReplyDropped };
struct ChannelSnapshot {
    std::uint32_t outstanding = 0, rejectedCommands = 0, droppedReplies = 0, rejectedControls = 0,
                  droppedControlReplies = 0;
    std::uint8_t parkedOwners = 0;
};
// Process-lifetime storage; no heap queue creation/deletion. One application
// producer and exactly one worker per Owner. No ISR calls. Methods never wait.
// begin() precedes all tasks. Worker lifecycle calls must not overlap for an
// owner. Drivers must stop/cancel autonomous activity BEFORE parking that owner.
class FreeRtosProfileChannel : public IChannel, public IControlChannel {
  public:
    FreeRtosProfileChannel() = default;
    FreeRtosProfileChannel(const FreeRtosProfileChannel &) = delete;
    FreeRtosProfileChannel &operator=(const FreeRtosProfileChannel &) = delete;
    bool begin();
    bool sendControl(const Control &control) noexcept override;
    bool receiveControlReply(ControlReply &reply) noexcept override;
    bool takeControl(Owner owner, Control &control);
    Completion completeControl(Owner owner, const ControlReply &reply);
    bool send(Owner owner, const Command &command) noexcept override;
    bool receiveReply(Reply &reply) noexcept override;
    bool observe(std::uint8_t componentSlot, DeviceObservation &value) noexcept override;
    bool quiescent() const noexcept override;
    ChannelSnapshot snapshot() const;
    // Setup or sole worker only. Unpark after adopting immutable driver config
    // and generation. Parking requires all this owner's commands completed.
    bool resumeOwner(Owner owner, std::uint32_t generation);
    bool parkOwner(Owner owner);
    bool take(Owner owner, Command &command);
    // A worker retains its in-flight ticket until complete; a second take fails.
    // Read replies must match it and have a terminal outcome. Queue overflow
    // completes the job with ReplyDropped; Broker later times out explicitly.
    Completion complete(Owner owner, const Ticket &ticket, const Reply *reply = nullptr);
    // Sole component owner publishes cached availability/busy/wake values. No UID.
    bool publish(std::uint8_t componentSlot, const DeviceObservation &value);

  private:
    static const std::uint8_t AllParked = (1U << ChannelLimits::Owners) - 1U;
    struct Worker {
        Ticket activeTicket;
        ControlTicket activeControl;
        bool controlling = false;
        CommandKind activeKind = CommandKind::ReadCard;
        std::uint32_t generation = 0, outstanding = 0;
        bool executing = false;
    };
    std::array<Worker, ChannelLimits::Owners> workers_ = {};
    std::array<QueueHandle_t, ChannelLimits::Owners> commands_ = {};
    std::array<StaticQueue_t, ChannelLimits::Owners> commandControls_ = {};
    alignas(Command) std::uint8_t
        commandBytes_[ChannelLimits::Owners][ChannelLimits::CommandsPerOwner * sizeof(Command)] =
            {};
    QueueHandle_t replies_ = nullptr;
    StaticQueue_t replyControl_ = {};
    alignas(Reply) std::uint8_t replyBytes_[ChannelLimits::Replies * sizeof(Reply)] = {};
    std::array<QueueHandle_t, Limits::Components> observations_ = {};
    std::array<StaticQueue_t, Limits::Components> observationControls_ = {};
    alignas(DeviceObservation) std::uint8_t
        observationBytes_[Limits::Components][sizeof(DeviceObservation)] = {};
    std::array<QueueHandle_t, ChannelLimits::Owners> controls_ = {};
    std::array<StaticQueue_t, ChannelLimits::Owners> controlControls_ = {};
    alignas(Control) std::uint8_t controlBytes_[ChannelLimits::Owners][sizeof(Control)] = {};
    QueueHandle_t controlReplies_ = nullptr;
    StaticQueue_t controlReplyControl_ = {};
    alignas(ControlReply) std::uint8_t
        controlReplyBytes_[ChannelLimits::Owners * sizeof(ControlReply)] = {};
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    ChannelSnapshot status_;
    bool started_ = false;
};
} // namespace devices
} // namespace larp
