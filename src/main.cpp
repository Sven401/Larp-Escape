#include "Config.h"
#include <array>
#include <atomic>
#include <cstdint>
#include "NFCReader.h"
#include "MCPHandler.h"
#include "Gamestate.h"
#include "Keymatrix.h"
#include <vector>
#include "FastLED.h"
#include "twinklefox.h"
#include "DFMinniHandler.h"
#include "ArduinoClock.h"
#include "GameLightEffects.h"
#include "EspNowCommunicator.h"
#include "ripple.h"
#include "burst.h"
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>


#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define DATA_PIN 15
#define VOLTS 5
#define MAX_MA 1000
#define NUM_LEDS 23
CRGBArray<NUM_LEDS> leds;

const std::array<std::uint8_t, 6> receiverMAC = {{0xB0, 0xA7, 0x32, 0xF1, 0x86, 0x54}};

using fl::MyTwinkleFox;
MyTwinkleFox twinkleFox(NUM_LEDS);
Burst burst (30,leds, NUM_LEDS);
Ripple ripples[4] = {
    Ripple(NUM_LEDS*1/4, leds, 30, NUM_LEDS),
    Ripple(NUM_LEDS*2/4, leds, 30, NUM_LEDS),
    Ripple(NUM_LEDS*3/4, leds, 30, NUM_LEDS),
    Ripple(NUM_LEDS*4/4, leds, 30, NUM_LEDS)
};



// Objects
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
NFCReader nfcReader(nfc);
Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;
MCPHandler mcpHandler1(mcp1, mcpConfigs[0], "MCP1");
MCPHandler mcpHandler2(mcp2, mcpConfigs[1], "MCP2");
std::vector<MCPHandler> handlers = {mcpHandler1, mcpHandler2};
KeyMatrix keymatrix(handlers, ROWMAP, COLMAP);
HardwareSerial mySoftwareSerial(1);
DFPlayerMini_Fast myMP3;
DFMinniHandler dfmHandler(mySoftwareSerial, myMP3, DFBUSY, DFWAKEUP);
ArduinoClock gameClock;
GameLightEffects gameLightEffects(twinkleFox, burst, ripples, gameClock, leds, NUM_LEDS);
EspNowCommunicator communicator(receiverMAC);
ReichstagGame game(nfcReader, keymatrix, keymatrix, dfmHandler, gameClock, gameLightEffects, communicator);


// Callback for message delivery status
struct SendStatusEvent
{
    esp_now_send_status_t status;
};

QueueHandle_t sendStatusQueue = nullptr;
std::atomic<std::uint32_t> droppedSendStatusEvents(0U);
std::uint32_t maximumSendStatusQueueDepth = 0U;
std::uint32_t maximumMainLoopDurationMicroseconds = 0U;
std::uint32_t lastRuntimeReportMilliseconds = 0U;

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    (void)mac_addr;
    const SendStatusEvent event = {status};
    if (sendStatusQueue == nullptr || xQueueSend(sendStatusQueue, &event, 0U) != pdTRUE)
    {
        droppedSendStatusEvents.fetch_add(1U, std::memory_order_relaxed);
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    Serial.print("Beginning");
    keymatrix.begin();
    nfcReader.begin();
    dfmHandler.begin();
    Serial.print("nfc should be ready");
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);

    keymatrix.printMatrixState();
    twinkleFox.currentPalette = MutedAllColors_p;
    twinkleFox.setTwinkleDensity(1);
    twinkleFox.setTwinkleSpeed(1);
    FastLED.setBrightness(100);
    if (!gameLightEffects.begin()) {
        Serial.println("Failed to start LED service");
        ESP.restart();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    sendStatusQueue = xQueueCreate(8U, sizeof(SendStatusEvent));
    if (sendStatusQueue == nullptr) {
        Serial.println("Failed to create ESP-NOW status queue");
        ESP.restart();
        return;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        ESP.restart();
    }

    if (esp_now_register_send_cb(onSent) != ESP_OK) {
        Serial.println("Failed to register ESP-NOW send callback");
        ESP.restart();
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC.data(), receiverMAC.size());
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;  

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        ESP.restart();
    }
}

void loop()
{
    const std::uint32_t loopStarted = micros();
    dfmHandler.service();

    SendStatusEvent event;
    const std::uint32_t pendingStatusEvents = static_cast<std::uint32_t>(
        sendStatusQueue == nullptr ? 0U : uxQueueMessagesWaiting(sendStatusQueue));
    if (pendingStatusEvents > maximumSendStatusQueueDepth)
    {
        maximumSendStatusQueueDepth = pendingStatusEvents;
    }
    while (sendStatusQueue != nullptr &&
           xQueueReceive(sendStatusQueue, &event, 0U) == pdTRUE)
    {
        Serial.print("Delivery Status: ");
        Serial.println(event.status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    }

    game.machine.run();

    const std::uint32_t loopDuration = micros() - loopStarted;
    if (loopDuration > maximumMainLoopDurationMicroseconds)
    {
        maximumMainLoopDurationMicroseconds = loopDuration;
    }

    const std::uint32_t now = millis();
    if ((now - lastRuntimeReportMilliseconds) >= 10000U)
    {
        lastRuntimeReportMilliseconds = now;
        Serial.print("Runtime max loop us: ");
        Serial.println(maximumMainLoopDurationMicroseconds);
        Serial.print("LED min free stack bytes: ");
        Serial.println(gameLightEffects.minimumStackFreeBytes());
        Serial.print("LED max queue depth: ");
        Serial.println(gameLightEffects.maximumQueueDepth());
        Serial.print("LED max render us: ");
        Serial.println(gameLightEffects.maximumRenderDurationMicroseconds());
        Serial.print("ESP-NOW max queue depth: ");
        Serial.println(maximumSendStatusQueueDepth);
        Serial.print("LED command drops: ");
        Serial.println(gameLightEffects.droppedCommandCount());
        Serial.print("ESP-NOW status drops: ");
        Serial.println(droppedSendStatusEvents.load(std::memory_order_relaxed));
    }
}
