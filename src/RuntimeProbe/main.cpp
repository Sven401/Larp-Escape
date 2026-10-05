// USB-only diagnostic profile. No external GPIO, SPI, I2C, audio or radio setup.
#include "../../test/test_game_application/GameFakes.h"
#include "Companion/StatusProtocol.h"
#include "CompanionStatusStore.h"
#include "GameRuntime/WakePulseSchedule.h"
#include <Arduino.h>
#include <atomic>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
CompanionStatusStore statusStore;
QueueHandle_t commands = nullptr;
TaskHandle_t producerHandle = nullptr;
TaskHandle_t consumerHandle = nullptr;
std::atomic<std::uint32_t> sent{0U}, dropped{0U}, consumed{0U}, snapshotsRead{0U}, failures{0U};
std::atomic<std::uint32_t> producerFreeStack{0U}, consumerFreeStack{0U};
bool ready = false;

larp::application::GameSnapshot syntheticStatus(std::uint32_t sequence) {
    return {larp::application::GamePhase::Idle,
            (sequence % 2U) != 0U,
            larp::domain::RoleId::Krieger,
            static_cast<std::uint8_t>(sequence % 4U),
            sequence,
            static_cast<std::uint8_t>(sequence % 7U),
            (sequence % 3U) == 0U,
            (sequence % 5U) == 0U,
            (sequence % 11U) == 0U};
}
bool consistent(const larp::application::GameSnapshot &snapshot) {
    const auto expected = syntheticStatus(snapshot.roundCount);
    return snapshot.phase == expected.phase && snapshot.rolePresent == expected.rolePresent &&
           snapshot.roleId == expected.roleId && snapshot.crystalCount == expected.crystalCount &&
           snapshot.seenRoleCount == expected.seenRoleCount &&
           snapshot.incorrectCrystal == expected.incorrectCrystal &&
           snapshot.keystoneError == expected.keystoneError &&
           snapshot.audioBusy == expected.audioBusy;
}
void producer(void *) {
    std::uint32_t sequence = 0U;
    for (;;) {
        // Deliberate overload: 16 attempts into a four-element queue.
        for (unsigned index = 0U; index < 16U; ++index) {
            ++sequence;
            if (xQueueSend(commands, &sequence, 0U) == pdTRUE)
                sent.fetch_add(1U);
            else
                dropped.fetch_add(1U);
            statusStore.publish(syntheticStatus(sequence));
        }
        producerFreeStack.store(uxTaskGetStackHighWaterMark(nullptr));
        vTaskDelay(pdMS_TO_TICKS(10U) > 0U ? pdMS_TO_TICKS(10U) : 1U);
    }
}
void consumer(void *) {
    std::uint32_t previous = 0U;
    TickType_t frame = xTaskGetTickCount();
    for (;;) {
        std::uint32_t sequence;
        for (unsigned drained = 0U; drained < 4U; ++drained) {
            if (xQueueReceive(commands, &sequence, 0U) != pdTRUE)
                break;
            if (sequence <= previous)
                failures.fetch_add(1U);
            previous = sequence;
            consumed.fetch_add(1U);
        }
        larp::application::GameSnapshot snapshot;
        if (statusStore.read(snapshot)) {
            snapshotsRead.fetch_add(1U);
            if (!consistent(snapshot))
                failures.fetch_add(1U);
        }
        consumerFreeStack.store(uxTaskGetStackHighWaterMark(nullptr));
        if (xTaskDelayUntil(&frame, pdMS_TO_TICKS(33U)) != pdTRUE) {
            frame = xTaskGetTickCount();
            vTaskDelay(1U);
        }
    }
}
bool checkGameOnDevice() {
    game_test::GameRig rig;
    rig.wakeToIdle();
    const auto &roles = larp::domain::defaultRoleDefinitions();
    for (std::size_t index = 0U; index < roles.size(); ++index) {
        rig.completeRound(roles[index]);
        if (rig.phase() != larp::application::GamePhase::GameCompleted ||
            rig.game.snapshot().roundCount != index + 1U)
            return false;
        rig.returnToIdle();
        if (index + 1U < roles.size())
            rig.clearRoleCardForNextRound();
    }
    if (rig.phase() != larp::application::GamePhase::Bonus)
        return false;
    rig.audio.finish();
    rig.tick();
    rig.tick(500U);
    return rig.phase() == larp::application::GamePhase::Idle &&
           rig.communicator.messages.size() == 1U && rig.clock.delayCalls == 0U;
}
bool checkWakeOnDevice() {
    larp::runtime::WakePulseSchedule schedule;
    if (!schedule.requestWake(UINT32_MAX - 399U))
        return false;
    for (std::uint32_t edge = 1U; edge <= 7U; ++edge)
        if (!schedule.advance(UINT32_MAX - 399U + edge * 100U, 250U))
            return false;
    if (schedule.isHigh() || !schedule.isWaking() || schedule.requestWake(350U))
        return false;
    return schedule.advance(400U, 250U) && !schedule.isHigh() && !schedule.isWaking();
}
void report() {
    const auto attempted = sent.load() + dropped.load();
    Serial.printf(
        "PROBE uptime_ms=%lu attempts=%lu accepted=%lu dropped=%lu consumed=%lu snapshots=%lu "
        "failures=%lu heap=%u min_heap=%u stack_producer=%lu stack_consumer=%lu\n",
        static_cast<unsigned long>(millis()), static_cast<unsigned long>(attempted),
        static_cast<unsigned long>(sent.load()), static_cast<unsigned long>(dropped.load()),
        static_cast<unsigned long>(consumed.load()),
        static_cast<unsigned long>(snapshotsRead.load()),
        static_cast<unsigned long>(failures.load()), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
        static_cast<unsigned long>(producerFreeStack.load()),
        static_cast<unsigned long>(consumerFreeStack.load()));
    larp::application::GameSnapshot snapshot;
    char json[larp::companion::maxStatusJsonBytes];
    std::size_t written = 0U;
    if (statusStore.read(snapshot) &&
        larp::companion::encodeStatus(snapshot, json, sizeof(json), written) ==
            larp::companion::StatusEncodeResult::Success)
        Serial.println(json);
    else
        Serial.println("PROBE status-unavailable");
}
} // namespace

void setup() {
    Serial.begin(115200);
    delay(300U);
    Serial.printf("LARP USB PROBE chip=%s revision=%u cores=%u cpu_mhz=%u flash_bytes=%u\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                  ESP.getCpuFreqMHz(), ESP.getFlashChipSize());
    const bool gamePassed = checkGameOnDevice();
    const bool wakePassed = checkWakeOnDevice();
    Serial.printf("SELFTEST game_six_roles=%s wake_rollover=%s\n", gamePassed ? "PASS" : "FAIL",
                  wakePassed ? "PASS" : "FAIL");
    if (!gamePassed || !wakePassed)
        failures.fetch_add(1U);
    if (!statusStore.begin()) {
        Serial.println("PROBE mailbox-init-failed");
        return;
    }
    statusStore.publish(syntheticStatus(0U));
    commands = xQueueCreate(4U, sizeof(std::uint32_t));
    if (commands == nullptr) {
        Serial.println("PROBE command-queue-init-failed");
        return;
    }
    // Start the consumer first; if producer creation fails, stop the reader.
    if (xTaskCreatePinnedToCore(consumer, "Probe reader", 3072U, nullptr, 1U, &consumerHandle, 0) !=
        pdPASS) {
        Serial.println("PROBE consumer-start-failed");
        return;
    }
    if (xTaskCreatePinnedToCore(producer, "Probe writer", 3072U, nullptr, 1U, &producerHandle, 1) !=
        pdPASS) {
        vTaskDelete(consumerHandle);
        Serial.println("PROBE producer-start-failed");
        return;
    }
    ready = true;
    Serial.println(
        "PROBE ready: synthetic queue load + cross-core snapshot copies; no external devices");
}
void loop() {
    static std::uint32_t lastReport = 0U;
    if (ready && static_cast<std::uint32_t>(millis() - lastReport) >= 5000U) {
        lastReport = millis();
        report();
    }
    delay(1U);
}
