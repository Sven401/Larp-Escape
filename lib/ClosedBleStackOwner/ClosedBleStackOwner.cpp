#include "ClosedBleStackOwner.h"

#include <esp_bt.h>
#include <esp_bt_main.h>

ClosedBleStackOwner::State ClosedBleStackOwner::begin() noexcept {
    if (state_ != State::NotStarted)
        return state_;

    esp_bt_controller_config_t controller = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&controller) != ESP_OK)
        return state_ = State::ControllerInitFailed;
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK)
        return state_ = State::ControllerEnableFailed;
    if (esp_bluedroid_init() != ESP_OK)
        return state_ = State::BluedroidInitFailed;
    if (esp_bluedroid_enable() != ESP_OK)
        return state_ = State::BluedroidEnableFailed;
    return state_ = State::ReadyClosed;
}
