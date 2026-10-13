#ifndef MCPHANDLER_H
#define MCPHANDLER_H
#pragma once

#include "MCPConfig.h"
#include <Adafruit_MCP23X17.h>
#include <cstdint>

// Borrows one physical expander and a stable configuration. A handler is never
// copied: diagnostic state and initialization belong to that device adapter.
class MCPHandler {
  public:
    MCPHandler(Adafruit_MCP23X17 &mcp, const MCPConfig &config, String name);
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
    // Uninitialized/invalid reads return HIGH, the inactive matrix level.
    uint16_t readGPIO(MCP_Pins gpio);

  private:
    Adafruit_MCP23X17 &mcp;
    uint16_t prevGpioState;
    const MCPConfig &config;
    String name;
    bool initialized;
    bool initializationAttempted;
};
#endif // MCPHANDLER_H
