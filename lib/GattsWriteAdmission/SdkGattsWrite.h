#pragma once
#include "GattsWriteAdmission.h"
#include <esp_gatts_api.h>
namespace larp {
namespace companion {
static_assert(static_cast<unsigned>(AttWriteStatus::Ok) == ESP_GATT_OK, "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InvalidHandle) == ESP_GATT_INVALID_HANDLE,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InvalidPdu) == ESP_GATT_INVALID_PDU,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::RequestUnsupported) ==
                  ESP_GATT_REQ_NOT_SUPPORTED,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InvalidOffset) == ESP_GATT_INVALID_OFFSET,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InsufficientAuthorization) ==
                  ESP_GATT_INSUF_AUTHORIZATION,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InvalidValueLength) ==
                  ESP_GATT_INVALID_ATTR_LEN,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::UnlikelyError) == ESP_GATT_ERR_UNLIKELY,
              "Pinned ATT status");
static_assert(static_cast<unsigned>(AttWriteStatus::InsufficientResources) ==
                  ESP_GATT_INSUF_RESOURCE,
              "Pinned ATT status");
// Actual pinned SDK ABI conversion only. NO authority/token inference.
// Callback source separately supplies its original lifecycle binding.
inline GattsWriteEvent fromSdkGattsWrite(esp_gatt_if_t interface,
                                         const esp_ble_gatts_cb_param_t &parameter) noexcept {
    GattsWriteEvent event;
    event.interface = interface;
    event.connection = parameter.write.conn_id;
    event.transaction = parameter.write.trans_id;
    event.attribute = parameter.write.handle;
    event.offset = parameter.write.offset;
    for (std::size_t i = 0; i < event.peerAddress.size(); ++i)
        event.peerAddress[i] = parameter.write.bda[i];
    event.prepared = parameter.write.is_prep;
    event.needResponse = parameter.write.need_rsp;
    event.count = parameter.write.len;
    event.bytes = reinterpret_cast<const char *>(parameter.write.value);
    return event;
}
// No prepared values are retained. Both EXECUTE and CANCEL are explicitly
// unsupported on this narrow endpoint. Original selected attribute is supplied
// by the callback lifecycle owner; the SDK execute event has no attribute/value.
inline GattsWriteEvent fromSdkGattsExecute(esp_gatt_if_t interface,
                                           const esp_ble_gatts_cb_param_t &parameter,
                                           const GattsWriteBinding &original) noexcept {
    GattsWriteEvent event;
    event.interface = interface;
    event.connection = parameter.exec_write.conn_id;
    event.transaction = parameter.exec_write.trans_id;
    event.attribute = original.attribute;
    for (std::size_t i = 0; i < event.peerAddress.size(); ++i)
        event.peerAddress[i] = parameter.exec_write.bda[i];
    event.prepared = true;
    event.needResponse = true;
    return event;
}
} // namespace companion
} // namespace larp
