#include "AudioOutput.h"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <cassert>
#include <cstring>
#include <iostream>

using namespace larp::audio;
static uart_config_t uartConfig;
static gpio_config_t gpioConfig;
static int txPin = -1, rxPin = -1, deleted = 0, gpioLevel = 1;
static int installResult = ESP_OK, gpioResult = ESP_OK, waitResult = ESP_OK;
static int copyCount = 10, copied = 0, reads = 0;
static CommandFrame sent;
unsigned long millis() { return 0; }
esp_err_t uart_param_config(uart_port_t port, const uart_config_t *config) {
    assert(port == UART_NUM_1); uartConfig = *config; return ESP_OK;
}
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts) {
    assert(port == UART_NUM_1 && rts == UART_PIN_NO_CHANGE && cts == UART_PIN_NO_CHANGE);
    txPin = tx; rxPin = rx; return ESP_OK;
}
esp_err_t uart_driver_install(uart_port_t port, int rx, int tx, int events, void *queue, int flags) {
    assert(port == UART_NUM_1 && rx == 256 && tx == 0 && events == 0 && queue == nullptr && flags == 0);
    return installResult;
}
esp_err_t uart_driver_delete(uart_port_t port) { assert(port == UART_NUM_1); ++deleted; return ESP_OK; }
int uart_tx_chars(uart_port_t port, const char *bytes, unsigned int length) {
    assert(port == UART_NUM_1 && length == 10); std::memcpy(sent.data(), bytes, length);
    ++copied; return copyCount;
}
esp_err_t uart_wait_tx_done(uart_port_t port, std::uint32_t ticks) {
    assert(port == UART_NUM_1 && ticks == 0); return waitResult;
}
int uart_read_bytes(uart_port_t port, void *, std::uint32_t length, std::uint32_t ticks) {
    assert(port == UART_NUM_1 && length == 32 && ticks == 0); ++reads; return 0;
}
esp_err_t gpio_config(const gpio_config_t *config) { gpioConfig = *config; return gpioResult; }
int gpio_get_level(gpio_num_t pin) { assert(pin == 14); return gpioLevel; }

int main(int argc, char **argv) {
    assert(argc == 2);
    const auto frame = AudioOutput::encode(0x16, 0);
    auto &bus = dfPlayerBus();
    if (!std::strcmp(argv[1], "install_failure")) {
        installResult = ESP_FAIL; assert(!bus.begin()); assert(!bus.submit(frame));
    } else if (!std::strcmp(argv[1], "gpio_failure")) {
        gpioResult = ESP_FAIL; assert(!bus.begin()); assert(deleted == 1);
    } else {
        assert(bus.begin()); assert(!bus.begin());
        assert(txPin == 26 && rxPin == 25);
        assert(uartConfig.baud_rate == 9600 && uartConfig.data_bits == UART_DATA_8_BITS);
        assert(uartConfig.stop_bits == UART_STOP_BITS_1 && uartConfig.parity == UART_PARITY_DISABLE);
        assert(uartConfig.flow_ctrl == UART_HW_FLOWCTRL_DISABLE && uartConfig.source_clk == UART_SCLK_APB);
        assert(gpioConfig.pin_bit_mask == (1ULL << 14) && gpioConfig.mode == GPIO_MODE_INPUT);
        assert(gpioConfig.pull_up_en == GPIO_PULLUP_DISABLE && gpioConfig.pull_down_en == GPIO_PULLDOWN_DISABLE);
        assert(gpioConfig.intr_type == GPIO_INTR_DISABLE); // No GPIO13 operation exists.
        assert(bus.poll() == TxResult::Failed);
        if (!std::strcmp(argv[1], "partial")) copyCount = 4;
        assert(bus.submit(frame)); assert(sent == frame && copied == 1);
        assert(!bus.submit(frame));
        waitResult = ESP_ERR_TIMEOUT; assert(bus.poll() == TxResult::Busy);
        assert(!bus.submit(frame));
        waitResult = !std::strcmp(argv[1], "tx_failure") ? ESP_FAIL : ESP_OK;
        const bool failure = copyCount != 10 || waitResult != ESP_OK;
        assert(bus.poll() == (failure ? TxResult::Failed : TxResult::Complete));
        copyCount = 10; assert(bus.submit(frame)); // Ownership released, recovery possible.
        assert(!bus.sampleBusyLow()); gpioLevel = 0; assert(bus.sampleBusyLow()); assert(reads == 2);
    }
    std::cout << "Audio UART " << argv[1] << " passed\n";
}
