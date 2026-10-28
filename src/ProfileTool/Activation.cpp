#include "Activation.h"
#include "CompanionIntegration/ApplicationProfileStaging.h"
#include "DeviceRuntime/OwnerConfiguration.h"
#include "DeviceRuntime/ProfileApplication.h"
#include "DeviceRuntime/ProfileController.h"
#include "DeviceRuntime/ProfileJournal.h"
#include "TemporaryProfileFile.h"
#include <cerrno>
#include <cstdio>
#include <memory>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
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
    explicit Owners(bool trace = true) : workers_{{&nfc_, &i2c_, &audio_, &led_, &peer_}} {
        trace_ = trace;
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
                if (trace_)
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
                        static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                                  ? control.component.bus
                                                  : 0),
                        static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                                  ? control.component.address
                                                  : 0),
                        static_cast<unsigned>(control.ticket.kind == ControlKind::Component
                                                  ? control.component.volume
                                                  : 0),
                        static_cast<unsigned long>(control.ticket.kind == ControlKind::Component
                                                       ? control.component.baud
                                                       : 0),
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
    bool processing_ = false, trace_ = true;
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
template <typename Controller> void report(const Controller &runtime) {
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
bool configured(const ProfileController &runtime) {
    return runtime.activation().hardwareReady &&
           runtime.activation().finish == ActivationFinish::Applied;
}
bool configured(const ProfileApplication &runtime) {
    const auto state = runtime.snapshot();
    return state.phase == ApplicationPhase::Ready && state.outcome == ApplicationOutcome::Ready &&
           state.storedMatchesActive && runtime.activation().hardwareReady;
}
template <typename Controller> bool drive(Owners &owners, Controller &runtime, std::uint32_t &now) {
    for (unsigned step = 0; step < 256 && runtime.busy(); ++step) {
        owners.pump(++now);
        runtime.service(now);
    }
    report(runtime);
    owners.report();
    return !runtime.busy() && configured(runtime);
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
// Native file backend. Existing directory supplied by caller; fixed filenames.
// OS synchronization is requested; physical power-loss durability is NOT a
// host observation. This backend is not an ESP32 filesystem implementation.
class NativeJournalStore : public IJournalStore {
  public:
    explicit NativeJournalStore(const char *directory) {
        for (unsigned slot = 0; slot < 2; ++slot) {
            const int count = std::snprintf(paths_[slot].data(), paths_[slot].size(),
                                            "%s/profile-%c.larp", directory, 'a' + slot);
            if (count < 0 || static_cast<std::size_t>(count) >= paths_[slot].size())
                valid_ = false;
        }
    }
    ~NativeJournalStore() {
        if (file_)
            std::fclose(file_);
    }
    JournalOpen openRead(std::uint8_t slot) override {
        if (!valid_ || slot > 1 || file_)
            return JournalOpen::Failure;
        errno = 0;
        file_ = std::fopen(paths_[slot].data(), "rb");
        writing_ = false;
        return file_             ? JournalOpen::Present
               : errno == ENOENT ? JournalOpen::Missing
                                 : JournalOpen::Failure;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (!file_ || writing_)
            return ReadStatus::Failure;
        const int value = std::fgetc(file_);
        if (value == EOF)
            return std::ferror(file_) ? ReadStatus::Failure : ReadStatus::End;
        byte = static_cast<std::uint8_t>(value);
        return ReadStatus::Byte;
    }
    bool closeRead() override {
        if (!file_ || writing_)
            return false;
        const bool good = std::fclose(file_) == 0;
        file_ = nullptr;
        return good;
    }
    bool beginWrite(std::uint8_t slot) override {
        if (!valid_ || slot > 1 || file_)
            return false;
        file_ = std::fopen(paths_[slot].data(), "wb");
        writing_ = true;
        return file_ != nullptr;
    }
    bool write(const char *bytes, std::size_t count) override {
        return file_ && writing_ && std::fwrite(bytes, 1, count, file_) == count;
    }
    JournalCommit finishWrite() override {
        if (!file_ || !writing_)
            return JournalCommit::Uncertain;
        const bool flushed = std::fflush(file_) == 0;
#ifdef _WIN32
        const bool synced = flushed && _commit(_fileno(file_)) == 0;
#else
        const bool synced = flushed && fsync(fileno(file_)) == 0;
#endif
        const bool closed = std::fclose(file_) == 0;
        file_ = nullptr;
        writing_ = false;
        return flushed && synced && closed ? JournalCommit::Confirmed : JournalCommit::Uncertain;
    }
    void abortWrite() override {
        if (file_)
            std::fclose(file_);
        file_ = nullptr;
        writing_ = false;
    }

  private:
    std::array<std::array<char, 1024>, 2> paths_ = {};
    std::FILE *file_ = nullptr;
    bool valid_ = true, writing_ = false;
};
template <typename Controller> bool exportActive(Controller &controller, const char *path) {
    std::FILE *output = std::fopen(path, "wb");
    if (!output) {
        std::perror("Cannot open active profile export");
        return false;
    }
    FileWriter writer(output);
    const auto result =
        controller.writeActive(writer, controller.activation().configurationRevision);
    const bool closed = std::fclose(output) == 0;
    std::printf("{\"event\":\"active_export\",\"result\":\"%s\"}\n",
                draftOutcomeName(result.outcome));
    return static_cast<bool>(result) && closed;
}
template <typename Controller> void reportStorage(const Controller &controller) {
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
template <typename Controller> DraftResult importFile(Controller &controller, const char *path) {
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
// Lazy startup source: an input path is read only for an empty journal.
// EOF includes checked close, so an initial import cannot hide close failure.
class InitialFileSource : public IProfileReader {
  public:
    explicit InitialFileSource(const char *path) : path_(path) {}
    ~InitialFileSource() {
        if (file_)
            std::fclose(file_);
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (ended_)
            return final_;
        if (!file_) {
            file_ = std::fopen(path_, "rb");
            if (!file_) {
                ended_ = true;
                final_ = ReadStatus::Failure;
                return final_;
            }
        }
        const int value = std::fgetc(file_);
        if (value != EOF) {
            byte = static_cast<std::uint8_t>(value);
            return ReadStatus::Byte;
        }
        const bool good = !std::ferror(file_);
        const bool closed = std::fclose(file_) == 0;
        file_ = nullptr;
        ended_ = true;
        final_ = good && closed ? ReadStatus::End : ReadStatus::Failure;
        return final_;
    }

  private:
    const char *path_;
    std::FILE *file_ = nullptr;
    bool ended_ = false;
    ReadStatus final_ = ReadStatus::End;
};
void reportApplication(const ProfileApplication &application) {
    const auto state = application.snapshot();
    const auto journal = application.persistence();
    std::printf(
        "{\"event\":\"profile_application\",\"phase\":\"%s\",\"result\":\"%s\","
        "\"storedGeneration\":%lu,\"storedProfileRevision\":%lu,\"storedConfigurationRevision\":%"
        "lu,"
        "\"storedMatchesActive\":%s,\"journalResult\":\"%s\",\"protectedSlot\":%u,"
        "\"writeUncertain\":%s,\"selectedUnconfirmed\":%s,\"fallbackAttempted\":%s}\n",
        applicationPhaseName(state.phase), applicationOutcomeName(state.outcome),
        static_cast<unsigned long>(state.storedGeneration),
        static_cast<unsigned long>(state.storedProfileRevision),
        static_cast<unsigned long>(state.storedConfigurationRevision),
        state.storedMatchesActive ? "true" : "false", journalOutcomeName(state.journal),
        static_cast<unsigned>(journal.protectedSlot), journal.writeUncertain ? "true" : "false",
        journal.selectedUnconfirmed ? "true" : "false", state.fallbackAttempted ? "true" : "false");
}
int runStoredActivation(const char *initialPath, bool hintFactory, const char *candidatePath,
                        const char *exportActivePath, const char *directory, bool restoreOnly) {
    std::unique_ptr<Owners> owners(new Owners());
    NativeJournalStore store(directory);
    std::unique_ptr<ProfileApplication> app(new ProfileApplication(*owners, *owners, store));
    InitialFileSource initial(initialPath);
    const auto begun =
        app->begin(restoreOnly ? BootPolicy::ExistingOnly : BootPolicy::InitializeEmpty,
                   hintFactory ? FactoryProfile::Hint : FactoryProfile::Standard,
                   initialPath ? &initial : nullptr);
    reportApplication(*app);
    if (!begun)
        return 1;
    std::uint32_t now = 0;
    if (!drive(*owners, *app, now)) {
        reportApplication(*app);
        return 1;
    }
    reportStorage(*app);
    reportApplication(*app);
    if (restoreOnly) {
        if (exportActivePath && !exportActive(*app, exportActivePath))
            return 1;
        return std::fflush(stdout) == 0 ? 0 : 1;
    }
    auto started = app->startGame(app->activation().configurationRevision, ++now);
    reportGameResult("start_initial", started);
    if (!started)
        return 1;
    for (unsigned step = 0; step < 3; ++step) {
        owners->pump(++now);
        app->service(now);
    }
    const auto stopped = app->stopGame(app->game().execution.stateRevision, ++now);
    reportGameResult("stop_initial", stopped);
    if (!stopped || !importFile(*app, candidatePath))
        return 1;
    const auto requested = app->activateDraft(app->storage().draftRevision,
                                              app->activation().configurationRevision, now);
    std::printf("{\"event\":\"activate_request\",\"result\":\"%s\"}\n",
                applicationOutcomeName(requested.outcome));
    if (!requested || !drive(*owners, *app, now)) {
        reportApplication(*app);
        return 1;
    }
    reportStorage(*app);
    reportApplication(*app);
    started = app->startGame(app->activation().configurationRevision, ++now);
    reportGameResult("start_candidate", started);
    if (!started)
        return 1;
    for (unsigned step = 0; step < 3; ++step) {
        owners->pump(++now);
        app->service(now);
    }
    report(*app);
    owners->report();
    if (exportActivePath) {
        const auto stoppedForExport = app->stopGame(app->game().execution.stateRevision, ++now);
        reportGameResult("stop_for_export", stoppedForExport);
        if (!stoppedForExport || !exportActive(*app, exportActivePath))
            return 1;
    }
    return std::fflush(stdout) == 0 ? 0 : 1;
}
} // namespace
int runActivation(const char *initialPath, bool hintFactory, const char *candidatePath,
                  const char *exportActivePath, const char *journalDirectory, bool restoreOnly) {
    if (journalDirectory)
        return runStoredActivation(initialPath, hintFactory, candidatePath, exportActivePath,
                                   journalDirectory, restoreOnly);
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
        if (!exportActive(*runtime, exportActivePath))
            return 1;
    }
    return std::fflush(stdout) == 0 ? 0 : 1;
}

int runApplicationFileProbe(const char *directory) {
    using larp::integration::ApplicationProfileStaging;
    // This command uses the existing native journal backend and synthetic owners.
    // One deliberate cleanup failure models uncertainty, never device power loss.
    class CleanupFault : public larp::companion::IProfileStagingStore {
      public:
        TemporaryProfileFile file;
        bool reject = false;
        unsigned io = 0;
        bool begin(std::uint32_t bytes) noexcept override {
            ++io;
            return file.begin(bytes);
        }
        bool append(const char *bytes, std::size_t count) noexcept override {
            ++io;
            return file.append(bytes, count);
        }
        bool seal() noexcept override {
            ++io;
            return file.seal();
        }
        bool discard() noexcept override {
            ++io;
            return !reject && file.discard();
        }
        ReadStatus readByte(std::uint8_t &byte) override {
            ++io;
            return file.readByte(byte);
        }
    } backend;
    std::unique_ptr<Owners> owners(new Owners(false));
    NativeJournalStore journal(directory);
    std::unique_ptr<ProfileApplication> app(new ProfileApplication(*owners, *owners, journal));
    if (!app->begin())
        return 1;
    std::uint32_t now = 0;
    for (unsigned step = 0; step < 256 && app->busy(); ++step) {
        owners->pump(++now);
        app->service(now);
    }
    if (!configured(*app))
        return 1;
    const auto revision = app->activation().configurationRevision;
    bool blocked = false, failed = false, abandoned = false, newBegin = true, started = false;
    {
        ApplicationProfileStaging file(*app, backend);
        if (!file.discard() || !file.begin(1))
            return 1;
        blocked = !app->startGame(revision, ++now);
        std::uint8_t byte = 0;
        if (!file.append("x", 1) || !file.seal() || file.readByte(byte) != ReadStatus::Byte ||
            byte != 'x' || file.readByte(byte) != ReadStatus::End)
            return 1;
        backend.reject = true;
        if (file.discard())
            return 1;
        failed = !app->startGame(revision, ++now);
    }
    abandoned = !app->startGame(revision, ++now);
    ApplicationProfileStaging replacement(*app, backend);
    newBegin = replacement.begin(1);
    backend.reject = false;
    if (newBegin || !replacement.discard())
        return 1;
    started = static_cast<bool>(app->startGame(revision, ++now));
    const auto before = backend.io;
    if (replacement.begin(1) || replacement.discard())
        return 1;
    const auto runningIo = backend.io - before;
    if (!blocked || !failed || !abandoned || newBegin || !started || runningIo ||
        app->profileFileBlocked())
        return 1;
    const auto count = std::printf(
        "{\"scope\":\"host_application_file_owner\",\"temporarySource\":\"actual_host_file\","
        "\"journalStorage\":\"host_file\",\"blockedBeforeCleanup\":true,\"failedCleanupBlocked\":"
        "true,"
        "\"abandonBlocked\":true,\"newBeginBeforeCleanup\":false,\"cleanedStart\":true,"
        "\"runningIoCalls\":%u,\"profileImportExecuted\":false,\"radioMeasured\":false}\n",
        runningIo);
    return count > 0 && std::fflush(stdout) == 0 ? 0 : 1;
}
