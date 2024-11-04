// MCPHandler.cpp
#include "MCPHandler.h"
#include "Config.h"

MCPHandler::MCPHandler(Adafruit_MCP23X17 &mcp, MCPConfig &config) : mcp(mcp), config(config), prevGpioState(0xFFFF) {}

void MCPHandler::begin(uint8_t i2c_addr)
{
    // Initialize first MCP23X17
    if (!mcp.begin_I2C(i2c_addr))
    {
        Serial.print("Error: MCP23XXX not found at adress");
        Serial.print(i2c_addr);
        ESP.restart();
    }
    for (int i = 0; i < config.numPins; i++)
    {
        uint8_t pin = config.pinConfigs[i].pin;   // Get pin number
        PinMode mode = config.pinConfigs[i].mode; // Get pin mode

        // Set the pin mode based on the configuration
        switch (mode)
        {
        case INPUT_PULLUP_MODE:
            mcp.pinMode(pin, INPUT_PULLUP);
            break;
        case INPUT_MODE:
            mcp.pinMode(pin, INPUT);
            break;
        case OUTPUT_MODE:
            mcp.pinMode(pin, OUTPUT);
            break;
        }
    }
}

void MCPHandler::printGpioStates()
{
    Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    // Ausgabe des vorherigen und aktuellen GPIO-Zustands als Binärzahlen für bessere Übersichtlichkeit
    Serial.print("Previous GPIO State: ");
    Serial.println(prevGpioState, BIN);
    uint16_t currentState = mcp.readGPIOAB();
    Serial.print("Current GPIO State: ");
    Serial.println(currentState, BIN);

    // Array für Tracking von Änderungen und deren Richtung (Fallend/Steigend)
    bool stateChanged[16] = {false};
    bool logicLevelRising[16] = {false};

    // Überprüfung der Zustände für jeden Pin und Ausgabe des Zustands
    for (int pin = 0; pin < 16; pin++)
    {
        bool current = (currentState & (1 << pin)) != 0;
        bool prev = (prevGpioState & (1 << pin)) != 0;

        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(": ");
        Serial.print(current ? "HIGH  " : "LOW  ");

        // Wenn sich der Zustand geändert hat, merken wir uns die Änderung und Richtung
        if (current != prev)
        {
            stateChanged[pin] = true;
            logicLevelRising[pin] = current; // HIGH ist steigend, LOW ist fallend
        }
    }

    // Zusammenfassende Ausgabe aller Pins, die sich geändert haben
    Serial.print("Changes Detected:");
    for (int pin = 0; pin < 16; pin++)
    {
        if (stateChanged[pin])
        {
            Serial.print("Pin ");
            Serial.print(pin);
            Serial.print(" changed to ");
            Serial.print(logicLevelRising[pin] ? "HIGH" : "LOW");
            Serial.println(logicLevelRising[pin] ? " (rising)" : " (falling)");
        }
    }

    // Aktualisieren des vorherigen Zustands
    prevGpioState = currentState;
}
void MCPHandler::printConfiguredGpioStates(bool verbose)
{
    if (verbose)
    {
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        Serial.print("Previous GPIO State: ");
        Serial.println(prevGpioState, BIN);
    }

    // Array für Tracking von Änderungen und deren Richtung (Fallend/Steigend)
    bool stateChanged[NUM_PINS] = {false};
    bool logicLevelRising[NUM_PINS] = {false};

    // Überprüfung der Zustände für nur konfigurierte Pins und Ausgabe des Zustands
    for (int i = 0; i < config.numPins; i++)
    {
        uint8_t pin = config.pinConfigs[i].pin;      // Get the configured pin number
        bool current = mcp.digitalRead(pin) == HIGH; // Read the pin state
        bool prev = (prevGpioState & (1 << pin)) != 0;
        if (verbose)
        {
            Serial.print("Pin ");
            Serial.print(pin);
            Serial.print(": ");
            Serial.print(current ? "HIGH  " : "LOW  ");
        }

        // Wenn sich der Zustand geändert hat, merken wir uns die Änderung und Richtung
        if (current != prev)
        {
            stateChanged[i] = true;
            logicLevelRising[i] = current; // HIGH ist steigend, LOW ist fallend
        }
    }

    // Zusammenfassende Ausgabe aller Pins, die sich geändert haben
    for (int i = 0; i < config.numPins; i++)
    {
        if (stateChanged[i])
        {
            Serial.print("Pin ");
            Serial.print(config.pinConfigs[i].pin);
            Serial.print(" changed to ");
            Serial.print(logicLevelRising[i] ? "HIGH" : "LOW");
            Serial.println(logicLevelRising[i] ? " (rising)" : " (falling)");
        }
    }

    // Aktualisieren des vorherigen Zustands
    prevGpioState = mcp.readGPIOAB(); // Optional: Update to reflect actual states if necessary
}
