#include "Activation.h"
#include "DeviceRuntime/OwnerConfiguration.h"
#include "DeviceRuntime/ProfileController.h"
#include <cstdio>
#include <memory>
namespace {
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
                std::printf(
                    "{\"event\":\"owner_control\",\"kind\":\"%s\",\"owner\":\"%s\","
                    "\"generation\":%lu,\"sequence\":%lu,\"component\":%u,\"pin0\":%u,"
                    "\"ledCount\":%u,\"bus\":%u,\"address\":%u,\"volume\":%u,\"baud\":%lu,"
                    "\"applied\":%s}\n",
                    controlKindName(control.ticket.kind), ownerName(worker->owner),
                    static_cast<unsigned long>(control.ticket.generation),
                    static_cast<unsigned long>(control.ticket.sequence),
                    static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                              ? control.component.id.value
                                              : 0),
                    static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                              ? control.component.pins[0]
                                              : 255),
                    static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                              ? control.component.ledCount
                                              : 0),
                    static_cast<unsigned>(
                        control.ticket.kind == ControlKind::Component ? control.component.bus : 0),
                    static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                              ? control.component.address
                                              : 0),
                    static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                              ? control.component.volume
                                              : 0),
                    static_cast<unsigned long>(
                        control.ticket.kind == ControlKind::Component ? control.component.baud : 0),
                    applied ? "true" : "false");
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
            if (!replies_.push(reply))
                ++dropped_;
        }
    }
};
void report(const ProfileController &runtime) {
    const auto value = runtime.activation();
    const auto game = runtime.game();
    std::printf(
        "{\"event\":\"activation\",\"phase\":\"%s\",\"finish\":\"%s\",\"error\":\"%s\","
        "\"configurationRevision\":%lu,\"profileRevision\":%lu,\"generation\":%lu,"
        "\"workingGeneration\":%lu,"
        "\"hardwareReady\":%s,\"operations\":%lu,\"staleReplies\":%lu,\"queueRejections\":%lu,"
        "\"gameRunning\":%s,\"state\":%u,\"stateRevision\":%lu}\n",
        activationPhaseName(value.phase), activationFinishName(value.finish),
        activationErrorName(value.error), static_cast<unsigned long>(value.configurationRevision),
        static_cast<unsigned long>(value.profileRevision),
        static_cast<unsigned long>(value.generation),
        static_cast<unsigned long>(value.workingGeneration), value.hardwareReady ? "true" : "false",
        static_cast<unsigned long>(value.completedOperations),
        static_cast<unsigned long>(value.staleReplies),
        static_cast<unsigned long>(value.queueRejections),
        game.execution.running ? "true" : "false", game.execution.state.value,
        static_cast<unsigned long>(game.execution.stateRevision));
}
bool drive(Owners &owners, ProfileController &runtime, std::uint32_t &now) {
    for (unsigned step = 0; step < 256 && runtime.busy(); ++step) {
        owners.pump(++now);
        runtime.service(now);
    }
    report(runtime);
    owners.report();
    return !runtime.busy() && runtime.activation().hardwareReady &&
           runtime.activation().finish == ActivationFinish::Applied;
}
class FileReader : public IProfileReader {
  public:
    explicit FileReader(std::FILE *file) : file_(file) {}
    ReadStatus readByte(std::uint8_t &byte) override {
        const int value = std::fgetc(file_);
        if (value == EOF)
            return std::ferror(file_) ? ReadStatus::Failure : ReadStatus::End;
        byte = static_cast<std::uint8_t>(value);
        return ReadStatus::Byte;
    }

  private:
    std::FILE *file_;
};
class FileWriter : public IProfileWriter {
  public:
    explicit FileWriter(std::FILE *file) : file_(file) {}
    bool write(const char *bytes, std::size_t count) override {
        return std::fwrite(bytes, 1, count, file_) == count;
    }

  private:
    std::FILE *file_;
};
void reportStorage(const ProfileController &controller) {
    const auto value = controller.storage();
    std::printf("{\"event\":\"profile_storage\",\"draftRevision\":%lu,"
                "\"activeProfileRevision\":%lu,\"draftProfileRevision\":%lu,"
                "\"hasActive\":%s,\"draftReady\":%s,\"applying\":%s}\n",
                static_cast<unsigned long>(value.draftRevision),
                static_cast<unsigned long>(value.activeProfileRevision),
                static_cast<unsigned long>(value.draftProfileRevision),
                value.hasActive ? "true" : "false", value.draftReady ? "true" : "false",
                value.applying ? "true" : "false");
}
DraftResult importFile(ProfileController &controller, const char *path) {
    std::FILE *input = std::fopen(path, "rb");
    if (!input) {
        std::perror("Cannot open draft profile");
        DraftResult result;
        result.outcome = DraftOutcome::InvalidSource;
        return result;
    }
    FileReader reader(input);
    auto result = controller.importDraft(reader, controller.storage().draftRevision);
    if (std::fclose(input) != 0 && result)
        result.outcome = DraftOutcome::ImportFailed;
    std::printf("{\"event\":\"draft_import\",\"result\":\"%s\",\"error\":\"%s\","
                "\"offset\":%lu,\"object\":%u}\n",
                draftOutcomeName(result.outcome), importErrorName(result.import.error),
                static_cast<unsigned long>(result.import.offset),
                static_cast<unsigned>(result.import.object));
    reportStorage(controller);
    return result;
}
void reportGameResult(const char *operation, const larp::execution::Result &result) {
    std::printf("{\"event\":\"game_control\",\"operation\":\"%s\",\"result\":\"%s\"}\n", operation,
                larp::execution::outcomeName(result.outcome));
}
} // namespace
int runActivation(const char *initialPath, bool hintFactory, const char *candidatePath,
                  const char *exportActivePath) {
    std::unique_ptr<Owners> owners(new Owners());
    std::unique_ptr<ProfileController> runtime(new ProfileController(*owners, *owners));
    if (!runtime->beginStorage()) {
        std::fputs("Profile storage allocation failed\n", stderr);
        return 1;
    }
    std::printf(
        "{\"event\":\"activation_demo\",\"syntheticOwners\":true,\"physicalHardware\":false,"
        "\"controllerBytes\":%lu,\"profileHeapBytes\":%lu,\"profileSlots\":2,"
        "\"ownerConfigurationBytes\":%lu,\"controlBytes\":%lu}\n",
        static_cast<unsigned long>(sizeof(ProfileController)),
        static_cast<unsigned long>(2 * sizeof(Profile)),
        static_cast<unsigned long>(sizeof(OwnerConfiguration)),
        static_cast<unsigned long>(sizeof(Control)));
    const auto initial =
        initialPath ? importFile(*runtime, initialPath)
                    : runtime->createDraft(
                          hintFactory ? FactoryProfile::Hint : FactoryProfile::Standard, 0);
    if (!initial)
        return 1;
    std::uint32_t now = 0;
    if (!runtime->activateDraft(runtime->storage().draftRevision, 0, now) ||
        !drive(*owners, *runtime, now))
        return 1;
    reportStorage(*runtime);
    auto result = runtime->startGame(runtime->activation().configurationRevision, ++now);
    reportGameResult("start_initial", result);
    if (!result)
        return 1;
    for (unsigned step = 0; step < 3; ++step) {
        owners->pump(++now);
        runtime->service(now);
        result = runtime->tickGame(now);
        if (!result)
            return 1;
    }
    result = runtime->stopGame(runtime->game().execution.stateRevision, ++now);
    reportGameResult("stop_initial", result);
    if (!result)
        return 1;
    owners->report();
    // Import directly into the one inactive slot; the old active remains locked.
    if (!importFile(*runtime, candidatePath))
        return 1;
    const auto requested = runtime->activateDraft(runtime->storage().draftRevision,
                                                  runtime->activation().configurationRevision, now);
    std::printf("{\"event\":\"activate_request\",\"result\":\"%s\",\"activation\":\"%s\"}\n",
                draftOutcomeName(requested.outcome),
                activationOutcomeName(requested.activation.outcome));
    if (!requested || !drive(*owners, *runtime, now))
        return 1;
    reportStorage(*runtime);
    result = runtime->startGame(runtime->activation().configurationRevision, ++now);
    reportGameResult("start_candidate", result);
    if (!result)
        return 1;
    for (unsigned step = 0; step < 3; ++step) {
        owners->pump(++now);
        runtime->service(now);
        result = runtime->tickGame(now);
        if (!result)
            return 1;
    }
    report(*runtime);
    owners->report();
    if (exportActivePath) {
        result = runtime->stopGame(runtime->game().execution.stateRevision, ++now);
        reportGameResult("stop_for_export", result);
        if (!result)
            return 1;
        std::FILE *output = std::fopen(exportActivePath, "wb");
        if (!output) {
            std::perror("Cannot open active profile export");
            return 1;
        }
        FileWriter writer(output);
        const auto exported =
            runtime->writeActive(writer, runtime->activation().configurationRevision);
        const bool closeOk = std::fclose(output) == 0;
        std::printf("{\"event\":\"active_export\",\"result\":\"%s\"}\n",
                    draftOutcomeName(exported.outcome));
        if (!exported || !closeOk)
            return 1;
    }
    return std::fflush(stdout) == 0 ? 0 : 1;
}
