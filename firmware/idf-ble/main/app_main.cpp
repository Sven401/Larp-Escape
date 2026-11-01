#include "Esp32LocalGeneration.h"
#include "Esp32RulesBootClock.h"
// Native qualification: real counter NVS, no radio start/enrollment/authority.
#include "NimblePeerSource.h"
#include "esp_log.h"
namespace {
struct ClosedIngress final : larp::companion::IPeerCipherIngress {
    bool open(const larp::companion::TlsOrigin &) noexcept override {
        return false;
    }
    bool push(const larp::companion::PeerCipherFragment &) noexcept override {
        return false;
    }
    bool poll(larp::companion::NimblePeerSource &) noexcept override {
        return false;
    }
    void compare(const larp::companion::TlsOrigin &, std::uint32_t) noexcept override {}
} ingress;
larp::companion::NimblePeerSource source(ingress);
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

    ESP_LOGI("native-ble-probe", "source compiled; ready=%d; radio/enrollment disabled",
             source.ready());
}
