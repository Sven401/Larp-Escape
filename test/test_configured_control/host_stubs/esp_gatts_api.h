#pragma once
// Deterministic service-only public SDK model; not a claim of SDK ABI size.
#include <cstdint>
#include <esp_err.h>
using esp_gatt_if_t = std::uint8_t;
using esp_gatt_status_t = int;
using esp_gatt_perm_t = std::uint16_t;
using esp_gatt_char_prop_t = std::uint8_t;
constexpr int ESP_GATT_OK = 0;
constexpr std::uint16_t ESP_UUID_LEN_16 = 2, ESP_UUID_LEN_128 = 16,
                        ESP_GATT_UUID_CHAR_CLIENT_CONFIG = 0x2902,
                        ESP_GATT_PERM_WRITE_ENC_MITM = 64,
                        ESP_GATT_PERM_READ_ENC_MITM = 4;
constexpr std::uint8_t ESP_GATT_CHAR_PROP_BIT_READ = 2, ESP_GATT_CHAR_PROP_BIT_WRITE = 8,
                       ESP_GATT_CHAR_PROP_BIT_WRITE_NR = 4,
                       ESP_GATT_CHAR_PROP_BIT_NOTIFY = 16, ESP_GATT_RSP_BY_APP = 0;
struct esp_bt_uuid_t {
    std::uint16_t len;
    union {
        std::uint16_t uuid16;
        std::uint32_t uuid32;
        std::uint8_t uuid128[16];
    } uuid;
};
struct esp_gatt_id_t {
    esp_bt_uuid_t uuid;
    std::uint8_t inst_id;
};
struct esp_gatt_srvc_id_t {
    esp_gatt_id_t id;
    bool is_primary;
};
struct esp_attr_value_t {
    std::uint16_t attr_max_len, attr_len;
    std::uint8_t *attr_value;
};
struct esp_attr_control_t {
    std::uint8_t auto_rsp;
};
enum esp_gatts_cb_event_t {
    ESP_GATTS_REG_EVT = 0,
    ESP_GATTS_READ_EVT = 1,
    ESP_GATTS_CREATE_EVT = 7,
    ESP_GATTS_ADD_CHAR_EVT = 9,
    ESP_GATTS_ADD_CHAR_DESCR_EVT = 10,
    ESP_GATTS_START_EVT = 12
};
union esp_ble_gatts_cb_param_t {
    struct {
        esp_gatt_status_t status;
        std::uint16_t app_id;
    } reg;
    struct {
        esp_gatt_status_t status;
        std::uint16_t service_handle;
        esp_gatt_srvc_id_t service_id;
    } create;
    struct {
        esp_gatt_status_t status;
        std::uint16_t attr_handle, service_handle;
        esp_bt_uuid_t char_uuid;
    } add_char;
    struct {
        esp_gatt_status_t status;
        std::uint16_t attr_handle, service_handle;
        esp_bt_uuid_t descr_uuid;
    } add_char_descr;
    struct {
        esp_gatt_status_t status;
        std::uint16_t service_handle;
    } start;
};
extern "C" {
esp_err_t esp_ble_gatts_create_service(esp_gatt_if_t, esp_gatt_srvc_id_t *, std::uint16_t);
esp_err_t esp_ble_gatts_add_char(std::uint16_t, esp_bt_uuid_t *, esp_gatt_perm_t,
                                 esp_gatt_char_prop_t, esp_attr_value_t *, esp_attr_control_t *);
esp_err_t esp_ble_gatts_start_service(std::uint16_t);
esp_err_t esp_ble_gatts_add_char_descr(std::uint16_t, esp_bt_uuid_t *, esp_gatt_perm_t,
                                       esp_attr_value_t *, esp_attr_control_t *);
}
