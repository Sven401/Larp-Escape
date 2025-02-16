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
MyTwinkleFox twinkleFox(NUM_LEDS);


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
ReichstagGame game(nfcReader, keymatrix, dfmHandler, twinkleFox);


unsigned long lastMillis = 0;

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
}

void loop()
{
    game.machine.run();
    twinkleFox.draw(fl::Fx::DrawContext(millis(), leds));
    FastLED.show();
}
