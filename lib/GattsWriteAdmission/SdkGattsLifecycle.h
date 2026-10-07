#pragma once
#include "FreeRtosGattsConnection.h"
#include "SdkGattsWrite.h"
namespace larp {
namespace companion {
// Immediate pinned SDK callback conversion; all fields frozen before owner call.
// link_role, HCI handle, address type and success flags confer no peer/GM rights.
inline GattsConnectionEvent
fromSdkGattsConnect(esp_gatt_if_t interface, const esp_ble_gatts_cb_param_t &parameter) noexcept {
    GattsConnectionEvent event;
    event.interface = interface;
    event.connection = parameter.connect.conn_id;
    for (std::size_t i = 0; i < event.peerAddress.size(); ++i)
        event.peerAddress[i] = parameter.connect.remote_bda[i];
    return event;
}
inline GattsConnectionEvent
fromSdkGattsDisconnect(esp_gatt_if_t interface,
                       const esp_ble_gatts_cb_param_t &parameter) noexcept {
    GattsConnectionEvent event;
    event.interface = interface;
    event.connection = parameter.disconnect.conn_id;
    for (std::size_t i = 0; i < event.peerAddress.size(); ++i)
        event.peerAddress[i] = parameter.disconnect.remote_bda[i];
    return event;
}
} // namespace companion
} // namespace larp
