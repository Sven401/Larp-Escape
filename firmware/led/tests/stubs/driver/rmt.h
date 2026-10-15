#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_TIMEOUT = 0x107;
enum rmt_channel_t { RMT_CHANNEL_0 = 0 };
enum gpio_num_t { GPIO_NUM_15 = 15 };
enum rmt_idle_level_t { RMT_IDLE_LEVEL_LOW = 0 };
struct rmt_item32_t {
    std::uint32_t duration0 : 15;
    std::uint32_t level0 : 1;
    std::uint32_t duration1 : 15;
    std::uint32_t level1 : 1;
};
struct rmt_config_t {
    gpio_num_t gpio_num{};
    rmt_channel_t channel{};
    unsigned clk_div = 0, mem_block_num = 0;
    struct { bool loop_en = false, carrier_en = false, idle_output_en = false; rmt_idle_level_t idle_level{}; } tx_config;
};
inline rmt_config_t defaultConfig(gpio_num_t pin, rmt_channel_t channel) {
    rmt_config_t config{}; config.gpio_num = pin; config.channel = channel; return config;
}
#define RMT_DEFAULT_CONFIG_TX(pin, channel) defaultConfig(pin, channel)
esp_err_t rmt_config(const rmt_config_t *config);
esp_err_t rmt_driver_install(rmt_channel_t channel, std::size_t receiveBuffer, int flags);
esp_err_t rmt_get_counter_clock(rmt_channel_t channel, std::uint32_t *frequency);
esp_err_t rmt_driver_uninstall(rmt_channel_t channel);
esp_err_t rmt_write_items(rmt_channel_t channel, const rmt_item32_t *items, int count, bool wait);
esp_err_t rmt_wait_tx_done(rmt_channel_t channel, unsigned timeout);
esp_err_t rmt_tx_stop(rmt_channel_t channel);
