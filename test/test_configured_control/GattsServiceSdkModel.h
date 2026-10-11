#pragma once
#include <array>
#include <cstring>
#include <esp_gatts_api.h>
#include <vector>
namespace host_service_sdk {
struct Call {
    unsigned kind = 0;
    std::uint8_t interface = 0;
    std::uint16_t service = 0, handles = 0, maximum = 0, permission = 0;
    std::uint8_t property = 0, response = 0;
    std::array<std::uint8_t, 16> uuid = {};
};
std::vector<Call> calls;
unsigned failAt = 0;
void (*beforeApi)() = nullptr;
int post(Call value) {
    if (host_rtos::depth)
        host_rtos::nonzeroWait = true;
    calls.push_back(value);
    if (beforeApi) {
        auto hook = beforeApi;
        beforeApi = nullptr;
        hook();
    }
    return failAt == calls.size() ? -1 : ESP_OK;
}
void reset() {
    calls.clear();
    failAt = 0;
    beforeApi = nullptr;
}
} // namespace host_service_sdk
extern "C" {
esp_err_t esp_ble_gatts_create_service(esp_gatt_if_t interface, esp_gatt_srvc_id_t *service,
                                       std::uint16_t handles) {
    host_service_sdk::Call value;
    value.kind = 1;
    value.interface = interface;
    value.handles = handles;
    if (!service || !service->is_primary || service->id.inst_id || service->id.uuid.len != 16)
        return -1;
    std::memcpy(value.uuid.data(), service->id.uuid.uuid.uuid128, value.uuid.size());
    return host_service_sdk::post(value);
}
esp_err_t esp_ble_gatts_add_char(std::uint16_t service, esp_bt_uuid_t *uuid,
                                 esp_gatt_perm_t permission, esp_gatt_char_prop_t property,
                                 esp_attr_value_t *initial, esp_attr_control_t *response) {
    host_service_sdk::Call value;
    value.kind = 2;
    value.service = service;
    value.permission = permission;
    value.property = property;
    if (!uuid || uuid->len != 16 || !initial || initial->attr_len || initial->attr_value ||
        !response)
        return -1;
    value.maximum = initial->attr_max_len;
    value.response = response->auto_rsp;
    std::memcpy(value.uuid.data(), uuid->uuid.uuid128, value.uuid.size());
    return host_service_sdk::post(value);
}
esp_err_t esp_ble_gatts_start_service(std::uint16_t service) {
    host_service_sdk::Call value;
    value.kind = 3;
    value.service = service;
    return host_service_sdk::post(value);
}
esp_err_t esp_ble_gatts_add_char_descr(std::uint16_t service, esp_bt_uuid_t *uuid,
                                       esp_gatt_perm_t permission, esp_attr_value_t *initial,
                                       esp_attr_control_t *response) {
    host_service_sdk::Call value;
    value.kind = 4;
    value.service = service;
    value.permission = permission;
    if (!uuid || uuid->len != ESP_UUID_LEN_16 ||
        uuid->uuid.uuid16 != ESP_GATT_UUID_CHAR_CLIENT_CONFIG || !initial ||
        initial->attr_max_len != 2 || initial->attr_len || initial->attr_value || !response)
        return -1;
    value.maximum = initial->attr_max_len;
    value.response = response->auto_rsp;
    return host_service_sdk::post(value);
}
}
