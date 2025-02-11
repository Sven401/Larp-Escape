#include "Config.h"
#include "NFCReader.h"
#include "MCPHandler.h"
#include "GameState.h"
#include "KeyMatrix.h"
#include <vector>
#include "FastLED.h"
#include "twinklefox.h"
#include "DFMinniHandler.h"

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define DATA_PIN 15
#define VOLTS 5
#define MAX_MA 1000
#define NUM_LEDS 23
CRGBArray<NUM_LEDS> leds;

using namespace fl;
TwinkleFox twinkleFox(NUM_LEDS);


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

unsigned long lastMillis = 0;

void setup()
{
    Serial.begin(115200);
    Wire.begin();

    Serial.print("Beginning");
    setupTransitions();
    keymatrix.begin();
    nfcReader.begin();
    dfmHandler.begin();
    Serial.print("nfc should be ready");
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);

    keymatrix.printMatrixState();
}

void loop()
{
    EVERY_N_SECONDS(1)
    {
        mcpHandler1.printGpioStates();
    }
    EVERY_N_SECONDS(1)
    {
        mcpHandler2.printGpioStates();
    }

    EVERY_N_SECONDS(1)
    {
        gameState.run();
    }

    EVERY_N_SECONDS(SECONDS_PER_PALETTE)
    {
        twinkleFox.chooseNextColorPalette(twinkleFox.targetPalette);
    }
    twinkleFox.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    EVERY_N_SECONDS(1)
    {
        keymatrix.printMatrixState();
    }
    
    EVERY_N_SECONDS(2){
        Serial.print("Attempting to read NFC card... ");
        auto card = nfcReader.getCard();
        if (card == nullptr) {
            Serial.println("No card detected.");
        } else {
            Serial.println("Card detected!");
            Serial.print("Card UID: ");
            for (int i = 0; i < 7; i++) {
                Serial.print(nfcReader.uid[i], HEX);
                if (i < 6) {
                    Serial.print(":");
                }
            }
            Serial.println();
        }
    }

    EVERY_N_SECONDS(1)
    {
        if (!dfmHandler.isBusy())
        {
            Serial.println("DFPlayer is not busy, attempting to play track 111.");
            dfmHandler.playTrack(111);
            delay(500); // Small delay to allow the command to be processed
            if (dfmHandler.isBusy()) {
                Serial.println("Track 111 is now playing.");
            } else {
                Serial.println("Failed to start playing track 111.");
            }
        } else {
            Serial.println("DFPlayer is busy, cannot play track 111.");
        }
    }
}
