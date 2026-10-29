// Build qualification only: real queues, no physical owners or authorisation.
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
} // namespace

extern "C" void app_main() {
    const larp::companion::PeerPublicKey invalidKey{};
    const bool ready = commands.begin() && profiles.begin() && !proof.validKey(invalidKey);
    ESP_LOGI("larp-idf-probe", "queues=%s; no BLE, device buses or product grant",
             ready ? "ready" : "failed");
    // Keep process-lifetime storage alive. No authority is installed here.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
