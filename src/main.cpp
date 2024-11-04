#include "Config.h"
#include "NFCReader.h"
#include "MCPHandler.h"
#include "GameState.h"

// Objects
NFCReader nfcReader(PN532_IRQ);
MCPHandler mcpHandler1(mcp1, mcpConfigs[0]);
MCPHandler mcpHandler2(mcp2, mcpConfigs[1]);
Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;
GameState gameState;

void setup()
{
    Serial.begin(115200);
    Wire.begin();

    Serial.print("Beginning");

    // Initialize components
    mcpHandler1.begin(0x20);
    Serial.print("mcp1 ready");
    mcpHandler2.begin(0x21);
    Serial.print("mcp2 ready");

    nfcReader.begin();
    Serial.print("nfc should be ready");
}

void loop()
{
    Serial.print("loop start");
    mcpHandler1.printConfiguredGpioStates();
    delay(1000);
    mcpHandler2.printConfiguredGpioStates();

    nfcReader.loop();
    Serial.println();
}
