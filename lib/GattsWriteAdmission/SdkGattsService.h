#pragma once
#include "GattsServiceEvent.h"
#include <cstring>
#include <esp_gatts_api.h>
namespace larp {
namespace companion {
// Freeze only selected real callback fields; no borrowed pointers escape.
inline GattsServiceEvent fromSdkGattsService(esp_gatts_cb_event_t kind, esp_gatt_if_t interface,
                                             const esp_ble_gatts_cb_param_t &parameter) noexcept {
    GattsServiceEvent event;
    event.interface = interface;
    esp_bt_uuid_t uuid = {};
    switch (kind) {
    case ESP_GATTS_REG_EVT:
        event.kind = GattsServiceEventKind::Registered;
        event.application = parameter.reg.app_id;
        event.success = parameter.reg.status == ESP_GATT_OK;
        break;
    case ESP_GATTS_CREATE_EVT:
        event.kind = GattsServiceEventKind::Created;
        event.service = parameter.create.service_handle;
        event.success = parameter.create.status == ESP_GATT_OK;
        event.instance = parameter.create.service_id.id.inst_id;
        event.primary = parameter.create.service_id.is_primary;
        uuid = parameter.create.service_id.id.uuid;
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        event.kind = GattsServiceEventKind::Attribute;
        event.service = parameter.add_char.service_handle;
        event.attribute = parameter.add_char.attr_handle;
        event.success = parameter.add_char.status == ESP_GATT_OK;
        uuid = parameter.add_char.char_uuid;
        break;
    case ESP_GATTS_START_EVT:
        event.kind = GattsServiceEventKind::Started;
        event.service = parameter.start.service_handle;
        event.success = parameter.start.status == ESP_GATT_OK;
        break;
    default:
        break;
    }
    if (uuid.len == ESP_UUID_LEN_128) {
        std::memcpy(event.uuid.data(), uuid.uuid.uuid128, event.uuid.size());
        event.uuid128 = true;
    }
    return event;
}
class SdkGattsServiceDriver {
  public:
    bool create(std::uint8_t interface, bool challenge = false, bool issue = false,
                bool auth = false) noexcept;
    bool addCommand(std::uint16_t service) noexcept;
    bool addProfile(std::uint16_t service) noexcept;
    bool addChallenge(std::uint16_t service) noexcept;
    bool addIssue(std::uint16_t service) noexcept;
    bool addProof(std::uint16_t service) noexcept;
    bool addResult(std::uint16_t service) noexcept;
    bool start(std::uint16_t service) noexcept;
};
} // namespace companion
} // namespace larp
