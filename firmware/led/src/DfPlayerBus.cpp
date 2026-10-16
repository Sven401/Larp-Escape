#include "AudioOutput.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>

namespace larp { namespace audio { namespace {

class DfPlayerBus final : public AudioBus {
public:
    bool begin() override {
        if (installed_) return false;
        uart_config_t config{};
        config.baud_rate = 9600;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_APB;
        if (uart_param_config(Port, &config) != ESP_OK ||
            uart_set_pin(Port, AudioOutput::TxPin, AudioOutput::RxPin,
                         UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK ||
            uart_driver_install(Port, 256, 0, 0, nullptr, 0) != ESP_OK) return false;
        gpio_config_t busy{};
        busy.pin_bit_mask = 1ULL << AudioOutput::BusyPin;
        busy.mode = GPIO_MODE_INPUT;
        busy.pull_up_en = GPIO_PULLUP_DISABLE;
        busy.pull_down_en = GPIO_PULLDOWN_DISABLE;
        busy.intr_type = GPIO_INTR_DISABLE;
        if (gpio_config(&busy) != ESP_OK) { uart_driver_delete(Port); return false; }
        installed_ = true;
        // GPIO13 is deliberately untouched: its module input and electrical
        // purpose are unverified. No wake pulse, reset, autoplay or pin scan.
        return true;
    }
    bool submit(const CommandFrame &frame) override {
        if (!installed_ || transmitting_) return false;
        // TX buffering is disabled. uart_tx_chars copies only into free FIFO
        // space and never waits; our sole owner submits ten bytes after TX_DONE.
        const int count = uart_tx_chars(Port, reinterpret_cast<const char *>(frame.data()), frame.size());
        transmitting_ = count > 0;
        partial_ = count != static_cast<int>(frame.size());
        // A short FIFO copy is a fault after the copied prefix has drained;
        // retaining ownership lets AudioOutput send a complete recovery Stop.
        return transmitting_;
    }
    TxResult poll() override {
        if (!installed_ || !transmitting_) return TxResult::Failed;
        const auto result = uart_wait_tx_done(Port, 0);
        if (result == ESP_ERR_TIMEOUT) return TxResult::Busy;
        transmitting_ = false;
        return result == ESP_OK && !partial_ ? TxResult::Complete : TxResult::Failed;
    }
    bool sampleBusyLow() override {
        // Bounded discard prevents unsolicited module replies filling RX.
        // They are not interpreted as correlated command acknowledgements.
        std::uint8_t ignored[32];
        if (installed_) uart_read_bytes(Port, ignored, sizeof(ignored), 0);
        return installed_ && gpio_get_level(static_cast<gpio_num_t>(AudioOutput::BusyPin)) == 0;
    }
private:
    static constexpr uart_port_t Port = UART_NUM_1;
    bool installed_ = false, transmitting_ = false, partial_ = false;
};

std::uint32_t productClock() { return millis(); }

} // namespace
AudioBus &dfPlayerBus() { static DfPlayerBus bus; return bus; }
AudioOutput::AudioOutput() : AudioOutput(dfPlayerBus(), productClock) {}

}} // namespace larp::audio
