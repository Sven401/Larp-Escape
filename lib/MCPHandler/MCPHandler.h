// MCPHandler.h
#ifndef MCPHANDLER_H
#define MCPHANDLER_H
#pragma once
#include <Adafruit_MCP23X17.h>
#include "MCPConfig.h"

class MCPHandler
{
public:
    MCPHandler(Adafruit_MCP23X17 &mcp, MCPConfig &config, String name);
    void begin(uint8_t i2c_addr = (uint8_t)32U);
    void printGpioStates();
    void printConfiguredGpioStates(bool verbose = false);
    void writeGPIO(MCP_Pins gpio, uint8_t value);
    uint16_t readGPIO(MCP_Pins gpio);

private:
    Adafruit_MCP23X17 &mcp;
    uint16_t prevGpioState;
    MCPConfig &config;
    String name;
};
#endif // MCPHANDLER_H