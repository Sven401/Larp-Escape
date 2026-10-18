#ifdef LARP_ESPNOW_TEST
#include "EspNowRadio.h"
#include "EspNowTest.h"
#include <Arduino.h>
#include <atomic>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/queue.h>

namespace larp { namespace espnowradio {
namespace {
using namespace espnowtest;
struct Event {
    bool receipt = false, ok = false;
    Peer peer{};
    Packet packet{};
};
QueueHandle_t queue = nullptr;
std::atomic<bool> dropped{false};
class Radio : public IRadio {
    Peer configured_{};
    bool configured = false;
public:
    bool send(const Peer &peer, const Packet &packet) override {
        if (!configured || peer != configured_) {
            if (configured && esp_now_del_peer(configured_.data()) != ESP_OK) return false;
            configured = false;
            esp_now_peer_info_t info{};
            std::copy(peer.begin(), peer.end(), info.peer_addr);
            info.channel = 1; info.ifidx = WIFI_IF_STA; info.encrypt = false;
            if (esp_now_add_peer(&info) != ESP_OK) return false;
            configured_ = peer; configured = true;
        }
        return esp_now_send(peer.data(), packet.data(), 56) == ESP_OK;
    }
} radio;
Core core(radio);
void tx(const uint8_t *mac, esp_now_send_status_t status) {
    if (!mac) return;
    Event e{}; std::copy(mac, mac + 6, e.peer.begin()); e.ok = status == ESP_NOW_SEND_SUCCESS;
    if (xQueueSend(queue, &e, 0) != pdTRUE) dropped.store(true);
}
void rx(const uint8_t *mac, const uint8_t *bytes, int size) {
    if (!mac || !bytes || size != 64) return;
    Event e{}; e.receipt = true;
    std::copy(mac, mac + 6, e.peer.begin()); std::copy(bytes, bytes + size, e.packet.begin());
    if (xQueueSend(queue, &e, 0) != pdTRUE) dropped.store(true);
}
}
bool begin() {
    queue = xQueueCreate(16, sizeof(Event));
    if (!queue || esp_netif_init() != ESP_OK) return false;
    const auto loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) return false;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT(); config.nvs_enable = 0;
    const bool ok = esp_wifi_init(&config) == ESP_OK &&
        esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK &&
        esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK && esp_wifi_start() == ESP_OK &&
        // ESP-IDF 4.4 requires modem sleep for WiFi/Bluetooth coexistence.
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) == ESP_OK && esp_wifi_set_ps(WIFI_PS_MIN_MODEM) == ESP_OK &&
        esp_now_init() == ESP_OK && esp_now_register_send_cb(tx) == ESP_OK && esp_now_register_recv_cb(rx) == ESP_OK;
    core.enable(ok); return ok;
}
void service(std::uint32_t now) {
    core.service(now);
    if (dropped.load()) { core.overflow(); return; }
    Event e{};
    for (unsigned n = 0; queue && n < 16 && xQueueReceive(queue, &e, 0) == pdTRUE; ++n) {
        if (e.receipt) core.receive(e.peer, e.packet.data(), e.packet.size());
        else core.delivery(e.peer, e.ok);
    }
}
void cancel() { core.cancel(); }
ledwire::EspNowSnapshot snapshot() { return core.snapshot(); }
bool submit(const std::array<std::uint8_t, 6> &peer, std::uint32_t now) {
    Token token{}; esp_fill_random(token.data(), token.size());
    return core.start(peer, token, now);
}
}}
#endif
