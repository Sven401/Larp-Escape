#include "LedOutput.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <esp_intr_alloc.h>

namespace larp { namespace led { namespace {

class RmtLedBus final : public LedBus {
public:
    bool begin() override {
        if (installed_) return false;
        rmt_config_t config = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(LedOutput::LedPin), Channel);
        config.clk_div = 2; // 80 MHz APB / 2: one tick is 25 ns.
        config.mem_block_num = 1;
        config.tx_config.loop_en = false;
        config.tx_config.carrier_en = false;
        config.tx_config.idle_output_en = true;
        config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        if (rmt_config(&config) != ESP_OK ||
            rmt_driver_install(Channel, 0, ESP_INTR_FLAG_LOWMED) != ESP_OK) return false;
        installed_ = true;
        std::uint32_t frequency = 0;
        if (rmt_get_counter_clock(Channel, &frequency) != ESP_OK || frequency != 40000000U) {
            rmt_driver_uninstall(Channel);
            installed_ = false;
            return false;
        }
        transmitting_ = false;
        return true;
    }

    bool submit(const Frame &frame) override {
        if (!installed_ || transmitting_) return false;
        std::size_t index = 0;
        for (const auto &pixel : frame) {
            if (pixel.red > LedOutput::Brightness || pixel.green > LedOutput::Brightness ||
                pixel.blue > LedOutput::Brightness) return false;
            const std::uint8_t grb[] = {pixel.green, pixel.red, pixel.blue};
            for (const auto value : grb) for (int bit = 7; bit >= 0; --bit) {
                const bool one = (value & (1U << bit)) != 0;
                auto &item = items_[index++];
                item.level0 = 1;
                item.duration0 = one ? 24 : 10;
                item.level1 = 0;
                item.duration1 = one ? 26 : 40;
            }
        }
        // Explicit 300 us LOW latch. Buffer remains untouched until TX_DONE.
        auto &latch = items_[index];
        latch.level0 = latch.level1 = 0;
        latch.duration0 = latch.duration1 = 6000;
        if (rmt_write_items(Channel, items_, ItemCount, false) != ESP_OK) return false;
        transmitting_ = true;
        return true;
    }

    TxResult poll() override {
        if (!installed_ || !transmitting_) return TxResult::Failed;
        const esp_err_t result = rmt_wait_tx_done(Channel, 0);
        if (result == ESP_ERR_TIMEOUT) return TxResult::Busy;
        if (result != ESP_OK) return TxResult::Failed;
        transmitting_ = false;
        return TxResult::Complete;
    }

    bool reset() override {
        if (installed_) {
            // Stop and detach the failed transfer before reusing its backing
            // storage. This is recovery only, never evidence of LED darkness.
            if (rmt_tx_stop(Channel) != ESP_OK || rmt_driver_uninstall(Channel) != ESP_OK) return false;
            installed_ = false;
        }
        transmitting_ = false;
        return begin();
    }
private:
    static constexpr rmt_channel_t Channel = RMT_CHANNEL_0;
    static constexpr std::size_t ItemCount = PixelCount * 24U + 1U;
    rmt_item32_t items_[ItemCount]{};
    bool installed_ = false, transmitting_ = false;
};

LedBus &productBus() { static RmtLedBus bus; return bus; }
std::uint32_t productMillis() { return millis(); }
} // namespace

LedOutput::LedOutput() : LedOutput(productBus(), productMillis) {}

#ifdef LARP_LED_CAMERA_LAB
// Available only in the isolated USB lab image; one owner, no BLE product loop.
LedBus &cameraLabBus() { return productBus(); }
#endif

}} // namespace larp::led
