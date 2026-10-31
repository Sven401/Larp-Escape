// Build qualification only: real queues, no physical owners or authorisation.
#include "Esp32ProgressPartition.h"
#include "FreeRtosGameControlChannel.h"
#include "FreeRtosProfileChannel.h"
#include "PsaPeerProofCrypto.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
larp::companion::FreeRtosGameControlChannel commands;
larp::devices::FreeRtosProfileChannel profiles;
larp::companion::PsaPeerProofCrypto proof;
Esp32ProgressPartition progressStore;
} // namespace

extern "C" void app_main() {
    // Force the actual adapter into the SDK image without opening/reading flash.
    ESP_LOGI("larp-idf-probe", "unopened progress capacity=%u",
             static_cast<unsigned>(progressStore.capacity()));
    const larp::companion::PeerPublicKey invalidKey{};
    const bool ready = commands.begin() && profiles.begin() && !proof.validKey(invalidKey);
    ESP_LOGI("larp-idf-probe", "queues=%s; no BLE, device buses or product grant",
             ready ? "ready" : "failed");
    // Keep process-lifetime storage alive. No authority is installed here.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
