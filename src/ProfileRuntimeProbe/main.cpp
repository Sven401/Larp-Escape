// Isolated synthetic-owner entry point. No external GPIO, bus, audio, LEDs or
// radio are initialized. Runtime owns actual activation/Session/queue logic.
#include "DeviceRuntime/OwnerConfiguration.h"
#include "DeviceRuntime/Runtime.h"
#include <Arduino.h>
#include <FreeRtosProfileChannel.h>
#include <freertos/task.h>
#include <memory>
#include <new>
namespace {
using namespace larp::devices;
using namespace larp::configuration;
// Reserve full fixed-capacity slots once at boot, before workers. ESP32's
// linker BSS region is smaller than its total allocator-addressable DRAM.
// No allocation in recurring control/game/device service; stable process lifetime.
std::unique_ptr<Profile> initialProfile, candidateProfile;
FreeRtosProfileChannel channel;
Runtime runtime(channel, channel);
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
    Serial.begin(115200);
    initialProfile.reset(new (std::nothrow) Profile());
    candidateProfile.reset(new (std::nothrow) Profile());
    if (!initialProfile || !candidateProfile) {
        Serial.println("profile_runtime_probe: profile storage allocation failed");
        return;
    }
    if (!makeStandardProfile(*initialProfile) || !makeHintExampleProfile(*candidateProfile) ||
        !channel.begin()) {
        Serial.println("profile_runtime_probe: setup rejected");
        return;
    }
    // Synthetic inputs are copied before task creation. Workers never read a Profile.
    storage.nfc.card = initialProfile->roles.get(0)->uid;
    for (std::size_t index = 0; index < initialProfile->roles.get(0)->crystals.size(); ++index) {
        const auto &crystal = *initialProfile->roles.get(0)->crystals.get(index);
        larp::domain::GridPosition position;
        position.row = crystal.row;
        position.column = static_cast<larp::domain::GridColumn>(crystal.column);
        storage.i2c.crystals.push_back(position);
    }
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
        "runtime=%u owner_config=%u channel=%u control=%u worker_storage=%u profile_heap=%u\n",
        unsigned(sizeof(runtime)), unsigned(sizeof(OwnerConfiguration)), unsigned(sizeof(channel)),
        unsigned(sizeof(Control)), unsigned(sizeof(storage)), unsigned(2 * sizeof(Profile)));
    boot = millis();
    ready = static_cast<bool>(runtime.activate(*initialProfile, 0, boot));
}
void loop() {
    if (!ready) {
        delay(100);
        return;
    }
    const std::uint32_t now = millis();
    runtime.service(now);
    auto activation = runtime.activation();
    auto game = runtime.game();
    if (!switchRequested && activation.hardwareReady && game.execution.running &&
        static_cast<std::uint32_t>(now - boot) >= 10000) {
        if (runtime.stopGame(game.execution.stateRevision, now))
            switchRequested = static_cast<bool>(
                runtime.activate(*candidateProfile, activation.configurationRevision, now));
    }
    activation = runtime.activation();
    if (activation.hardwareReady && !runtime.game().execution.running &&
        lastStartedGeneration != activation.generation) {
        if (runtime.startGame(activation.configurationRevision, now))
            lastStartedGeneration = activation.generation;
    }
    if (runtime.game().execution.running)
        runtime.tickGame(now);
    static std::uint32_t last = 0;
    if (static_cast<std::uint32_t>(now - last) >= 1000) {
        last = now;
        game = runtime.game();
        const auto traffic = runtime.traffic();
        const auto queues = channel.snapshot();
        Serial.printf(
            "profile_runtime_probe: phase=%s finish=%s cfg=%lu profile=%lu generation=%lu "
            "state=%u role=%u crystals=%u accepted=%lu replies=%lu stale=%lu timeout=%lu "
            "outstanding=%lu "
            "control_drops=%lu heap=%u stacks=",
            activationPhaseName(activation.phase), activationFinishName(activation.finish),
            (unsigned long)activation.configurationRevision,
            (unsigned long)activation.profileRevision, (unsigned long)activation.generation,
            game.execution.state.value, game.role.value, game.crystalCount,
            (unsigned long)traffic.accepted, (unsigned long)traffic.received,
            (unsigned long)traffic.staleReplies, (unsigned long)traffic.timedOut,
            (unsigned long)queues.outstanding, (unsigned long)queues.droppedControlReplies,
            unsigned(ESP.getFreeHeap()));
        for (const auto *worker : workers)
            Serial.printf("%u,", unsigned(uxTaskGetStackHighWaterMark(worker->handle)));
        Serial.println();
    }
    delay(5);
}
