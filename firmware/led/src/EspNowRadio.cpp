#ifdef LARP_ESPNOW_TEST
#include "EspNowRadio.h"
#include "EspNowTest.h"
#include "RssiMeasurement.h"
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
    int strength = 0;
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
Peer local{}, selected{};
larp::rssi::Filter filter;
Token sampled{};
bool controlIntensity = false;
unsigned appliedIntensity = 100;
std::uint32_t lastIntensityAt = 0;
void sniff(void *buffer, wifi_promiscuous_pkt_type_t type) {
    if (!buffer || type != WIFI_PKT_MGMT) return;
    const auto *p = static_cast<const wifi_promiscuous_pkt_t *>(buffer);
    larp::rssi::Observation o;
    if (!larp::rssi::decode(p->payload, p->rx_ctrl.sig_len, p->rx_ctrl.rssi, o) || o.destination != local) return;
    Event e{}; e.receipt = true; e.peer = o.source; e.packet = o.packet; e.strength = o.strength;
    if (xQueueSend(queue, &e, 0) != pdTRUE) dropped.store(true);
}
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
        esp_now_init() == ESP_OK && esp_now_register_send_cb(tx) == ESP_OK && esp_now_register_recv_cb(rx) == ESP_OK &&
        esp_wifi_get_mac(WIFI_IF_STA, local.data()) == ESP_OK &&
        esp_wifi_set_promiscuous_rx_cb(sniff) == ESP_OK;
    wifi_promiscuous_filter_t capture{}; capture.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    const bool captureOk = ok && esp_wifi_set_promiscuous_filter(&capture) == ESP_OK && esp_wifi_set_promiscuous(true) == ESP_OK;
    core.enable(captureOk); return captureOk;
}
void service(std::uint32_t now) {
    core.service(now);
    if (dropped.load()) { core.overflow(); return; }
    Event e{};
    for (unsigned n = 0; queue && n < 16 && xQueueReceive(queue, &e, 0) == pdTRUE; ++n) {
        if (e.receipt) core.receive(e.peer, e.packet.data(), e.packet.size(), e.strength);
        else core.delivery(e.peer, e.ok);
    }
    const auto state = core.snapshot();
    if (state.received && state.rssiValid && state.token != sampled) {
        sampled = state.token; filter.sample(state.rssi, now);
    }
}
void clearIntensity() { controlIntensity = false; appliedIntensity = 100; }
void cancel() { core.cancel(); filter.clear(); sampled.fill(0); clearIntensity(); }
unsigned intensity(std::uint32_t now) {
    if (!controlIntensity) return 100;
    if (now - lastIntensityAt >= 33U) {
        lastIntensityAt = now; const auto target = filter.target(now);
        if (appliedIntensity < target) appliedIntensity += std::min(3U, target - appliedIntensity);
        else appliedIntensity -= std::min(3U, appliedIntensity - target);
    }
    return appliedIntensity;
}
ledwire::EspNowSnapshot snapshot() {
    auto s = core.snapshot(); s.rssiSupported = true;
    if (s.rssiValid) { s.filteredRssi = static_cast<std::int8_t>(filter.value()); s.intensity = filter.target(millis()); }
    return s;
}
bool submit(const std::array<std::uint8_t, 6> &peer, std::uint32_t now, bool rssi, bool intensity) {
    Token token{}; esp_fill_random(token.data(), token.size());
    if (peer != selected) { filter.clear(); sampled.fill(0); }
    if (!core.start(peer, token, now, rssi)) return false;
    selected = peer; controlIntensity = rssi && intensity;
    return true;
}
}}
#endif
