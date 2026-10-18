#include "EspNowCommunicator.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Bench firmware: no Arduino startup, NVS, external GPIOs, game or BLE.
// USB requests are deliberate; boot never sends an ACTIVATE packet.
namespace {
constexpr uint8_t channel = 1;
constexpr size_t packetSize = 32;
struct Event {
    uint8_t kind; // 1: RX, 2: link delivery callback
    uint8_t mac[6];
    int length;
    uint8_t data[packetSize];
    esp_now_send_status_t status;
};
QueueHandle_t events;
std::atomic<uint32_t> dropped{0};
std::array<uint8_t, 6> peer{};
bool peerSet = false, radioOn = true;
uint32_t received = 0, receipts = 0, successes = 0, failures = 0, rejected = 0;
uint32_t bootId;
const std::array<uint8_t, packetSize> activation = {{'A','C','T','I','V','A','T','E'}};
const uint8_t ackMagic[8] = {'L','A','R','P','-','A','C','K'};

void rx(const uint8_t *mac, const uint8_t *data, int size) {
    if (!mac || !data || size != static_cast<int>(packetSize)) return;
    Event e{}; e.kind = 1; e.length = size;
    std::memcpy(e.mac, mac, 6); std::memcpy(e.data, data, packetSize);
    if (xQueueSend(events, &e, 0) != pdTRUE) dropped.fetch_add(1);
}
void tx(const uint8_t *mac, esp_now_send_status_t status) {
    Event e{}; e.kind = 2; e.status = status;
    if (mac) std::memcpy(e.mac, mac, 6);
    if (xQueueSend(events, &e, 0) != pdTRUE) dropped.fetch_add(1);
}
void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(n >> (i * 8));
}
uint32_t get32(const uint8_t *p) {
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) n |= static_cast<uint32_t>(p[i]) << (i * 8);
    return n;
}
void status() {
    std::printf("{\"event\":\"status\",\"rx\":%lu,\"receipts\":%lu,\"tx_ok\":%lu,\"tx_fail\":%lu,\"drops\":%lu,\"rejected\":%lu,\"radio\":%s}\n",
        static_cast<unsigned long>(received), static_cast<unsigned long>(receipts),
        static_cast<unsigned long>(successes), static_cast<unsigned long>(failures),
        static_cast<unsigned long>(dropped.load()), static_cast<unsigned long>(rejected), radioOn ? "true" : "false");
}
void command(const char *line) {
    if (std::strncmp(line, "PEER ", 5) == 0) {
        unsigned b[6]; char trailing;
        if (std::sscanf(line + 5, "%2x:%2x:%2x:%2x:%2x:%2x%c", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &trailing) != 6) {
            std::puts("{\"event\":\"error\",\"detail\":\"peer syntax\"}"); return;
        }
        if (peerSet) { ESP_ERROR_CHECK(esp_now_del_peer(peer.data())); peerSet = false; }
        for (unsigned i = 0; i < 6; ++i) peer[i] = static_cast<uint8_t>(b[i]);
        esp_now_peer_info_t info{};
        std::memcpy(info.peer_addr, peer.data(), 6); info.channel = channel;
        info.ifidx = WIFI_IF_STA; info.encrypt = false;
        const auto result = esp_now_add_peer(&info);
        peerSet = result == ESP_OK;
        std::printf("{\"event\":\"peer\",\"ok\":%s}\n", peerSet ? "true" : "false");
    } else if (std::strcmp(line, "SEND") == 0) {
        EspNowCommunicator sender(peer);
        const bool accepted = peerSet && radioOn && sender.send(larp::ports::GameMessage::Activate);
        std::printf("{\"event\":\"send\",\"accepted\":%s}\n", accepted ? "true" : "false");
    } else if (std::strcmp(line, "STATUS") == 0) status();
    else if (std::strcmp(line, "RADIO OFF") == 0) {
        const auto result = esp_wifi_stop();
        if (result == ESP_OK) radioOn = false;
        std::printf("{\"event\":\"radio\",\"on\":false,\"ok\":%s}\n", result == ESP_OK ? "true" : "false");
    } else if (std::strcmp(line, "RADIO ON") == 0) {
        const auto result = esp_wifi_start();
        if (result == ESP_OK) {
            ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE)); radioOn = true;
        }
        std::printf("{\"event\":\"radio\",\"on\":true,\"ok\":%s}\n", result == ESP_OK ? "true" : "false");
    } else std::puts("{\"event\":\"error\",\"detail\":\"unknown command\"}");
}
void service(const Event &e) {
    if (e.kind == 2) {
        if (e.status == ESP_NOW_SEND_SUCCESS) ++successes; else ++failures;
        std::printf("{\"event\":\"delivery\",\"ok\":%s}\n", e.status == ESP_NOW_SEND_SUCCESS ? "true" : "false");
        return;
    }
    if (!peerSet || std::memcmp(peer.data(), e.mac, 6) != 0) { ++rejected; return; }
    if (std::memcmp(e.data, activation.data(), packetSize) == 0) {
        ++received;
        std::array<uint8_t, packetSize> ack{};
        std::memcpy(ack.data(), ackMagic, 8); put32(ack.data() + 8, bootId); put32(ack.data() + 12, received);
        const auto result = esp_now_send(peer.data(), ack.data(), ack.size());
        std::printf("{\"event\":\"receive\",\"count\":%lu,\"boot\":%lu,\"bytes\":32,\"exact_activate\":true,\"ack_accepted\":%s}\n",
            static_cast<unsigned long>(received), static_cast<unsigned long>(bootId), result == ESP_OK ? "true" : "false");
    } else if (std::memcmp(e.data, ackMagic, 8) == 0) {
        for (size_t i = 16; i < packetSize; ++i) if (e.data[i] != 0) { ++rejected; return; }
        ++receipts;
        std::printf("{\"event\":\"receipt\",\"count\":%lu,\"boot\":%lu}\n",
            static_cast<unsigned long>(get32(e.data + 12)), static_cast<unsigned long>(get32(e.data + 8)));
    } else ++rejected;
}
} // namespace

extern "C" void app_main() {
    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 115200));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, nullptr, 0));
    events = xQueueCreate(16, sizeof(Event));
    if (!events) { std::puts("{\"event\":\"error\",\"detail\":\"queue\"}"); return; }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = 0; // No initialization, repair, erase or storage of NVS.
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(tx));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(rx));
    bootId = esp_random();
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    std::printf("{\"event\":\"ready\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"channel\":1,\"boot\":%lu}\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], static_cast<unsigned long>(bootId));
    char line[48]{}; size_t used = 0; bool overflow = false;
    for (;;) {
        uint8_t c;
        for (unsigned i = 0; i < 64 && uart_read_bytes(UART_NUM_0, &c, 1, 0) == 1; ++i) {
            if (c == '\r') continue;
            if (c == '\n') {
                line[used] = 0;
                if (!overflow) command(line);
                else std::puts("{\"event\":\"error\",\"detail\":\"command too long\"}");
                used = 0; overflow = false;
            } else if (used < sizeof(line) - 1) line[used++] = static_cast<char>(c);
            else overflow = true;
        }
        Event e;
        for (unsigned i = 0; i < 16 && xQueueReceive(events, &e, 0) == pdTRUE; ++i) service(e);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
