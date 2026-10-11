#include "ClosedBleStackOwner.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_heap_caps.h>

#if defined(LARP_PRODUCT_GATTS_CLOSED) && defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_ENABLED)
// Arduino's initArduino() runs before setup(). Its weak fallback reports that
// BT is unused and irreversibly releases all controller memory unless this
// product owner claims it at link time. No controller or radio is started here.
extern "C" bool btInUse(void) { return true; }
#endif

ClosedBleStackOwner::State ClosedBleStackOwner::begin() noexcept {
    struct SdkPort {
        using Mode = ClosedBleStartupAttempt::Mode;
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
        std::int32_t controllerInit(Mode mode) noexcept {
            esp_bt_controller_config_t controller = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            controller.mode = static_cast<esp_bt_mode_t>(mode);
            return esp_bt_controller_init(&controller);
        }
        std::int32_t controllerEnable(Mode mode) noexcept {
            return esp_bt_controller_enable(static_cast<esp_bt_mode_t>(mode));
        }
        std::int32_t bluedroidInit() noexcept { return esp_bluedroid_init(); }
        std::int32_t bluedroidEnable() noexcept { return esp_bluedroid_enable(); }
    } port;
    static_assert(static_cast<std::uint8_t>(ESP_BT_MODE_BLE) ==
                  static_cast<std::uint8_t>(ClosedBleStartupAttempt::Mode::Ble),
                  "BLE startup mode must match the pinned ESP-IDF ABI");
    return attempt_.begin(port);
}
