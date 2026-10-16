#pragma once
#include "uart.h"
using gpio_num_t = int;
constexpr int GPIO_MODE_INPUT = 1, GPIO_PULLUP_DISABLE = 0, GPIO_PULLDOWN_DISABLE = 0;
constexpr int GPIO_INTR_DISABLE = 0;
struct gpio_config_t {
    std::uint64_t pin_bit_mask = 0;
    int mode = 0, pull_up_en = 0, pull_down_en = 0, intr_type = 0;
};
esp_err_t gpio_config(const gpio_config_t *);
int gpio_get_level(gpio_num_t);
