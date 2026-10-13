#include "Esp32PeerProofNonce.h"
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_random.h>
namespace larp {
namespace companion {
bool Esp32PeerProofNonce::make(PeerNonce &destination) noexcept {
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED ||
        esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
        return false;
    PeerNonce value;
    esp_fill_random(value.data(), value.size());
    bool nonzero = false;
    for (const auto byte : value)
        if (byte)
            nonzero = true;
    if (!nonzero || esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED ||
        esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
        return false;
    destination = value;
    return true;
}
} // namespace companion
} // namespace larp
