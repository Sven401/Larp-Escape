#include "MCPHandler.h"
#include "GameRuntime/MatrixConfiguration.h"

MCPHandler::MCPHandler(Adafruit_MCP23X17 &mcp, const MCPConfig &config, String name, TwoWire *wire)
    : mcp(mcp), wire(wire), checkedDevice(config.i2c_addr, wire), prevGpioState(0xFFFF),
      config(config), name(name), initialized(false), initializationAttempted(false) {}

bool MCPHandler::configurationValid() const {
    if (config.numPins > NUM_PINS ||
        config.numPins > larp::runtime::MatrixConfiguration::PinsPerHandler)
        return false;
    std::array<larp::runtime::ExpanderPinConfiguration,
               larp::runtime::MatrixConfiguration::PinsPerHandler>
        pins = {};
    for (std::size_t position = 0; position < config.numPins; ++position) {
        int normalizedMode = -1;
        switch (config.pinConfigs[position].mode) {
        case INPUT_MODE:
            normalizedMode = 0;
            break;
        case INPUT_PULLUP_MODE:
            normalizedMode = 1;
            break;
        case OUTPUT_MODE:
            normalizedMode = 2;
            break;
        default:
            return false;
        }
        pins[position] = {config.pinConfigs[position].pin, normalizedMode};
    }
    return larp::runtime::isValidExpanderConfiguration(config.i2c_addr, pins, config.numPins);
}

bool MCPHandler::begin() {
    if (!wire || !configurationValid())
        return false;
    if (initialized)
        return true;
    if (initializationAttempted)
        return false;
    initializationAttempted = true;
    if (!mcp.begin_I2C(config.i2c_addr, wire)) {
        Serial.print("MCP23XXX not found at address ");
        Serial.println(config.i2c_addr);
        return false;
    }
    Serial.print("MCP23XXX found at address ");
    Serial.println(config.i2c_addr);
    for (std::size_t position = 0; position < config.numPins; ++position) {
        const uint8_t pin = config.pinConfigs[position].pin;
        switch (config.pinConfigs[position].mode) {
        case INPUT_MODE:
            if (!setPinMode(pin, INPUT))
                return false;
            break;
        case INPUT_PULLUP_MODE:
            if (!setPinMode(pin, INPUT_PULLUP))
                return false;
            break;
        case OUTPUT_MODE:
            if (!setPinMode(pin, OUTPUT))
                return false;
            break;
        }
    }
    // Output latch levels are preserved. Choosing startup HIGH levels requires
    // a wiring/timing check and is not inferred from configuration validation.
    initialized = true;
    return true;
}

bool MCPHandler::isReady() const {
    return initialized;
}
std::uint8_t MCPHandler::i2cAddress() const {
    return config.i2c_addr;
}
bool MCPHandler::sharesDeviceWith(const MCPHandler &other) const {
    return &mcp == &other.mcp;
}

void MCPHandler::printGpioStates() {
    if (!initialized)
        return;
    // Diagnostics read levels without changing the matrix pin directions.
    uint16_t currentState = 0;
    if (!readGPIOAB(currentState)) {
        Serial.println("MCP GPIO unavailable");
        return;
    }

    // Array für Tracking von Änderungen und deren Richtung (Fallend/Steigend)
    bool stateChanged[16] = {false};
    bool logicLevelRising[16] = {false};

    // Überprüfung der Zustände für jeden Pin und Ausgabe des Zustands
    for (int pin = 0; pin < 16; pin++) {
        bool current = (currentState & (1U << pin)) != 0;
        bool prev = (prevGpioState & (1U << pin)) != 0;

        // Wenn sich der Zustand geändert hat, merken wir uns die Änderung und Richtung
        if (current != prev) {
            stateChanged[pin] = true;
            logicLevelRising[pin] = current; // HIGH ist steigend, LOW ist fallend
        }
    }

    // Zusammenfassende Ausgabe aller Pins, die sich geändert haben
    for (int pin = 0; pin < 16; pin++) {
        if (stateChanged[pin]) {
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
void MCPHandler::printConfiguredGpioStates(bool verbose) {
    if (!initialized || !configurationValid())
        return;
    uint16_t currentState = 0;
    if (!readGPIOAB(currentState)) {
        Serial.println("MCP GPIO unavailable");
        return;
    }
    if (verbose) {
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        Serial.print("Previous GPIO State: ");
        Serial.println(prevGpioState, BIN);
    }

    // Array für Tracking von Änderungen und deren Richtung (Fallend/Steigend)
    bool stateChanged[NUM_PINS] = {false};
    bool logicLevelRising[NUM_PINS] = {false};

    // Überprüfung der Zustände für nur konfigurierte Pins und Ausgabe des Zustands
    for (int i = 0; i < config.numPins; i++) {
        uint8_t pin = config.pinConfigs[i].pin; // Get the configured pin number
        bool current = (currentState & (1U << pin)) != 0;
        bool prev = (prevGpioState & (1U << pin)) != 0;
        if (verbose) {
            Serial.print("Pin ");
            Serial.print(pin);
            Serial.print(": ");
            Serial.print(current ? "HIGH  " : "LOW  ");
        }

        // Wenn sich der Zustand geändert hat, merken wir uns die Änderung und Richtung
        if (current != prev) {
            stateChanged[i] = true;
            logicLevelRising[i] = current; // HIGH ist steigend, LOW ist fallend
        }
    }

    // Zusammenfassende Ausgabe aller Pins, die sich geändert haben
    for (int i = 0; i < config.numPins; i++) {
        if (stateChanged[i]) {
            Serial.print("Pin ");
            Serial.print(config.pinConfigs[i].pin);
            Serial.print(" changed to ");
            Serial.print(logicLevelRising[i] ? "HIGH" : "LOW");
            Serial.println(logicLevelRising[i] ? " (rising)" : " (falling)");
        }
    }

    // Aktualisieren des vorherigen Zustands
    prevGpioState = currentState;
}

// MCP23017 BANK=0 register layout, as used by Adafruit MCP23017 2.3.2.
// Use the bool BusIO overloads: scalar read()/RegisterBits erase I2C errors.
bool MCPHandler::updateRegisterBit(uint8_t baseRegister, uint8_t pin, bool set) {
    const uint16_t address = static_cast<uint16_t>(baseRegister * 2U + pin / 8U);
    Adafruit_BusIO_Register reg(&checkedDevice, address);
    uint8_t value = 0;
    if (!reg.read(&value))
        return false;
    const uint8_t mask = static_cast<uint8_t>(1U << (pin % 8U));
    value = set ? static_cast<uint8_t>(value | mask) : static_cast<uint8_t>(value & ~mask);
    return reg.write(value);
}

bool MCPHandler::setPinMode(uint8_t pin, uint8_t mode) {
    return updateRegisterBit(MCP23XXX_IODIR, pin, mode != OUTPUT) &&
           updateRegisterBit(MCP23XXX_GPPU, pin, mode == INPUT_PULLUP);
}

bool MCPHandler::writeGPIO(MCP_Pins gpio, uint8_t value) {
    if (!initialized || !larp::runtime::isValidMatrixPin(static_cast<int>(gpio)) ||
        (value != LOW && value != HIGH))
        return false;
    if (!setPinMode(static_cast<uint8_t>(gpio), OUTPUT))
        return false;
    // Read the output latch, not sampled inputs, when preserving other outputs.
    return updateRegisterBit(MCP23XXX_OLAT, static_cast<uint8_t>(gpio), value == HIGH);
}

bool MCPHandler::readGPIO(MCP_Pins gpio, uint8_t &value) {
    value = LOW;
    if (!initialized || !larp::runtime::isValidMatrixPin(static_cast<int>(gpio)) ||
        !setPinMode(static_cast<uint8_t>(gpio), INPUT_PULLUP))
        return false;
    Adafruit_BusIO_Register reg(&checkedDevice,
                                MCP23XXX_GPIO * 2U + static_cast<uint8_t>(gpio) / 8U);
    uint8_t port = 0;
    if (!reg.read(&port))
        return false;
    value = (port & (1U << (static_cast<uint8_t>(gpio) % 8U))) ? HIGH : LOW;
    return true;
}

bool MCPHandler::readGPIOAB(uint16_t &value) {
    value = 0;
    if (!initialized)
        return false;
    Adafruit_BusIO_Register reg(&checkedDevice, MCP23XXX_GPIO * 2U, 2);
    return reg.read(&value);
}
