#include "../../led/include/EspNowTest.h"
#include <cstdio>
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>

namespace {
using namespace larp::espnowtest;
struct Event { Peer peer{}; Packet packet{}; };
QueueHandle_t queue;
std::atomic<uint32_t> dropped{0};
void rx(const uint8_t *mac, const uint8_t *data, int size) {
    if (!mac || !data || size != 56) return;
    Event e{}; std::copy(mac, mac + 6, e.peer.begin()); std::copy(data, data + size, e.packet.begin());
    if (xQueueSend(queue, &e, 0) != pdTRUE) dropped.fetch_add(1);
}
}
extern "C" void app_main() {
    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 115200));
    queue = xQueueCreate(16, sizeof(Event));
    if (!queue) return;
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); cfg.nvs_enable = 0;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)); ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_now_init()); ESP_ERROR_CHECK(esp_now_register_recv_cb(rx));
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    const uint32_t boot = esp_random(); uint32_t count = 0;
    std::printf("LARP ESP-NOW app receiver ready %02X:%02X:%02X:%02X:%02X:%02X channel=1\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Peer active{}; bool added = false;
    for (;;) {
        Event e;
        if (xQueueReceive(queue, &e, pdMS_TO_TICKS(10)) != pdTRUE) continue;
        if (dropped.load()) { std::puts("LARP ESP-NOW receiver queue fault; restart required"); return; }
        if (!validPeer(e.peer) || !validPacket(e.packet.data(), 56, false) || count == UINT32_MAX) continue;
        if (!added || active != e.peer) {
            if (added) { ESP_ERROR_CHECK(esp_now_del_peer(active.data())); added = false; }
            esp_now_peer_info_t p{}; std::copy(e.peer.begin(), e.peer.end(), p.peer_addr);
            p.channel = 1; p.ifidx = WIFI_IF_STA;
            if (esp_now_add_peer(&p) != ESP_OK) continue;
            active = e.peer; added = true;
        }
        e.packet[5] = 2;
        encode32(e.packet.data() + 56, boot);
        encode32(e.packet.data() + 60, ++count);
        const auto result = esp_now_send(e.peer.data(), e.packet.data(), e.packet.size());
        std::printf("LARP verified ACTIVATE count=%lu receipt_accepted=%d\n", static_cast<unsigned long>(count), result == ESP_OK);
    }
}
