#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "HardwareAcceptance/RadioProtocol.h"

#include <cstring>

namespace {
using namespace hardware_acceptance;
constexpr size_t MaxRecords = 128;
constexpr int ReadyLed = 2;

struct __attribute__((packed)) Record {
    uint64_t run;
    uint32_t sequence;
    uint8_t source[6];
    uint8_t duplicates;
};

struct Inbound {
    RadioFrame frame;
    uint8_t source[6];
};

Preferences storage;
QueueHandle_t incoming;
Record records[MaxRecords] = {};
size_t recordCount = 0;
volatile uint32_t queueDrops = 0;
uint32_t invalidFrames = 0;
uint32_t storageFailures = 0;
String command;
bool ready = false;

void onReceive(const uint8_t *source, const uint8_t *data, int length) {
    if (length != static_cast<int>(sizeof(RadioFrame)) || !incoming)
        return;
    Inbound item{};
    memcpy(&item.frame, data, sizeof(RadioFrame));
    memcpy(item.source, source, sizeof(item.source));
    if (xQueueSend(incoming, &item, 0) != pdTRUE)
        ++queueDrops;
}

void printMac(const uint8_t *mac) {
    for (int i = 0; i < 6; ++i) {
        if (i)
            Serial.print(':');
        if (mac[i] < 16)
            Serial.print('0');
        Serial.print(mac[i], HEX);
    }
}

void report() {
    Serial.printf("REPORT records=%u queue_drops=%lu invalid=%lu storage_failures=%lu\n",
                  static_cast<unsigned>(recordCount), static_cast<unsigned long>(queueDrops),
                  static_cast<unsigned long>(invalidFrames),
                  static_cast<unsigned long>(storageFailures));
    for (size_t i = 0; i < recordCount; ++i) {
        Serial.printf("RECEIVED index=%u run=%016llX sequence=%lu duplicates=%u source=",
                      static_cast<unsigned>(i), static_cast<unsigned long long>(records[i].run),
                      static_cast<unsigned long>(records[i].sequence), records[i].duplicates);
        printMac(records[i].source);
        Serial.println();
    }
    Serial.println("REPORT END");
}

bool persist() {
    const size_t bytes = recordCount * sizeof(Record);
    return storage.putBytes("records", records, bytes) == bytes;
}

void handle(const Inbound &item) {
    const RadioFrame &frame = item.frame;
    if (frame.magic != RadioMagic || frame.version != RadioVersion || frame.type != RadioData ||
        frame.reserved != 0 || frame.check != radioCheck(frame) || frame.run == 0 ||
        frame.sequence == 0) {
        ++invalidFrames;
        return;
    }
    size_t index = 0;
    for (; index < recordCount; ++index)
        if (records[index].run == frame.run && records[index].sequence == frame.sequence)
            break;
    if (index == recordCount) {
        if (recordCount == MaxRecords) {
            Serial.printf("FULL run=%016llX sequence=%lu\n",
                          static_cast<unsigned long long>(frame.run),
                          static_cast<unsigned long>(frame.sequence));
            return;
        }
        records[index] = {frame.run, frame.sequence, {}, 0};
        memcpy(records[index].source, item.source, 6);
        ++recordCount;
    } else if (records[index].duplicates < 255) {
        ++records[index].duplicates;
    }
    if (!persist()) {
        ++storageFailures;
        Serial.printf("STORE_FAIL run=%016llX sequence=%lu\n",
                      static_cast<unsigned long long>(frame.run),
                      static_cast<unsigned long>(frame.sequence));
        return; // Never acknowledge a packet that was not saved.
    }
    if (!esp_now_is_peer_exist(item.source)) {
        esp_now_peer_info_t peer{};
        memcpy(peer.peer_addr, item.source, 6);
        peer.channel = RadioChannel;
        if (esp_now_add_peer(&peer) != ESP_OK) {
            Serial.println("ACK_PEER_FAIL");
            return;
        }
    }
    RadioFrame ack{RadioMagic, RadioVersion, RadioAck, 0, frame.run, frame.sequence, 0};
    ack.check = radioCheck(ack);
    const esp_err_t sent = esp_now_send(item.source, reinterpret_cast<const uint8_t *>(&ack),
                                        sizeof(ack));
    Serial.printf("STORED run=%016llX sequence=%lu duplicate=%u ack_queue=%d\n",
                  static_cast<unsigned long long>(frame.run),
                  static_cast<unsigned long>(frame.sequence), records[index].duplicates,
                  static_cast<int>(sent));
}
} // namespace

void setup() {
    Serial.begin(115200);
    pinMode(ReadyLed, OUTPUT);
    digitalWrite(ReadyLed, LOW);
    incoming = xQueueCreate(16, sizeof(Inbound));
    if (!incoming || !storage.begin("hw-radio", false)) {
        Serial.println("RECEIVER FAILED storage_or_queue");
        return;
    }
    const size_t bytes = storage.getBytesLength("records");
    if (bytes <= sizeof(records) && bytes % sizeof(Record) == 0 &&
        (bytes == 0 || storage.getBytes("records", records, bytes) == bytes))
        recordCount = bytes / sizeof(Record);
    else {
        Serial.println("RECEIVER FAILED stored_report_invalid");
        return;
    }
    if (!WiFi.mode(WIFI_STA) || esp_wifi_set_channel(RadioChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_now_init() != ESP_OK || esp_now_register_recv_cb(onReceive) != ESP_OK) {
        Serial.println("RECEIVER FAILED radio_init");
        return;
    }
    Serial.printf("RECEIVER READY version=%u channel=%u mac=%s stored=%u\n", RadioVersion,
                  RadioChannel, WiFi.macAddress().c_str(), static_cast<unsigned>(recordCount));
    ready = true;
    report();
}

void loop() {
    Inbound item{};
    if (incoming && xQueueReceive(incoming, &item, pdMS_TO_TICKS(20)) == pdTRUE)
        handle(item);
    while (Serial.available()) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\n' || ch == '\r') {
            command.trim();
            if (command == "report")
                report();
            else if (command.length())
                Serial.println("UNKNOWN COMMAND (use report)");
            command = "";
        } else if (command.length() < 24)
            command += ch;
    }
    // The on-board LED pulses only while radio and storage are ready.
    static uint32_t lastToggle = 0;
    if (ready && millis() - lastToggle >= 500) {
        lastToggle = millis();
        digitalWrite(ReadyLed, !digitalRead(ReadyLed));
    }
}
