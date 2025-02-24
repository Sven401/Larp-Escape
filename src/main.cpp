#include "Config.h"
#include "NFCReader.h"
#include "MCPHandler.h"
#include "GameState.h"
#include "KeyMatrix.h"
#include <vector>
#include "FastLED.h"
#include "twinklefox.h"
#include "DFMinniHandler.h"
#include "ripple.h"
#include "burst.h"
#include <esp_now.h>
#include <WiFi.h>


#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define DATA_PIN 15
#define VOLTS 5
#define MAX_MA 1000
#define NUM_LEDS 23
CRGBArray<NUM_LEDS> leds;

uint8_t receiverMAC[6] = {0xB0, 0xA7, 0x32, 0xF1, 0x86, 0x54};

using namespace fl;
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
ReichstagGame game(nfcReader, keymatrix, dfmHandler, twinkleFox, burst, ripples);


// Callback for message delivery status
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Delivery Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void ledTask(void *pvParameters) {
    const TickType_t delay = pdMS_TO_TICKS(33); // ~30 FPS (33ms per frame)
    
    while (true) {
        twinkleFox.draw(fl::Fx::DrawContext(millis(), leds)); 
        burst.draw();
        ripples[0].draw();
        ripples[1].draw();
        ripples[2].draw();
        ripples[3].draw();
        FastLED.show();
        vTaskDelay(delay);
    }
}

void wakeUpTask(void *pvParameters) {
    pinMode(DFWAKEUP, OUTPUT);

    while (true) {
        int onTime = random(200, 500);   // Random ON time between 100ms and 300ms
        int offTime = random(100, 400); // Random OFF time between 1.3s and 2.5s

        digitalWrite(DFWAKEUP, HIGH);
        vTaskDelay(pdMS_TO_TICKS(onTime));

        digitalWrite(DFWAKEUP, LOW);
        vTaskDelay(pdMS_TO_TICKS(offTime));
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
        // Create the LED update task on Core 1
        xTaskCreatePinnedToCore(
            ledTask,       // Function to run
            "LED Task",    // Task name
            2048,          // Stack size (2KB should be enough)
            nullptr,       // Task parameters
            1,             // Priority (1 = low priority)
            nullptr,// Task handle
            1              // Run on Core 1
        );
        xTaskCreatePinnedToCore(
            wakeUpTask,      
            "Blink Task",   
            1024,          
            nullptr,       
            1,             
            nullptr,       
            1              
        );
    pinMode(DFWAKEUP, OUTPUT);
    digitalWrite(DFWAKEUP, HIGH);
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        ESP.restart();
    }

    esp_now_register_send_cb(onSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;  

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        ESP.restart();
    }
}

void loop()
{
    game.machine.run();    
}
