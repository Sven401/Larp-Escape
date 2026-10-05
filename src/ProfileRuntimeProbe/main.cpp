// Synthetic owners only: building/running this probe never configures external
// hardware or networking. It is separate from the production legacy entry point.
#include "DeviceRuntime/Broker.h"
#include <Arduino.h>
#include <FreeRtosProfileChannel.h>
#include <freertos/task.h>

namespace {
using namespace larp::devices;
using namespace larp::configuration;
Profile profile;
Plan plan;
FreeRtosProfileChannel channel;
Broker broker(channel);
larp::execution::Session session(broker);
// ESP-IDF v4.4 specifies task depths in BYTES, unlike vanilla FreeRTOS.
static const std::size_t WorkerStackBytes = 4096;
static_assert(sizeof(StackType_t) == 1, "Pinned ESP32 port uses byte stack elements");
struct Worker {
    Owner owner = Owner::Nfc;
    Command command;
    TaskHandle_t handle = nullptr;
    StaticTask_t control = {};
    alignas(16) StackType_t stack[WorkerStackBytes] = {};
    std::uint32_t playStarted = 0;
    std::uint8_t crystal = 0;
    bool playing = false;
};
std::array<Worker, ChannelLimits::Owners> workers = {};
std::array<DeviceObservation, Limits::Components> observations = {};
bool ready = false;
std::uint8_t slot(ComponentId component) {
    for (std::uint8_t index = 0; index < plan.routeCount; ++index)
        if (plan.routes[index].id == component)
            return index;
    return 255;
}
void ownerTask(void *argument) {
    Worker &worker = *static_cast<Worker *>(argument);
    for (;;) {
        const std::uint32_t now = millis();
        if (worker.playing) {
            if (static_cast<std::uint32_t>(now - worker.playStarted) >= 100) {
                const std::uint8_t index = slot(worker.command.ticket.component);
                observations[index].sample.audioBusy = false;
                channel.publish(index, observations[index]);
                channel.complete(worker.owner, worker.command.ticket);
                worker.playing = false;
            }
        } else if (channel.take(worker.owner, worker.command)) {
            const Command &command = worker.command;
            const std::uint8_t index = slot(command.ticket.component);
            Reply reply;
            reply.ticket = command.ticket;
            reply.kind = command.kind;
            reply.completedMilliseconds = now;
            reply.outcome = PollOutcome::Fresh;
            bool read = false;
            switch (command.kind) {
            case CommandKind::ReadCard:
                reply.card = profile.roles.get(0)->uid;
                read = true;
                break;
            case CommandKind::ReadCrystals: {
                const Role &role = *profile.roles.get(0);
                if (worker.crystal < role.crystals.size()) {
                    const Crystal &crystal = *role.crystals.get(worker.crystal++);
                    larp::domain::GridPosition position;
                    position.row = crystal.row;
                    position.column = static_cast<larp::domain::GridColumn>(crystal.column);
                    reply.crystals.push_back(position);
                }
                read = true;
                break;
            }
            case CommandKind::PlayTrack:
                worker.playStarted = now;
                worker.playing = true;
                observations[index].sample.audioBusy = true;
                break;
            case CommandKind::WakeAudio:
            case CommandKind::ApplyEffect:
            case CommandKind::SendActivation:
                break;
            }
            observations[index].processedSequence = command.ticket.sequence;
            observations[index].lastOutcome = ActionOutcome::Applied;
            channel.publish(index, observations[index]);
            if (!worker.playing)
                channel.complete(worker.owner, command.ticket, read ? &reply : nullptr);
        }
        // Each component mailbox has exactly one writer, its configured owner.
        for (std::uint8_t index = 0; index < plan.routeCount; ++index)
            if (plan.routes[index].owner == worker.owner)
                channel.publish(index, observations[index]);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace
void setup() {
    Serial.begin(115200);
    if (!makeStandardProfile(profile) || !compilePlan(profile, plan) || !channel.begin() ||
        !broker.configure(plan, 1)) {
        Serial.println("profile_runtime_probe: setup rejected");
        return;
    }
    for (std::uint8_t index = 0; index < plan.routeCount; ++index) {
        observations[index].generation = 1;
        observations[index].sample.available = plan.routes[index].enabled;
        observations[index].sample.wakes.fill(true);
    }
    for (std::size_t index = 0; index < workers.size(); ++index) {
        Worker &worker = workers[index];
        worker.owner = static_cast<Owner>(index);
        if (!channel.resumeOwner(worker.owner, 1))
            return;
        worker.handle =
            xTaskCreateStaticPinnedToCore(ownerTask, ownerName(worker.owner), WorkerStackBytes,
                                          &worker, 1, worker.stack, &worker.control, 0);
        if (!worker.handle) {
            Serial.println("profile_runtime_probe: worker creation failed");
            return;
        }
    }
    Serial.printf("profile_runtime_probe: synthetic=true gpio=false network=false "
                  "profile=%u plan=%u session=%u broker=%u channel=%u command=%u reply=%u\n",
                  unsigned(sizeof(Profile)), unsigned(sizeof(Plan)), unsigned(sizeof(session)),
                  unsigned(sizeof(broker)), unsigned(sizeof(channel)), unsigned(sizeof(Command)),
                  unsigned(sizeof(Reply)));
    ready = static_cast<bool>(session.start(profile, millis()));
}
void loop() {
    if (!ready) {
        delay(100);
        return;
    }
    broker.service();
    session.tick(millis());
    static std::uint32_t last = 0;
    const std::uint32_t now = millis();
    if (static_cast<std::uint32_t>(now - last) >= 1000) {
        last = now;
        const auto state = session.snapshot();
        const auto traffic = broker.snapshot();
        const auto queues = channel.snapshot();
        Serial.printf("profile_runtime_probe: state=%u revision=%lu role=%u crystals=%u "
                      "accepted=%lu rejected=%lu replies=%lu stale=%lu timeout=%lu outstanding=%lu "
                      "drops=%lu heap=%u stacks=",
                      state.execution.state.value, (unsigned long)state.execution.stateRevision,
                      state.role.value, state.crystalCount, (unsigned long)traffic.accepted,
                      (unsigned long)traffic.rejected, (unsigned long)traffic.received,
                      (unsigned long)traffic.staleReplies, (unsigned long)traffic.timedOut,
                      (unsigned long)queues.outstanding, (unsigned long)queues.droppedReplies,
                      unsigned(ESP.getFreeHeap()));
        for (const Worker &worker : workers)
            Serial.printf("%u,", unsigned(uxTaskGetStackHighWaterMark(worker.handle)));
        Serial.println();
    }
    delay(5);
}
