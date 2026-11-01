#include "Esp32LocalGeneration.h"
#include "Esp32RulesBootClock.h"
// Native qualification: real counter NVS/queues, no physical owners or authority.
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
    // Only startup entropy; must finish before any ADC/I2S/RF/device owner.
    // Failure is permanent for this process and opens no runtime/authority.
    if (!Esp32RulesBootClock::instance().beginEarlyStartup()) {
        ESP_LOGE("larp-boot-clock", "startup clock unavailable; startup closed");
        return;
    }
    // Counter availability gates ONLY future Local identity production. A missing
    // COUNTER RECORD is never created. NVS init may internally recover pages;
    // unavailable reservation does not disable communication/setup.
    if (Esp32LocalGeneration::instance().reserveEarlyStartup(Esp32RulesBootClock::instance()) !=
        larp::devices::LocalGenerationStatus::Reserved)
        ESP_LOGW("larp-local-generation", "local generation unavailable; no Local input admission");

    // The progress partition stays unopened. Counter NVS above is actual IO.
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
