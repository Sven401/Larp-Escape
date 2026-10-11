#include "ClosedBleStackOwner.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_heap_caps.h>

ClosedBleStackOwner::State ClosedBleStackOwner::begin() noexcept {
    struct SdkPort {
        std::uint8_t controllerStatus() noexcept {
            return static_cast<std::uint8_t>(esp_bt_controller_get_status());
        }
        std::uint8_t bluedroidStatus() noexcept {
            return static_cast<std::uint8_t>(esp_bluedroid_get_status());
        }
        std::uint32_t internalFree() noexcept {
            return static_cast<std::uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }
        std::uint32_t internalLargest() noexcept {
            return static_cast<std::uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }
        std::int32_t controllerInit() noexcept {
            esp_bt_controller_config_t controller = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            return esp_bt_controller_init(&controller);
        }
        std::int32_t controllerEnable() noexcept {
            return esp_bt_controller_enable(ESP_BT_MODE_BLE);
        }
        std::int32_t bluedroidInit() noexcept { return esp_bluedroid_init(); }
        std::int32_t bluedroidEnable() noexcept { return esp_bluedroid_enable(); }
    } port;
    return attempt_.begin(port);
}
