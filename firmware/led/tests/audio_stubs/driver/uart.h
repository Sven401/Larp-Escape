#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using uart_port_t = int;
constexpr int ESP_OK = 0, ESP_ERR_TIMEOUT = 1, ESP_FAIL = -1;
constexpr int UART_NUM_1 = 1, UART_DATA_8_BITS = 3, UART_PARITY_DISABLE = 0;
constexpr int UART_STOP_BITS_1 = 1, UART_HW_FLOWCTRL_DISABLE = 0, UART_SCLK_APB = 1;
constexpr int UART_PIN_NO_CHANGE = -1;
struct uart_config_t {
    int baud_rate = 0, data_bits = 0, parity = 0, stop_bits = 0, flow_ctrl = 0;
    unsigned char rx_flow_ctrl_thresh = 0;
    int source_clk = 0;
};
esp_err_t uart_param_config(uart_port_t, const uart_config_t *);
esp_err_t uart_set_pin(uart_port_t, int, int, int, int);
esp_err_t uart_driver_install(uart_port_t, int, int, int, void *, int);
esp_err_t uart_driver_delete(uart_port_t);
int uart_tx_chars(uart_port_t, const char *, unsigned int);
esp_err_t uart_wait_tx_done(uart_port_t, std::uint32_t);
int uart_read_bytes(uart_port_t, void *, std::uint32_t, std::uint32_t);
