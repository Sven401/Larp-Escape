#pragma once
// Derived from the reviewed synthetic owners in src/ProfileTool/Activation.cpp.
// Copied control/data FIFO, real OwnerConfiguration, synthetic observations only.
#include "DeviceRuntime/Channel.h"
#include "DeviceRuntime/OwnerConfiguration.h"
#include <cstdio>
using namespace larp::devices;
using namespace larp::configuration;
template <typename T, std::size_t N> struct Fifo {
    std::array<T, N> values = {};
    std::size_t head = 0, count = 0;
    bool push(const T &value) {
        if (count == N)
            return false;
        values[(head + count) % N] = value;
        ++count;
        return true;
    }
    bool pop(T &value) {
        if (!count)
            return false;
        value = values[head];
        head = (head + 1) % N;
        --count;
        return true;
    }
};
struct Worker {
    explicit Worker(Owner owner) : settings(owner), owner(owner) {}
    OwnerConfiguration settings;
    Owner owner;
    Fifo<Command, ChannelLimits::CommandsPerOwner> commands;
    Fifo<Control, 1> controls;
    std::uint32_t generation = 0;
    bool running = false;
};
class Owners : public IChannel, public IControlChannel {
  public:
    Owners() : workers_{{&nfc_, &i2c_, &audio_, &led_, &peer_}} {}
    larp::domain::CardUid card = {};
    void staleObservations() {
        for (auto &value : observations_)
            value.generation = 0;
    }
    bool send(Owner owner, const Command &command) noexcept override {
        const unsigned index = static_cast<unsigned>(owner);
        return index < workers_.size() && workers_[index]->running &&
               command.ticket.generation == workers_[index]->generation &&
               workers_[index]->commands.push(command);
    }
    bool receiveReply(Reply &reply) noexcept override {
        return replies_.pop(reply);
    }
    bool observe(std::uint8_t slot, DeviceObservation &value) noexcept override {
        if (slot >= Limits::Components)
            return false;
        value = observations_[slot];
        return true;
    }
    bool quiescent() const noexcept override {
        if (processing_)
            return false;
        for (const auto *worker : workers_)
            if (worker->running || worker->commands.count)
                return false;
        return true;
    }
    bool sendControl(const Control &control) noexcept override {
        const unsigned index = static_cast<unsigned>(control.ticket.owner);
        return index < workers_.size() && workers_[index]->controls.push(control);
    }
    bool receiveControlReply(ControlReply &reply) noexcept override {
        return controlReplies_.pop(reply);
    }
    void pump(std::uint32_t now) {
        processing_ = true;
        for (auto *worker : workers_) {
            Control control;
            if (worker->controls.pop(control)) {
                bool applied = false;
                switch (control.ticket.kind) {
                case ControlKind::Park: {
                    worker->running = false;
                    Command command;
                    for (std::size_t step = 0;
                         step < ChannelLimits::CommandsPerOwner && worker->commands.pop(command);
                         ++step) {
                        ++cancelled_;
                        complete(*worker, command, now, true);
                    }
                    applied = worker->commands.count == 0;
                    break;
                }
                case ControlKind::Begin:
                    applied = !worker->running && worker->settings.begin(control);
                    break;
                case ControlKind::Component:
                    applied = !worker->running && worker->settings.append(control);
                    break;
                case ControlKind::Commit:
                    applied = !worker->running && worker->settings.commit(control);
                    break;
                case ControlKind::Resume:
                    applied = !worker->running && worker->settings.ready(control.ticket.generation);
                    if (applied) {
                        worker->generation = control.ticket.generation;
                        worker->running = true;
                    }
                    break;
                }
                ControlReply reply;
                reply.ticket = control.ticket;
                reply.completedMilliseconds = now;
                reply.outcome = applied ? ControlOutcome::Applied : ControlOutcome::Rejected;
                if (!controlReplies_.push(reply))
                    ++dropped_;
            }
            if (!worker->running)
                continue;
            Command command;
            if (worker->commands.pop(command))
                complete(*worker, command, now, false);
            for (std::uint8_t index = 0; index < worker->settings.count(); ++index) {
                const auto &component = *worker->settings.get(index);
                auto &observation = observations_[component.slot];
                observation.generation = worker->generation;
                observation.sample.available = true;
                observation.sample.wakes.fill(true);
            }
        }
        processing_ = false;
    }
    void report() const {
        std::size_t pending = 0;
        for (const auto *worker : workers_)
            pending += worker->commands.count;
        std::printf("{\"event\":\"owners\",\"queuedCommands\":%lu,\"cancelled\":%lu,"
                    "\"droppedReplies\":%lu}\n",
                    static_cast<unsigned long>(pending), static_cast<unsigned long>(cancelled_),
                    static_cast<unsigned long>(dropped_));
    }

  private:
    Worker nfc_{Owner::Nfc}, i2c_{Owner::I2c}, audio_{Owner::Audio}, led_{Owner::Led},
        peer_{Owner::Peer};
    std::array<Worker *, ChannelLimits::Owners> workers_;
    std::array<DeviceObservation, Limits::Components> observations_ = {};
    Fifo<Reply, ChannelLimits::Replies> replies_;
    Fifo<ControlReply, ChannelLimits::Owners> controlReplies_;
    std::uint32_t cancelled_ = 0, dropped_ = 0;
    bool processing_ = false;
    void complete(Worker &worker, const Command &command, std::uint32_t now, bool cancelled) {
        const auto *component = worker.settings.find(command.ticket.component);
        const bool available =
            !cancelled && component && command.ticket.generation == worker.generation;
        if (component) {
            auto &observation = observations_[component->slot];
            observation.processedSequence = command.ticket.sequence;
            observation.lastOutcome =
                available ? ActionOutcome::Applied : ActionOutcome::Unavailable;
        }
        if (command.kind == CommandKind::ReadCard || command.kind == CommandKind::ReadCrystals) {
            Reply reply;
            reply.ticket = command.ticket;
            reply.kind = command.kind;
            reply.completedMilliseconds = now;
            reply.outcome = available ? PollOutcome::Fresh : PollOutcome::Unavailable;
            if (command.kind == CommandKind::ReadCard && available)
                reply.card = card;
            if (!replies_.push(reply))
                ++dropped_;
        }
    }
};
