// Isolated synthetic-owner entry point. No external GPIO, bus, audio, LEDs or
// radio are initialized. Runtime owns actual activation/Session/queue logic.
#include "Companion/CommandReceipt.h"
#include "Companion/CommandStream.h"
#include "Companion/IdentityAccess.h"
#include "Companion/ReplyFrameSender.h"
#include "Companion/StatusFrameSender.h"
#include "CompanionIntegration/ApplicationProfileStaging.h"
#include "CompanionIntegration/GameControl.h"
#include "DeviceRuntime/OwnerConfiguration.h"
#include "DeviceRuntime/ProfileApplication.h"
#include "GameDomain/GameDefinitions.h"
#include <Arduino.h>
#include <Esp32DeviceIdentityStore.h>
#include <Esp32ProfileStagingStore.h>
#include <Esp32ProfileStore.h>
#include <Esp32StartupIdentityEntropy.h>
#include <FreeRtosCommandFrameQueue.h>
#include <FreeRtosGameControlChannel.h>
#include <FreeRtosProfileChannel.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <freertos/task.h>
namespace {
using namespace larp::devices;
using namespace larp::configuration;
FreeRtosProfileChannel channel;
// Two startup allocations, private profiles; workers never borrow their contents.
Esp32DeviceIdentityStore publicIdentityStore;
Esp32StartupIdentityEntropy identityEntropy;
larp::companion::DeviceIdentityOwner publicIdentity(publicIdentityStore, identityEntropy);
Esp32ProfileStore persistentStore;
Esp32ProfileStagingStore temporarySource(persistentStore);
ProfileApplication runtime(channel, channel, persistentStore);
larp::integration::ApplicationProfileStaging guardedSource(runtime, temporarySource);
bool sourceChecked = false;
larp::companion::FreeRtosGameControlChannel companionChannel;
larp::companion::IdentityAccess identityAccess(companionChannel);
larp::companion::BoundConnection probeConnection;
larp::companion::TrustedAuthority probeRoute;
larp::companion::CommandStream commandStream(companionChannel);
larp::companion::FreeRtosCommandFrameQueue frameQueue(companionChannel);
// Synthetic bounded bearer: verifies current routing, copies one fixed value.
// No GATT, radio or peer acknowledgment is performed by this implementation.
class ProbeReplyBearer : public larp::companion::IReplyFrameBearer {
  public:
    larp::companion::RoutedReplyFrame last;
    std::uint32_t copies = 0;
    larp::companion::ReplyFrameCopy
    copyReplyFrame(const larp::companion::RoutedReplyFrame &frame) noexcept override {
        using namespace larp::companion;
        TrustedAuthority current;
        if (!companionChannel.readAuthority(current) || !current.gameMaster ||
            current.principal != frame.principal || current.epoch != frame.epoch)
            return ReplyFrameCopy::Unavailable;
        last = frame;
        if (copies != UINT32_MAX)
            ++copies;
        return ReplyFrameCopy::Copied;
    }
} replyBearer;
larp::companion::ReplyFrameSender replySender(companionChannel, replyBearer);
larp::companion::StatusFrameSender statusSender(companionChannel, replyBearer);
// Fixed owner scratch; do not place the 524-byte mailbox copy on each loop stack.
larp::companion::RoutedConfiguredStatus statusScratch;
std::array<char, 20> commandFrame = {};
larp::integration::GameControl gameControl(runtime, companionChannel);
larp::companion::BootIdentity bootIdentity;
std::uint32_t probeLease = 0;
bool startPending = false;
std::array<char, larp::companion::maxGameCommandJsonBytes> commandMessage = {};
larp::companion::CommandStreamResult submitFramedCommand(std::size_t count, std::uint32_t now) {
    using namespace larp::companion;
    if (!identityAccess.route(probeConnection, probeRoute)) {
        CommandStreamResult unavailable;
        unavailable.outcome = CommandStreamOutcome::Unauthorized;
        return unavailable;
    }
    if (commandStream.snapshot().complete)
        return commandStream.retry(now);
    CommandStreamResult result;
    for (std::size_t offset = 0; offset < count;) {
        const auto length = count - offset < 12 ? count - offset : 12;
        std::size_t written = 0;
        if (!encodeCommandFrame(1, static_cast<std::uint16_t>(count),
                                static_cast<std::uint16_t>(offset), commandMessage.data() + offset,
                                length, commandFrame.data(), commandFrame.size(), written)) {
            result.outcome = CommandStreamOutcome::InvalidFrame;
            return result;
        }
        // Synthetic callback/owner handoff through the same fixed RTOS queue.
        if (frameQueue.copyWrite(commandFrame.data(), written, probeRoute.principal,
                                 probeRoute.epoch, now) != FrameCopyOutcome::Copied) {
            result.outcome = CommandStreamOutcome::InvalidFrame;
            return result;
        }
        const auto dispatch = dispatchCommandFrame(frameQueue, commandStream, now);
        result = dispatch.stream;
        // Synthetic best-effort receipt; no GATT ack, retry or game tick.
        copyCommandReceipt(dispatch, companionChannel, replyBearer);
        if (result.outcome != CommandStreamOutcome::Receiving &&
            result.outcome != CommandStreamOutcome::Queued &&
            result.outcome != CommandStreamOutcome::QueueFull)
            return result;
        offset += length;
    }
    return result;
}
// ESP-IDF v4.4 task depths are BYTES; this pinned port has byte stack elements.
static const std::size_t WorkerStackBytes = 4096;
static_assert(sizeof(StackType_t) == 1, "Pinned ESP32 byte stack port required");
struct Worker {
    explicit Worker(Owner owner) : settings(owner), owner(owner) {}
    OwnerConfiguration settings;
    Owner owner;
    Command command;
    Control controlMessage;
    TaskHandle_t handle = nullptr;
    StaticTask_t taskControl = {};
    alignas(16) StackType_t stack[WorkerStackBytes] = {};
    std::array<DeviceObservation, Limits::Components> observations = {};
    larp::domain::CardUid card = {};
    larp::ports::CrystalInputBatch crystals;
    std::uint32_t playStarted = 0, generation = 0;
    bool playing = false, running = false;
};
struct Workers {
    Worker nfc{Owner::Nfc}, i2c{Owner::I2c}, audio{Owner::Audio}, led{Owner::Led},
        peer{Owner::Peer};
} storage;
const std::array<Worker *, ChannelLimits::Owners> workers = {
    {&storage.nfc, &storage.i2c, &storage.audio, &storage.led, &storage.peer}};
bool ready = false, switchRequested = false;
std::uint32_t boot = 0, lastStartedGeneration = 0;
void completeRead(Worker &worker, const Command &command, std::uint32_t now, bool cancelled) {
    const auto *component = worker.settings.find(command.ticket.component);
    Reply reply;
    reply.ticket = command.ticket;
    reply.kind = command.kind;
    reply.completedMilliseconds = now;
    reply.outcome = !cancelled && component ? PollOutcome::Fresh : PollOutcome::Unavailable;
    const bool read =
        command.kind == CommandKind::ReadCard || command.kind == CommandKind::ReadCrystals;
    if (!cancelled) {
        if (command.kind == CommandKind::ReadCard)
            reply.card = worker.card;
        else if (command.kind == CommandKind::ReadCrystals)
            reply.crystals = worker.crystals;
    }
    if (component) {
        auto &value = worker.observations[component->slot];
        value.processedSequence = command.ticket.sequence;
        value.lastOutcome = cancelled ? ActionOutcome::Unavailable : ActionOutcome::Applied;
        channel.publish(component->slot, value);
    }
    channel.complete(worker.owner, command.ticket, read ? &reply : nullptr);
}
bool control(Worker &worker, const Control &request, std::uint32_t now) {
    switch (request.ticket.kind) {
    case ControlKind::Park:
        worker.running = false;
        if (worker.playing) {
            const auto *component = worker.settings.find(worker.command.ticket.component);
            if (component)
                worker.observations[component->slot].sample.audioBusy = false;
            completeRead(worker, worker.command, now, true);
            worker.playing = false;
        }
        // Cancel copied queued jobs. A physical backend must first stop its
        // in-flight device operation; this synthetic audio can stop immediately.
        for (std::size_t step = 0;
             step < ChannelLimits::CommandsPerOwner && channel.take(worker.owner, worker.command);
             ++step)
            completeRead(worker, worker.command, now, true);
        return channel.parkOwner(worker.owner);
    case ControlKind::Begin:
        return !worker.running && worker.settings.begin(request);
    case ControlKind::Component:
        return !worker.running && worker.settings.append(request);
    case ControlKind::Commit:
        return !worker.running && worker.settings.commit(request);
    case ControlKind::Resume:
        if (!worker.running && worker.settings.ready(request.ticket.generation) &&
            channel.resumeOwner(worker.owner, request.ticket.generation)) {
            worker.generation = request.ticket.generation;
            worker.running = true;
            // Publish initial snapshots before acknowledging Resume. The
            // application may start the new Session immediately on that ack.
            for (std::uint8_t index = 0; index < worker.settings.count(); ++index) {
                const auto &component = *worker.settings.get(index);
                auto &value = worker.observations[component.slot];
                value.generation = worker.generation;
                value.sample.available = true;
                value.sample.audioBusy = false;
                value.sample.wakes.fill(true);
                channel.publish(component.slot, value);
            }
            return true;
        }
        return false;
    }
    return false;
}
void ownerTask(void *argument) {
    Worker &worker = *static_cast<Worker *>(argument);
    for (;;) {
        const std::uint32_t now = millis();
        if (channel.takeControl(worker.owner, worker.controlMessage)) {
            ControlReply reply;
            reply.ticket = worker.controlMessage.ticket;
            reply.outcome = control(worker, worker.controlMessage, now) ? ControlOutcome::Applied
                                                                        : ControlOutcome::Rejected;
            reply.completedMilliseconds = millis();
            channel.completeControl(worker.owner, reply);
        }
        if (!worker.running) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (worker.playing) {
            if (static_cast<std::uint32_t>(now - worker.playStarted) >= 100) {
                const auto *component = worker.settings.find(worker.command.ticket.component);
                if (component)
                    worker.observations[component->slot].sample.audioBusy = false;
                completeRead(worker, worker.command, now, false);
                worker.playing = false;
            }
        } else if (channel.take(worker.owner, worker.command)) {
            const auto *component = worker.settings.find(worker.command.ticket.component);
            if (worker.command.kind == CommandKind::PlayTrack && component) {
                worker.playing = true;
                worker.playStarted = now;
                auto &value = worker.observations[component->slot];
                value.processedSequence = worker.command.ticket.sequence;
                value.lastOutcome = ActionOutcome::Applied;
                value.sample.audioBusy = true;
                channel.publish(component->slot, value);
            } else
                completeRead(worker, worker.command, now, false);
        }
        for (std::uint8_t index = 0; index < worker.settings.count(); ++index) {
            const auto &component = *worker.settings.get(index);
            auto &value = worker.observations[component.slot];
            value.generation = worker.generation;
            value.sample.available = true;
            value.sample.wakes.fill(true);
            channel.publish(component.slot, value);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace
void setup() {
    // Isolated probe has not initialized RF, ADC or I2S. Enable physical
    // entropy temporarily, then release the ADC/RF resource before any owners.
    bootloader_random_enable();
    bootIdentity.high = esp_random();
    bootIdentity.low = esp_random();
    bootloader_random_disable();
    Serial.begin(115200);
    if (!publicIdentityStore.begin()) {
        Serial.println("profile_runtime_probe: public identity store failed; no erase; stopped");
        return;
    }
    const auto identityResult = publicIdentity.begin();
    if (identityResult != larp::companion::DeviceIdentityOutcome::Loaded &&
        identityResult != larp::companion::DeviceIdentityOutcome::Created) {
        Serial.println("profile_runtime_probe: public identity unavailable; stopped");
        return;
    }
    if (!persistentStore.begin()) {
        Serial.println("profile_runtime_probe: storage mount failed; no format; stopped");
        return;
    }
    const auto begun = runtime.begin();
    if (!begun || !channel.begin() || !companionChannel.begin() || !frameQueue.begin() ||
        !gameControl.begin(bootIdentity)) {
        Serial.printf("profile_runtime_probe: application setup rejected=%s; stopped\n",
                      applicationOutcomeName(begun.outcome));
        return;
    }
    // Public device label is loaded from NVS; peer label/verification are SYNTHETIC.
    // No provisioning, keys, pairing, radio or real identity proof here.
    larp::companion::Identity device, peer;
    if (!publicIdentity.identity(device))
        return;
    peer.bytes[0] = 2;
    larp::companion::VerifiedPeerEvidence evidence;
    evidence.peer = peer;
    evidence.encrypted = evidence.mitm = evidence.secureConnections = evidence.bonded = true;
    if (identityAccess.begin(device) != larp::companion::IdentityAccessOutcome::Initialized ||
        identityAccess.connect(0, probeConnection) !=
            larp::companion::IdentityAccessOutcome::Connected ||
        identityAccess.authenticate(probeConnection, evidence) !=
            larp::companion::IdentityAccessOutcome::Verified ||
        identityAccess.assignGameMaster(peer, 1) !=
            larp::companion::IdentityAccessOutcome::Assigned ||
        !identityAccess.route(probeConnection, probeRoute))
        return;
    gameControl.service(millis());
    probeLease = gameControl.snapshot().lease;
    if (!probeLease) {
        Serial.println("profile_runtime_probe: local control lease rejected");
        return;
    }
    // Factory input values copied before workers, independent of private slots.
    const auto &definition = larp::domain::defaultRoleDefinitions()[0];
    storage.nfc.card = definition.uid;
    for (const auto &crystal : definition.crystals)
        storage.i2c.crystals.push_back(crystal.position);
    for (auto *worker : workers) {
        worker->handle =
            xTaskCreateStaticPinnedToCore(ownerTask, ownerName(worker->owner), WorkerStackBytes,
                                          worker, 1, worker->stack, &worker->taskControl, 0);
        if (!worker->handle) {
            Serial.println("profile_runtime_probe: worker creation failed");
            return;
        }
    }
    Serial.printf(
        "profile_runtime_probe: synthetic=true gpio=false network=false two_profiles=true "
        "journal=true "
        "runtime=%u owner_config=%u channel=%u control=%u worker_storage=%u profile_heap=%u "
        "companion_channel=%u game_control=%u\n",
        unsigned(sizeof(runtime)), unsigned(sizeof(OwnerConfiguration)), unsigned(sizeof(channel)),
        unsigned(sizeof(Control)), unsigned(sizeof(storage)), unsigned(2 * sizeof(Profile)),
        unsigned(sizeof(companionChannel)), unsigned(sizeof(gameControl)));
    boot = millis();
    ready = true; // First service queues activation with post-IO fresh time.
}
void loop() {
    if (!ready) {
        delay(100);
        return;
    }
    const std::uint32_t now = millis();
    runtime.service(now);
    auto activation = runtime.activation();
    const auto application = runtime.snapshot();
    if (application.phase == ApplicationPhase::Fault ||
        (application.phase == ApplicationPhase::Ready &&
         application.outcome == ApplicationOutcome::ControllerRejected)) {
        Serial.printf("profile_runtime_probe: application stopped phase=%s result=%s\n",
                      applicationPhaseName(application.phase),
                      applicationOutcomeName(application.outcome));
        ready = false;
        return;
    }
    if (!sourceChecked && application.phase == ApplicationPhase::Ready) {
        // Same application actor, after confirmed Ready and before any GM start.
        std::uint8_t byte = 0;
        const bool good = guardedSource.discard() && guardedSource.begin(1) &&
                          !runtime.startGame(activation.configurationRevision, now) &&
                          guardedSource.append("x", 1) && guardedSource.seal() &&
                          guardedSource.readByte(byte) == ReadStatus::Byte && byte == 'x' &&
                          guardedSource.readByte(byte) == ReadStatus::End;
        const bool closed = guardedSource.discard();
        if (!good || !closed || runtime.profileFileBlocked()) {
            Serial.println("profile_runtime_probe: source owner check failed; stopped");
            ready = false;
            return;
        }
        sourceChecked = true;
        Serial.printf("temporary_source_probe: synthetic=true network=false bytes=%u "
                      "owner_bytes=%u imported=false saved=false start_guarded=true\n",
                      unsigned(sizeof(temporarySource)), unsigned(sizeof(guardedSource)));
    }
    if (!switchRequested && activation.hardwareReady && runtime.gameRunning() &&
        static_cast<std::uint32_t>(now - boot) >= 10000) {
        if (runtime.stopGame(runtime.game().execution.stateRevision, now)) {
            const auto created =
                runtime.createDraft(FactoryProfile::Hint, runtime.storage().draftRevision);
            switchRequested = true;
            if (!created) {
                Serial.printf("profile_runtime_probe: draft rejected=%s\n",
                              draftOutcomeName(created.outcome));
                ready = false;
                return;
            }
            const auto requested = runtime.activateDraft(runtime.storage().draftRevision,
                                                         activation.configurationRevision, now);
            if (!requested) {
                Serial.printf("profile_runtime_probe: switch rejected=%s\n",
                              applicationOutcomeName(requested.outcome));
                ready = false;
                return;
            }
        }
    }
    activation = runtime.activation();
    if (runtime.snapshot().phase == ApplicationPhase::Ready && activation.hardwareReady &&
        !runtime.gameRunning() && lastStartedGeneration != activation.generation) {
        if (!startPending) {
            larp::companion::TrustedGameRequest request;
            request.principal = probeRoute.principal;
            request.command.boot = bootIdentity;
            request.command.lease = probeLease;
            request.command.request = gameControl.snapshot().nextRequest;
            request.command.expectedConfigurationRevision = activation.configurationRevision;
            request.command.expectedStateRevision = runtime.execution().stateRevision;
            std::size_t written = 0;
            const auto encoded = larp::companion::encodeGameCommand(
                request.command, commandMessage.data(), commandMessage.size(), written);
            if (encoded != larp::companion::ConfiguredEncodeResult::Success) {
                Serial.println("profile_runtime_probe: GM command encoding rejected");
                ready = false;
                return;
            }
            const auto submitted = submitFramedCommand(written, now);
            if (submitted.outcome != larp::companion::CommandStreamOutcome::Queued &&
                submitted.outcome != larp::companion::CommandStreamOutcome::QueueFull) {
                Serial.printf(
                    "profile_runtime_probe: GM command admission rejected=%s\n",
                    larp::companion::commandDecodeErrorName(submitted.submission.decode.error));
                ready = false;
                return;
            }
            startPending = submitted.outcome == larp::companion::CommandStreamOutcome::Queued;
        }
    }
    // Application tick happened above exactly once. This drains only copied GM
    // requests and publishes a bounded status, without another game tick.
    gameControl.service(now);
    larp::companion::RoutedGameReply receipt;
    if (!replySender.pending() && companionChannel.takeCurrentReply(receipt)) {
        if (receipt.principal == probeRoute.principal &&
            receipt.connectionEpoch == probeRoute.epoch && startPending) {
            startPending = false;
            if (receipt.reply.outcome == larp::companion::GameReplyOutcome::Executed)
                lastStartedGeneration = runtime.activation().generation;
            else {
                Serial.printf("profile_runtime_probe: GM start rejected=%s\n",
                              larp::companion::gameReplyOutcomeName(receipt.reply.outcome));
                ready = false;
                return;
            }
        }
        if (replySender.load(receipt, now) != larp::companion::ReplySendOutcome::Loaded) {
            Serial.println("profile_runtime_probe: reply frame load dropped; game continues");
        }
    }
    // Read the producer-bound current mailbox only while the sender is free.
    // Encoder freezes this copy; subsequent publications cannot replace fragments.
    if (!replySender.pending() && !statusSender.pending() &&
        companionChannel.readCurrentStatus(statusScratch))
        statusSender.load(statusScratch, now);
    // At most one reply/status copy per step. Inbound processing receipts above
    // are separate. A completing reply still owns this whole step.
    const auto sent = larp::companion::serviceConfiguredFrames(replySender, statusSender, now, 20);
    const auto replySend = sent.reply;
    if (replySend == larp::companion::ReplySendOutcome::Expired ||
        replySend == larp::companion::ReplySendOutcome::Unauthorized ||
        replySend == larp::companion::ReplySendOutcome::TransportUnavailable) {
        Serial.println("profile_runtime_probe: synthetic reply stream dropped; game continues");
    }
    static std::uint32_t last = 0;
    if (static_cast<std::uint32_t>(now - last) >= 1000) {
        last = now;
        const auto game = runtime.game();
        const auto traffic = runtime.traffic();
        const auto queues = channel.snapshot();
        Serial.printf(
            "profile_runtime_probe: phase=%s finish=%s cfg=%lu profile=%lu generation=%lu "
            "state=%u role=%u crystals=%u accepted=%lu replies=%lu stale=%lu timeout=%lu "
            "outstanding=%lu "
            "control_drops=%lu draft_revision=%lu stored_generation=%lu application=%s "
            "stored_match=%s outbound_value_copies=%lu status_pending=%s heap=%u stacks=",
            activationPhaseName(activation.phase), activationFinishName(activation.finish),
            (unsigned long)activation.configurationRevision,
            (unsigned long)activation.profileRevision, (unsigned long)activation.generation,
            game.execution.state.value, game.role.value, game.crystalCount,
            (unsigned long)traffic.accepted, (unsigned long)traffic.received,
            (unsigned long)traffic.staleReplies, (unsigned long)traffic.timedOut,
            (unsigned long)queues.outstanding, (unsigned long)queues.droppedControlReplies,
            (unsigned long)runtime.storage().draftRevision,
            (unsigned long)runtime.snapshot().storedGeneration,
            applicationPhaseName(runtime.snapshot().phase),
            runtime.snapshot().storedMatchesActive ? "true" : "false",
            (unsigned long)replyBearer.copies, statusSender.pending() ? "true" : "false",
            unsigned(ESP.getFreeHeap()));
        for (const auto *worker : workers)
            Serial.printf("%u,", unsigned(uxTaskGetStackHighWaterMark(worker->handle)));
        Serial.println();
    }
    delay(5);
}
