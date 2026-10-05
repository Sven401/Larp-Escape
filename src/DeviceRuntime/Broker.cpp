#include "DeviceRuntime/Broker.h"
#include <limits>
namespace larp {
namespace devices {
namespace {
void increment(std::uint32_t &value) {
    if (value != std::numeric_limits<std::uint32_t>::max())
        ++value;
}
bool accepts(ComponentKind component, CommandKind command) {
    switch (command) {
    case CommandKind::ReadCard:
        return component == ComponentKind::NfcSpi;
    case CommandKind::ReadCrystals:
        return component == ComponentKind::Matrix;
    case CommandKind::PlayTrack:
    case CommandKind::WakeAudio:
        return component == ComponentKind::AudioUart;
    case CommandKind::ApplyEffect:
        return component == ComponentKind::LedStrip;
    case CommandKind::SendActivation:
        return component == ComponentKind::EspNow;
    }
    return false;
}
Owner ownerFor(CommandKind kind) {
    switch (kind) {
    case CommandKind::ReadCard:
        return Owner::Nfc;
    case CommandKind::ReadCrystals:
        return Owner::I2c;
    case CommandKind::PlayTrack:
    case CommandKind::WakeAudio:
        return Owner::Audio;
    case CommandKind::ApplyEffect:
        return Owner::Led;
    case CommandKind::SendActivation:
        return Owner::Peer;
    }
    return Owner::Count;
}
} // namespace
bool sameTicket(const Ticket &a, const Ticket &b) {
    return a.generation == b.generation && a.sequence == b.sequence &&
           a.profileRevision == b.profileRevision && a.stateRevision == b.stateRevision &&
           a.component == b.component && a.requester == b.requester;
}
std::uint8_t Broker::slot(ComponentId id) const {
    if (configured_)
        for (std::uint8_t index = 0; index < plan_.routeCount; ++index)
            if (plan_.routes[index].id == id && plan_.routes[index].enabled)
                return index;
    return 255;
}
bool Broker::configure(const Plan &plan, std::uint32_t generation) {
    if (servicing_ || generation == 0 || generation <= status_.generation ||
        !channel_.quiescent() || plan.routeCount > Limits::Components ||
        plan.replyTimeoutMilliseconds == 0)
        return false;
    for (std::uint8_t index = 0; index < plan.routeCount; ++index)
        if (static_cast<unsigned>(plan.routes[index].owner) >= ChannelLimits::Owners ||
            plan.routes[index].id.value == 0)
            return false;
    plan_ = plan;
    pending_.fill(Pending());
    playbackReservations_.fill(0);
    status_.generation = generation;
    configured_ = true;
    return true;
}
void Broker::service() {
    if (servicing_ || !configured_)
        return;
    servicing_ = true;
    Reply reply;
    for (std::size_t count = 0;
         count < ChannelLimits::ReplyDrainPerService && channel_.receiveReply(reply); ++count) {
        const std::uint8_t index = slot(reply.ticket.component);
        if (index == 255 || !pending_[index].active || pending_[index].ready ||
            !sameTicket(reply.ticket, pending_[index].reply.ticket) ||
            reply.kind != pending_[index].reply.kind ||
            (reply.outcome != PollOutcome::Fresh && reply.outcome != PollOutcome::Rejected &&
             reply.outcome != PollOutcome::Unavailable)) {
            increment(status_.staleReplies);
            continue;
        }
        // A timely completion may be consumed on a later application tick.
        // A completion after its deadline cannot become a fresh input.
        if (static_cast<std::uint32_t>(reply.completedMilliseconds - pending_[index].issued) >=
            plan_.replyTimeoutMilliseconds) {
            reply.outcome = PollOutcome::Unavailable;
            increment(status_.timedOut);
        }
        pending_[index].reply = reply;
        pending_[index].ready = true;
        increment(status_.received);
    }
    servicing_ = false;
}
bool Broker::issue(Command &command) {
    const std::uint8_t index = slot(command.ticket.component);
    if (index == 255 || !accepts(plan_.routes[index].kind, command.kind) ||
        plan_.routes[index].owner != ownerFor(command.kind) ||
        status_.lastSequence == std::numeric_limits<std::uint32_t>::max()) {
        increment(status_.rejected);
        return false;
    }
    command.ticket.generation = status_.generation;
    command.ticket.profileRevision = plan_.profileRevision;
    command.ticket.sequence = ++status_.lastSequence;
    if (!channel_.send(plan_.routes[index].owner, command)) {
        increment(status_.rejected);
        return false;
    }
    increment(status_.accepted);
    return true;
}
bool Broker::sample(const configuration::Component &component,
                    execution::DeviceSample &value) noexcept {
    const std::uint8_t index = slot(component.id);
    DeviceObservation observation;
    if (index == 255 || component.kind != plan_.routes[index].kind ||
        !channel_.observe(index, observation) || observation.generation != status_.generation)
        return false;
    value = observation.sample;
    // A queued play is busy before the owner starts it. Release only after its
    // generation-tagged observation acknowledges processing that sequence.
    if (playbackReservations_[index] != 0) {
        if (observation.processedSequence >= playbackReservations_[index])
            playbackReservations_[index] = 0;
        else
            value.audioBusy = true;
    }
    return true;
}
PollOutcome Broker::poll(const execution::PollRequest &request, CommandKind kind, Reply &reply) {
    const std::uint8_t index = slot(request.component);
    if (index == 255 || !accepts(plan_.routes[index].kind, kind) ||
        plan_.routes[index].owner != ownerFor(kind) ||
        request.profileRevision != plan_.profileRevision || request.timeoutMilliseconds == 0)
        return PollOutcome::Unavailable;
    Pending &pending = pending_[index];
    if (pending.active) {
        const Ticket &ticket = pending.reply.ticket;
        if (ticket.profileRevision != request.profileRevision ||
            ticket.stateRevision != request.stateRevision)
            pending.active = false; // Old reply will be rejected by its complete ticket.
        else if (ticket.requester != request.requester || pending.reply.kind != kind)
            return PollOutcome::Rejected; // One logical read per physical component.
        else if (pending.ready) {
            reply = pending.reply;
            pending.active = false;
            return reply.outcome;
        } else if (static_cast<std::uint32_t>(request.nowMilliseconds - pending.issued) >=
                   plan_.replyTimeoutMilliseconds) {
            pending.active = false;
            increment(status_.timedOut);
            return PollOutcome::Unavailable;
        } else
            return PollOutcome::Pending;
    }
    Command command;
    command.kind = kind;
    command.poll = request;
    command.ticket.component = request.component;
    command.ticket.requester = request.requester;
    command.ticket.stateRevision = request.stateRevision;
    if (!issue(command))
        return PollOutcome::Rejected;
    pending.reply = Reply();
    pending.reply.ticket = command.ticket;
    pending.reply.kind = kind;
    pending.issued = request.nowMilliseconds;
    pending.active = true;
    pending.ready = false;
    return PollOutcome::Queued;
}
PollOutcome Broker::pollCard(const execution::PollRequest &request, domain::CardUid &uid) noexcept {
    Reply reply;
    const PollOutcome result = poll(request, CommandKind::ReadCard, reply);
    if (result == PollOutcome::Fresh)
        uid = reply.card;
    return result;
}
PollOutcome Broker::pollCrystals(const execution::PollRequest &request,
                                 ports::CrystalInputBatch &batch) noexcept {
    Reply reply;
    const PollOutcome result = poll(request, CommandKind::ReadCrystals, reply);
    if (result == PollOutcome::Fresh)
        batch = reply.crystals;
    return result;
}
ActionOutcome Broker::output(Command &command, Owner owner) {
    const std::uint8_t index = slot(command.ticket.component);
    if (index == 255 || plan_.routes[index].owner != owner)
        return ActionOutcome::Unavailable;
    return issue(command) ? ActionOutcome::Queued : ActionOutcome::Rejected;
}
ActionOutcome Broker::playTrack(ComponentId component, std::uint16_t track,
                                const execution::ActionContext &context) noexcept {
    if (track == 0)
        return ActionOutcome::Rejected;
    Command command;
    command.kind = CommandKind::PlayTrack;
    command.ticket.component = component;
    command.context = context;
    command.track = track;
    const ActionOutcome result = output(command, Owner::Audio);
    if (result == ActionOutcome::Queued)
        playbackReservations_[slot(component)] = command.ticket.sequence;
    return result;
}
ActionOutcome Broker::wakeAudio(ComponentId component,
                                const execution::ActionContext &context) noexcept {
    Command command;
    command.kind = CommandKind::WakeAudio;
    command.ticket.component = component;
    command.context = context;
    return output(command, Owner::Audio);
}
ActionOutcome Broker::applyEffect(const configuration::Effect &effect,
                                  configuration::ActionId binding,
                                  const execution::ActionContext &context) noexcept {
    Command command;
    command.kind = CommandKind::ApplyEffect;
    command.ticket.component = effect.component;
    command.ticket.requester = binding;
    command.context = context;
    command.effect = effect;
    return output(command, Owner::Led);
}
ActionOutcome Broker::sendActivation(ComponentId component,
                                     const execution::ActionContext &context) noexcept {
    Command command;
    command.kind = CommandKind::SendActivation;
    command.ticket.component = component;
    command.context = context;
    return output(command, Owner::Peer);
}
BrokerSnapshot Broker::snapshot() const {
    BrokerSnapshot value = status_;
    value.pendingReads = 0;
    for (const Pending &pending : pending_)
        if (pending.active)
            ++value.pendingReads;
    return value;
}
} // namespace devices
} // namespace larp
