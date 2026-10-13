#include "FreeRtosProfileChannel.h"
#include <limits>
namespace larp {
namespace devices {
namespace {
void increment(std::uint32_t &value) {
    if (value != std::numeric_limits<std::uint32_t>::max())
        ++value;
}
bool valid(Owner owner) {
    return static_cast<unsigned>(owner) < ChannelLimits::Owners;
}
} // namespace
bool FreeRtosProfileChannel::begin() {
    if (started_)
        return false;
    for (std::size_t index = 0; index < ChannelLimits::Owners; ++index)
        commands_[index] = xQueueCreateStatic(ChannelLimits::CommandsPerOwner, sizeof(Command),
                                              commandBytes_[index], &commandControls_[index]);
    for (std::size_t index = 0; index < ChannelLimits::Owners; ++index)
        controls_[index] =
            xQueueCreateStatic(1, sizeof(Control), controlBytes_[index], &controlControls_[index]);
    controlReplies_ = xQueueCreateStatic(ChannelLimits::Owners, sizeof(ControlReply),
                                         controlReplyBytes_, &controlReplyControl_);
    replies_ =
        xQueueCreateStatic(ChannelLimits::Replies, sizeof(Reply), replyBytes_, &replyControl_);
    for (std::size_t index = 0; index < Limits::Components; ++index)
        observations_[index] = xQueueCreateStatic(
            1, sizeof(DeviceObservation), observationBytes_[index], &observationControls_[index]);
    started_ = true;
    status_.parkedOwners = AllParked;
    return true;
}
bool FreeRtosProfileChannel::sendControl(const Control &control) noexcept {
    if (!started_ || !valid(control.ticket.owner) || control.ticket.generation == 0 ||
        control.ticket.sequence == 0)
        return false;
    if (xQueueSendToBack(controls_[static_cast<unsigned>(control.ticket.owner)], &control, 0) ==
        pdTRUE)
        return true;
    portENTER_CRITICAL(&mutex_);
    increment(status_.rejectedControls);
    portEXIT_CRITICAL(&mutex_);
    return false;
}
bool FreeRtosProfileChannel::receiveControlReply(ControlReply &reply) noexcept {
    return started_ && xQueueReceive(controlReplies_, &reply, 0) == pdTRUE;
}
bool FreeRtosProfileChannel::takeControl(Owner owner, Control &control) {
    if (!started_ || !valid(owner))
        return false;
    Worker &worker = workers_[static_cast<unsigned>(owner)];
    if (worker.controlling ||
        xQueueReceive(controls_[static_cast<unsigned>(owner)], &control, 0) != pdTRUE)
        return false;
    worker.activeControl = control.ticket;
    worker.controlling = true;
    return true;
}
Completion FreeRtosProfileChannel::completeControl(Owner owner, const ControlReply &reply) {
    if (!started_ || !valid(owner))
        return Completion::Invalid;
    Worker &worker = workers_[static_cast<unsigned>(owner)];
    if (!worker.controlling || !sameControlTicket(reply.ticket, worker.activeControl) ||
        (reply.outcome != ControlOutcome::Applied && reply.outcome != ControlOutcome::Rejected &&
         reply.outcome != ControlOutcome::Failed))
        return Completion::Invalid;
    const bool dropped = xQueueSendToBack(controlReplies_, &reply, 0) != pdTRUE;
    worker.controlling = false;
    if (dropped) {
        portENTER_CRITICAL(&mutex_);
        increment(status_.droppedControlReplies);
        portEXIT_CRITICAL(&mutex_);
    }
    return dropped ? Completion::ReplyDropped : Completion::Completed;
}
bool FreeRtosProfileChannel::send(Owner owner, const Command &command) noexcept {
    if (!started_ || !valid(owner))
        return false;
    const unsigned index = static_cast<unsigned>(owner);
    portENTER_CRITICAL(&mutex_);
    Worker &worker = workers_[index];
    const bool allowed = (status_.parkedOwners & (1U << index)) == 0 &&
                         command.ticket.generation == worker.generation &&
                         command.ticket.sequence != 0;
    if (allowed) {
        // Count before publishing; a fast worker must never complete an
        // uncounted command. Parking cannot race past this reservation.
        ++worker.outstanding;
        ++status_.outstanding;
    } else
        increment(status_.rejectedCommands);
    portEXIT_CRITICAL(&mutex_);
    if (!allowed)
        return false;
    if (xQueueSendToBack(commands_[index], &command, 0) == pdTRUE)
        return true;
    portENTER_CRITICAL(&mutex_);
    --worker.outstanding;
    --status_.outstanding;
    increment(status_.rejectedCommands);
    portEXIT_CRITICAL(&mutex_);
    return false;
}
bool FreeRtosProfileChannel::receiveReply(Reply &reply) noexcept {
    return started_ && xQueueReceive(replies_, &reply, 0) == pdTRUE;
}
bool FreeRtosProfileChannel::observe(std::uint8_t slot, DeviceObservation &value) noexcept {
    return started_ && slot < Limits::Components &&
           xQueuePeek(observations_[slot], &value, 0) == pdTRUE;
}
ChannelSnapshot FreeRtosProfileChannel::snapshot() const {
    portENTER_CRITICAL(&mutex_);
    const ChannelSnapshot result = status_;
    portEXIT_CRITICAL(&mutex_);
    return result;
}
bool FreeRtosProfileChannel::quiescent() const noexcept {
    const ChannelSnapshot value = snapshot();
    return started_ && value.outstanding == 0 && value.parkedOwners == AllParked;
}
bool FreeRtosProfileChannel::resumeOwner(Owner owner, std::uint32_t generation) {
    if (!started_ || !valid(owner) || generation == 0)
        return false;
    const unsigned index = static_cast<unsigned>(owner);
    portENTER_CRITICAL(&mutex_);
    Worker &worker = workers_[index];
    const bool allowed = (status_.parkedOwners & (1U << index)) != 0 && worker.outstanding == 0 &&
                         generation >= worker.generation;
    if (allowed) {
        worker.generation = generation;
        status_.parkedOwners &= static_cast<std::uint8_t>(~(1U << index));
    }
    portEXIT_CRITICAL(&mutex_);
    return allowed;
}
bool FreeRtosProfileChannel::parkOwner(Owner owner) {
    if (!started_ || !valid(owner))
        return false;
    const unsigned index = static_cast<unsigned>(owner);
    portENTER_CRITICAL(&mutex_);
    const bool allowed = workers_[index].outstanding == 0;
    if (allowed)
        status_.parkedOwners |= static_cast<std::uint8_t>(1U << index);
    portEXIT_CRITICAL(&mutex_);
    return allowed;
}
bool FreeRtosProfileChannel::take(Owner owner, Command &command) {
    if (!started_ || !valid(owner))
        return false;
    Worker &worker = workers_[static_cast<unsigned>(owner)];
    if (worker.executing ||
        xQueueReceive(commands_[static_cast<unsigned>(owner)], &command, 0) != pdTRUE)
        return false;
    worker.activeTicket = command.ticket;
    worker.activeKind = command.kind;
    worker.executing = true;
    return true;
}
Completion FreeRtosProfileChannel::complete(Owner owner, const Ticket &ticket, const Reply *reply) {
    if (!started_ || !valid(owner))
        return Completion::Invalid;
    Worker &worker = workers_[static_cast<unsigned>(owner)];
    if (!worker.executing || !sameTicket(worker.activeTicket, ticket))
        return Completion::Invalid;
    const bool read = worker.activeKind == CommandKind::ReadCard ||
                      worker.activeKind == CommandKind::ReadCrystals;
    if (read != (reply != nullptr) ||
        (reply &&
         (!sameTicket(reply->ticket, ticket) || reply->kind != worker.activeKind ||
          (reply->outcome != PollOutcome::Fresh && reply->outcome != PollOutcome::Rejected &&
           reply->outcome != PollOutcome::Unavailable))))
        return Completion::Invalid;
    const bool dropped = reply && xQueueSendToBack(replies_, reply, 0) != pdTRUE;
    worker.executing = false;
    portENTER_CRITICAL(&mutex_);
    --worker.outstanding;
    --status_.outstanding;
    if (dropped)
        increment(status_.droppedReplies);
    portEXIT_CRITICAL(&mutex_);
    return dropped ? Completion::ReplyDropped : Completion::Completed;
}
bool FreeRtosProfileChannel::publish(std::uint8_t slot, const DeviceObservation &value) {
    return started_ && slot < Limits::Components &&
           xQueueOverwrite(observations_[slot], &value) == pdTRUE;
}
} // namespace devices
} // namespace larp
