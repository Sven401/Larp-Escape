#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_PN532.h>
#include <Adafruit_SPIDevice.h>
#include <DFPlayerMini_Fast.h>
#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <array>
#include <cstring>

#include "Config.h"
#include "GameRuntime/Pn532PassiveFrame.h"
#include "HardwareAcceptance/RadioProtocol.h"

using namespace hardware_acceptance;
namespace {
constexpr uint8_t LedPin = 15;
constexpr uint8_t LedCount = 23;
constexpr uint8_t WakeCount = 4;
constexpr uint8_t WakePins[WakeCount] = {15, 14, 12, 11};
constexpr uint8_t RowPins[9] = {15, 14, 13, 12, 11, 10, 9, 8, 8};
constexpr uint8_t ColPins[9] = {9, 0, 1, 2, 3, 4, 5, 6, 13};
constexpr uint8_t Peer[6] = {0xA8, 0x42, 0xE3, 0xCE, 0xDE, 0xD8};
constexpr uint8_t TagPage = 4; // First user-data page; never lock/configuration.
constexpr uint8_t NfcIrqPin = 27;

Adafruit_MCP23X17 mcp0;
Adafruit_MCP23X17 mcp1;
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
Adafruit_SPIDevice nfcTrace(PN532_SS, PN532_SCK, PN532_MISO, PN532_MOSI,
                           1000000, SPI_BITORDER_LSBFIRST, SPI_MODE0);
CRGB leds[LedCount];
HardwareSerial audioSerial(1);
DFPlayerMini_Fast player;
bool mcpReady = false, nfcReady = false, radioReady = false;
bool rawMatrix[81] = {}, stableMatrix[81] = {};
uint8_t matrixAgreement[81] = {}, matrixDown[81] = {}, matrixUp[81] = {};
bool rawWake[WakeCount] = {}, stableWake[WakeCount] = {};
uint8_t wakeAgreement[WakeCount] = {}, wakeDown[WakeCount] = {}, wakeUp[WakeCount] = {};
uint32_t lastScan = 0, scanMaxUs = 0, ledMaxUs = 0, ledFrames = 0;
bool lastBusy = false;
uint8_t armedUid[10] = {};
uint8_t armedUidLength = 0;
uint8_t tagSizeCode = 0;
uint32_t writeAttempts = 0;
String command;

struct AckEvent { RadioFrame frame; uint8_t source[6]; };
QueueHandle_t acks = nullptr;
volatile uint32_t ackQueueDrops = 0;
volatile uint32_t sendsOk = 0, sendsFailed = 0;

void onReceive(const uint8_t *source, const uint8_t *data, int length) {
    if (length != sizeof(RadioFrame) || !acks)
        return;
    AckEvent event{};
    memcpy(&event.frame, data, sizeof(event.frame));
    memcpy(event.source, source, 6);
    if (xQueueSend(acks, &event, 0) != pdTRUE)
        ++ackQueueDrops;
}
void onSent(const uint8_t *, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) ++sendsOk;
    else ++sendsFailed;
}

void printHex(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) Serial.printf("%02X", bytes[i]);
}

void scanInputs() {
    if (!mcpReady || millis() - lastScan < 35) return;
    lastScan = millis();
    const uint32_t started = micros();
    bool sensed[81] = {};
    for (uint8_t col = 0; col < 9; ++col) {
        mcp0.digitalWrite(ColPins[col], LOW);
        mcp0.pinMode(ColPins[col], OUTPUT);
        for (uint8_t row = 0; row < 9; ++row) {
            Adafruit_MCP23X17 &device = row == 8 ? mcp0 : mcp1;
            sensed[row * 9 + col] = device.digitalRead(RowPins[row]) == LOW;
        }
        mcp0.digitalWrite(ColPins[col], HIGH);
        mcp0.pinMode(ColPins[col], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < 81; ++i) {
        if (sensed[i] != rawMatrix[i]) { rawMatrix[i] = sensed[i]; matrixAgreement[i] = 1; }
        else if (matrixAgreement[i] < 2) ++matrixAgreement[i];
        if (matrixAgreement[i] >= 2 && stableMatrix[i] != sensed[i]) {
            stableMatrix[i] = sensed[i];
            uint8_t &count = sensed[i] ? matrixDown[i] : matrixUp[i];
            if (count < 255) ++count;
            Serial.printf("MATRIX row=%u col=%c state=%s count=%u\n", i / 9 + 1,
                          'A' + i % 9, sensed[i] ? "DOWN" : "UP", count);
        }
    }
    for (uint8_t i = 0; i < WakeCount; ++i) {
        const bool sensedWake = mcp0.digitalRead(WakePins[i]) == HIGH;
        if (sensedWake != rawWake[i]) { rawWake[i] = sensedWake; wakeAgreement[i] = 1; }
        else if (wakeAgreement[i] < 2) ++wakeAgreement[i];
        if (wakeAgreement[i] >= 2 && stableWake[i] != sensedWake) {
            stableWake[i] = sensedWake;
            uint8_t &count = sensedWake ? wakeDown[i] : wakeUp[i];
            if (count < 255) ++count;
            Serial.printf("WAKE input=%u state=%s count=%u\n", i + 1,
                          sensedWake ? "ACTIVE" : "RELEASED", count);
        }
    }
    const uint32_t duration = micros() - started;
    if (duration > scanMaxUs) scanMaxUs = duration;
}

void showLeds() {
    const uint32_t started = micros();
    FastLED.show();
    const uint32_t duration = micros() - started;
    if (duration > ledMaxUs) ledMaxUs = duration;
    ++ledFrames;
}

void ledSweep() {
    const CRGB colors[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    const char *names[3] = {"RED", "GREEN", "BLUE"};
    for (uint8_t i = 0; i < LedCount; ++i)
        for (uint8_t color = 0; color < 3; ++color) {
            fill_solid(leds, LedCount, CRGB::Black);
            leds[i] = colors[color];
            showLeds();
            Serial.printf("LED index=%u color=%s brightness=80 show_us=%lu\n", i,
                          names[color], static_cast<unsigned long>(ledMaxUs));
            delay(350);
        }
    fill_solid(leds, LedCount, CRGB::Black);
    showLeds();
    Serial.printf("LED_SWEEP_END frames=%lu max_show_us=%lu\n",
                  static_cast<unsigned long>(ledFrames), static_cast<unsigned long>(ledMaxUs));
}

void ledGroups() {
    for (uint8_t i = 0; i < LedCount; ++i)
        leds[i] = i < 8 ? CRGB::Red : (i < 16 ? CRGB::Green : CRGB::Blue);
    showLeds();
    Serial.println("LED_GROUPS red=0..7 green=8..15 blue=16..22");
    delay(2500);
    for (uint8_t brightness : {16, 32, 48, 64, 80}) {
        FastLED.setBrightness(brightness);
        showLeds();
        Serial.printf("LED_BRIGHTNESS value=%u max_show_us=%lu\n", brightness,
                      static_cast<unsigned long>(ledMaxUs));
        delay(700);
    }
    fill_solid(leds, LedCount, CRGB::Black);
    showLeds();
    FastLED.setBrightness(80);
}

bool selectTag(uint8_t *uid, uint8_t &length, uint8_t &sizeCode) {
    if (!nfcReady) return false;
    length = 0;
    if (!nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A)) {
        Serial.println("TAG_PROBE detection_start_failed"); return false;
    }
    const uint32_t deadline = millis() + 1200;
    while (digitalRead(NfcIrqPin) != LOW &&
           static_cast<int32_t>(deadline - millis()) > 0) delay(2);
    if (digitalRead(NfcIrqPin) != LOW) {
        Serial.println("TAG_PROBE detection_timeout"); return false;
    }
    if (!nfc.readDetectedPassiveTargetID(uid, &length)) {
        Serial.println("TAG_PROBE detection_frame_failed"); return false;
    }
    Serial.print("TAG_PROBE uid="); printHex(uid, length);
    Serial.printf(" uid_bytes=%u\n", length);
    if (length != 7) {
        Serial.println("TAG_PROBE unsupported_uid_length"); return false;
    }
    uint8_t cmd = 0x60, answer[10] = {}, answerLength = sizeof(answer);
    if (!nfc.inDataExchange(&cmd, 1, answer, &answerLength)) {
        Serial.println("TAG_PROBE get_version_failed"); return false;
    }
    Serial.print("TAG_PROBE version="); printHex(answer, answerLength);
    Serial.println();
    if (answerLength != 8 ||
        answer[0] != 0x00 || answer[1] != 0x04 || answer[2] != 0x04 ||
        answer[3] != 0x02 || answer[4] != 0x01 || answer[5] != 0x00 ||
        answer[7] != 0x03) return false;
    sizeCode = answer[6];
    return sizeCode == 0x0F || sizeCode == 0x11 || sizeCode == 0x13;
}

void tagScan() {
    uint8_t uid[10] = {}, length = 0, sizeCode = 0;
    if (!selectTag(uid, length, sizeCode)) {
        armedUidLength = 0;
        Serial.println("TAG_SCAN result=UNSUPPORTED_OR_MISSING (no write armed)");
        return;
    }
    memcpy(armedUid, uid, 7);
    armedUidLength = length;
    tagSizeCode = sizeCode;
    const unsigned capacity = sizeCode == 0x0F ? 144 : (sizeCode == 0x11 ? 504 : 888);
    Serial.print("TAG_SCAN result=ARMED uid="); printHex(uid, length);
    Serial.printf(" type=NTAG%u capacity_bytes=%u user_page=4\n",
                  sizeCode == 0x0F ? 213 : (sizeCode == 0x11 ? 215 : 216), capacity);
}

void tagRaw() {
    if (!nfcReady) { Serial.println("TAG_RAW result=NOT_READY"); return; }
    uint8_t command[3] = {PN532_COMMAND_INLISTPASSIVETARGET, 1,
                          PN532_MIFARE_ISO14443A};
    const uint32_t started = millis();
    const bool commandReady = nfc.sendCommandCheckAck(command, sizeof(command), 1200);
    Serial.printf("TAG_RAW command_ready=%u wait_ms=%lu irq=%u\n", commandReady,
                  static_cast<unsigned long>(millis() - started), digitalRead(NfcIrqPin));
    if (!commandReady) return;
    uint8_t frame[64] = {};
    const uint8_t readCommand = 0x03;
    const bool received = nfcTrace.write_then_read(&readCommand, 1, frame, sizeof(frame));
    Serial.printf("TAG_RAW spi_read=%u first32=", received);
    if (received) printHex(frame, 32);
    Serial.println();
    if (!received) return;
    larp::runtime::PassiveTarget target;
    const auto outcome = larp::runtime::parsePassiveTarget(frame, sizeof(frame), target);
    Serial.printf("TAG_RAW frame_outcome=%u uid_bytes=%u\n", static_cast<unsigned>(outcome),
                  target.length);
}

void tagRead() {
    uint8_t uid[10] = {}, length = 0, sizeCode = 0, data[16] = {};
    if (!selectTag(uid, length, sizeCode)) {
        Serial.println("TAG_READ result=UNSUPPORTED_OR_MISSING"); return;
    }
    if (!nfc.ntag2xx_ReadPage(TagPage, data)) {
        Serial.println("TAG_READ result=FAILED"); return;
    }
    Serial.print("TAG_READ result=OK uid="); printHex(uid, length);
    Serial.print(" page4_bytes="); printHex(data, 4);
    Serial.println();
}

void tagWrite(char variant) {
    const uint32_t attempt = ++writeAttempts;
    if (armedUidLength != 7 || (variant != 'A' && variant != 'B')) {
        Serial.printf("TAG_WRITE attempt=%lu result=NOT_ARMED\n", static_cast<unsigned long>(attempt));
        return;
    }
    uint8_t uid[10] = {}, length = 0, sizeCode = 0;
    if (!selectTag(uid, length, sizeCode) || length != armedUidLength ||
        sizeCode != tagSizeCode || memcmp(uid, armedUid, length) != 0) {
        Serial.printf("TAG_WRITE attempt=%lu result=TAG_MISMATCH_OR_MISSING\n",
                      static_cast<unsigned long>(attempt));
        return;
    }
    uint8_t expected[4] = {'L', 'A', '0', '1'};
    if (variant == 'B') { expected[1] = 'B'; expected[3] = '2'; }
    Serial.printf("TAG_WRITE attempt=%lu variant=%c page=4 expected=",
                  static_cast<unsigned long>(attempt), variant);
    printHex(expected, 4); Serial.println(" START");
    const bool sent = nfc.ntag2xx_WritePage(TagPage, expected);
    uint8_t readback[16] = {};
    const bool read = sent && nfc.ntag2xx_ReadPage(TagPage, readback);
    Serial.printf("TAG_WRITE attempt=%lu result=%s readback=", static_cast<unsigned long>(attempt),
                  read && memcmp(expected, readback, 4) == 0 ? "VERIFIED" : "UNCLEAR");
    if (read) printHex(readback, 4);
    else Serial.print("NONE");
    Serial.println();
}

void radioRun(uint64_t run, unsigned count) {
    if (!radioReady || !run || count == 0 || count > 50) {
        Serial.println("RADIO_RUN rejected (require ready, nonzero run, count 1..50)"); return;
    }
    Serial.printf("RADIO_RUN start=%016llX count=%u peer=A842E3CEDED8 channel=1\n",
                  static_cast<unsigned long long>(run), count);
    for (unsigned i = 1; i <= count; ++i) {
        RadioFrame frame{RadioMagic, RadioVersion, RadioData, 0, run, i, 0};
        frame.check = radioCheck(frame);
        const esp_err_t queued = esp_now_send(Peer, reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
        Serial.printf("RADIO_SEND run=%016llX sequence=%u queued=%d\n",
                      static_cast<unsigned long long>(run), i, static_cast<int>(queued));
        const uint32_t end = millis() + 750;
        bool acknowledged = false;
        while (static_cast<int32_t>(end - millis()) > 0) {
            AckEvent event{};
            if (xQueueReceive(acks, &event, pdMS_TO_TICKS(20)) == pdTRUE &&
                memcmp(event.source, Peer, 6) == 0 &&
                event.frame.magic == RadioMagic && event.frame.version == RadioVersion &&
                event.frame.type == RadioAck && event.frame.reserved == 0 &&
                event.frame.check == radioCheck(event.frame)) {
                Serial.printf("RADIO_ACK run=%016llX sequence=%lu\n",
                              static_cast<unsigned long long>(event.frame.run),
                              static_cast<unsigned long>(event.frame.sequence));
                if (event.frame.run == run && event.frame.sequence == i) acknowledged = true;
            }
            if (acknowledged) break;
        }
        Serial.printf("RADIO_RESULT run=%016llX sequence=%u ack=%u\n",
                      static_cast<unsigned long long>(run), i, acknowledged ? 1 : 0);
    }
    Serial.printf("RADIO_RUN_END send_ok=%lu send_fail=%lu ack_queue_drops=%lu\n",
                  static_cast<unsigned long>(sendsOk), static_cast<unsigned long>(sendsFailed),
                  static_cast<unsigned long>(ackQueueDrops));
}

void summary() {
    unsigned wakeComplete = 0, matrixComplete = 0;
    for (uint8_t i = 0; i < WakeCount; ++i) {
        if (wakeDown[i] >= 3 && wakeUp[i] >= 3) ++wakeComplete;
        Serial.printf("WAKE_SUMMARY input=%u active=%u released=%u\n", i + 1,
                      wakeDown[i], wakeUp[i]);
    }
    for (uint8_t i = 0; i < 81; ++i) {
        if (matrixDown[i] && matrixUp[i]) ++matrixComplete;
        Serial.printf("MATRIX_SUMMARY row=%u col=%c down=%u up=%u\n",
                      i / 9 + 1, 'A' + i % 9, matrixDown[i], matrixUp[i]);
    }
    Serial.printf("SUMMARY wake_3x=%u/4 matrix_touched=%u/81 scan_max_us=%lu led_frames=%lu led_max_us=%lu\n",
                  wakeComplete, matrixComplete, static_cast<unsigned long>(scanMaxUs),
                  static_cast<unsigned long>(ledFrames), static_cast<unsigned long>(ledMaxUs));
}

void dispatch(String line) {
    line.trim();
    if (line == "status") {
        Serial.printf("STATUS mcp=%u nfc=%u radio=%u mac=%s busy=%u\n", mcpReady,
                      nfcReady, radioReady, WiFi.macAddress().c_str(), digitalRead(DFBUSY) == LOW);
    } else if (line == "summary") summary();
    else if (line == "led sweep") ledSweep();
    else if (line == "led groups") ledGroups();
    else if (line == "tag scan") tagScan();
    else if (line == "tag raw") tagRaw();
    else if (line == "tag read") tagRead();
    else if (line == "tag write A") tagWrite('A');
    else if (line == "tag write B") tagWrite('B');
    else if (line == "audio info") {
        Serial.printf("AUDIO_INFO sd_tracks=%d busy=%u\n", player.numSdTracks(),
                      digitalRead(DFBUSY) == LOW);
    }
    else if (line.startsWith("audio ")) {
        const int track = line.substring(6).toInt();
        if (track > 0 && track <= 999) {
            player.playFromMP3Folder(track);
            Serial.printf("AUDIO requested_track=%d busy_at_send=%u\n", track,
                          digitalRead(DFBUSY) == LOW);
        } else Serial.println("AUDIO rejected track 1..999");
    } else if (line.startsWith("radio ")) {
        unsigned long long run = 0;
        unsigned count = 0;
        if (sscanf(line.c_str(), "radio %llx %u", &run, &count) == 2)
            radioRun(static_cast<uint64_t>(run), count);
        else Serial.println("RADIO syntax: radio <hex-run-id> <count>");
    } else if (line.length()) Serial.println("UNKNOWN COMMAND");
}
} // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("BOX_DIAGNOSTIC version=1");
    Wire.begin();
    mcpReady = mcp0.begin_I2C(0x20) && mcp1.begin_I2C(0x21);
    Serial.printf("MCP ready=%u addresses=0x20,0x21\n", mcpReady);
    if (mcpReady) {
        for (uint8_t pin = 0; pin < 16; ++pin) {
            mcp0.pinMode(pin, INPUT_PULLUP);
            mcp1.pinMode(pin, INPUT_PULLUP);
        }
        for (uint8_t i = 0; i < 81; ++i) {
            rawMatrix[i] = stableMatrix[i] = false;
            matrixAgreement[i] = 0;
        }
        for (uint8_t i = 0; i < WakeCount; ++i) {
            rawWake[i] = stableWake[i] = mcp0.digitalRead(WakePins[i]) == HIGH;
            Serial.printf("WAKE_IDLE input=%u level=%u\n", i + 1, stableWake[i]);
        }
    }
    const bool traceTransport = nfcTrace.begin();
    const bool nfcTransport = traceTransport && nfc.begin();
    pinMode(NfcIrqPin, INPUT_PULLUP);
    const uint32_t nfcVersion = nfcTransport ? nfc.getFirmwareVersion() : 0;
    const bool samReady = nfcVersion != 0 && nfc.SAMConfig();
    nfcReady = nfcTransport && nfcVersion != 0 && samReady;
    Serial.printf("PN532 transport=%u firmware=%08lX sam=%u ready=%u\n", nfcTransport,
                  static_cast<unsigned long>(nfcVersion), samReady, nfcReady);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
    FastLED.addLeds<WS2811, LedPin, GRB>(leds, LedCount).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(80);
    fill_solid(leds, LedCount, CRGB::Black);
    showLeds();
    pinMode(DFBUSY, INPUT);
    pinMode(DFWAKEUP, OUTPUT);
    digitalWrite(DFWAKEUP, HIGH);
    audioSerial.begin(9600, SERIAL_8N1, 25, 26);
    for (uint8_t i = 0; i < 4; ++i) {
        digitalWrite(DFWAKEUP, HIGH); delay(100);
        digitalWrite(DFWAKEUP, LOW); delay(100);
    }
    player.begin(audioSerial);
    player.volume(VOL);
    lastBusy = digitalRead(DFBUSY) == LOW;
    Serial.printf("DFPLAYER initialized=1 busy=%u\n", lastBusy);
    acks = xQueueCreate(16, sizeof(AckEvent));
    if (acks && WiFi.mode(WIFI_STA) &&
        esp_wifi_set_channel(RadioChannel, WIFI_SECOND_CHAN_NONE) == ESP_OK &&
        esp_now_init() == ESP_OK && esp_now_register_recv_cb(onReceive) == ESP_OK &&
        esp_now_register_send_cb(onSent) == ESP_OK) {
        esp_now_peer_info_t peer{};
        memcpy(peer.peer_addr, Peer, 6);
        peer.channel = RadioChannel;
        radioReady = esp_now_add_peer(&peer) == ESP_OK;
    }
    Serial.printf("RADIO ready=%u channel=%u peer=A842E3CEDED8 mac=%s\n", radioReady,
                  RadioChannel, WiFi.macAddress().c_str());
    Serial.println("BOX_DIAGNOSTIC READY commands: status, summary, led sweep, led groups, audio info/N, tag scan/raw/read/write A/write B, radio HEXRUN N");
}

void loop() {
    scanInputs();
    const bool busy = digitalRead(DFBUSY) == LOW;
    if (busy != lastBusy) {
        lastBusy = busy;
        Serial.printf("AUDIO_BUSY state=%u at_ms=%lu\n", busy,
                      static_cast<unsigned long>(millis()));
    }
    while (Serial.available()) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\r' || ch == '\n') {
            dispatch(command);
            command = "";
        } else if (command.length() < 80) command += ch;
    }
    delay(1);
}
