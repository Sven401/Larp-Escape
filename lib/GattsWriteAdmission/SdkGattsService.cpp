#include "SdkGattsService.h"
namespace larp {
namespace companion {
namespace {
esp_bt_uuid_t sdkUuid(const std::array<std::uint8_t, 16> &value) {
    esp_bt_uuid_t uuid = {};
    uuid.len = ESP_UUID_LEN_128;
    std::memcpy(uuid.uuid.uuid128, value.data(), value.size());
    return uuid;
}
bool add(std::uint16_t service, const std::array<std::uint8_t, 16> &value, bool read = false) {
    auto uuid = sdkUuid(value);
    esp_attr_value_t initial = {};
    initial.attr_max_len = 128;
    esp_attr_control_t control = {};
    control.auto_rsp = ESP_GATT_RSP_BY_APP;
    // SDK encryption/MITM permission is not a verified peer/GM grant.
    return esp_ble_gatts_add_char(service, &uuid,
                                  read ? ESP_GATT_PERM_READ_ENC_MITM : ESP_GATT_PERM_WRITE_ENC_MITM,
                                  read ? ESP_GATT_CHAR_PROP_BIT_READ : ESP_GATT_CHAR_PROP_BIT_WRITE,
                                  &initial, &control) == ESP_OK;
}
} // namespace
bool SdkGattsServiceDriver::create(std::uint8_t interface, bool challenge) noexcept {
    esp_gatt_srvc_id_t service = {};
    service.id.uuid = sdkUuid(GattsServiceDefinition::service());
    service.id.inst_id = 0;
    service.is_primary = true;
    return esp_ble_gatts_create_service(interface, &service,
                                        challenge ? GattsServiceDefinition::ChallengeHandles
                                                  : GattsServiceDefinition::Handles) == ESP_OK;
}
bool SdkGattsServiceDriver::addCommand(std::uint16_t service) noexcept {
    return add(service, GattsServiceDefinition::command());
}
bool SdkGattsServiceDriver::addProfile(std::uint16_t service) noexcept {
    return add(service, GattsServiceDefinition::profile());
}
bool SdkGattsServiceDriver::addChallenge(std::uint16_t service) noexcept {
    return add(service, GattsServiceDefinition::challenge(), true);
}
bool SdkGattsServiceDriver::start(std::uint16_t service) noexcept {
    return esp_ble_gatts_start_service(service) == ESP_OK;
}
} // namespace companion
} // namespace larp
