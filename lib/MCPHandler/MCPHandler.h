#ifndef MCPHANDLER_H
#define MCPHANDLER_H
#pragma once

#include "MCPConfig.h"
#include <Adafruit_I2CDevice.h>
#include <Adafruit_MCP23X17.h>
#include <cstdint>

// Borrows one physical expander and a stable configuration. A handler is never
// copied: diagnostic state and initialization belong to that device adapter.
class MCPHandler {
  public:
    MCPHandler(Adafruit_MCP23X17 &mcp, const MCPConfig &config, String name, TwoWire *wire = &Wire);
    MCPHandler(const MCPHandler &) = delete;
    MCPHandler &operator=(const MCPHandler &) = delete;
    MCPHandler(MCPHandler &&) = delete;
    MCPHandler &operator=(MCPHandler &&) = delete;

    // Startup policy belongs to the application. A failed I2C attempt returns
    // false and is not retried through the library's allocating begin_I2C path.
    bool begin();
    bool configurationValid() const;
    bool isReady() const;
    std::uint8_t i2cAddress() const;
    bool sharesDeviceWith(const MCPHandler &other) const;
    void printGpioStates();
    void printConfiguredGpioStates(bool verbose = false);
    bool writeGPIO(MCP_Pins gpio, uint8_t value);
    // Success means the I2C operations completed, not a physical contact check.
    // Failed reads clear their output and never encode failure as HIGH.
    bool readGPIO(MCP_Pins gpio, uint8_t &value);
    bool readGPIOAB(uint16_t &value);

  private:
    bool setPinMode(uint8_t pin, uint8_t mode);
    bool updateRegisterBit(uint8_t baseRegister, uint8_t pin, bool set);
    Adafruit_MCP23X17 &mcp;
    TwoWire *wire;
    // Shares the bus initialized once by mcp.begin_I2C; no second begin/retry.
    Adafruit_I2CDevice checkedDevice;
    uint16_t prevGpioState;
    const MCPConfig &config;
    String name;
    bool initialized;
    bool initializationAttempted;
};
#endif // MCPHANDLER_H
