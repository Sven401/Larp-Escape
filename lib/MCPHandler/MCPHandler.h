// MCPHandler.h
#ifndef MCPHANDLER_H
#define MCPHANDLER_H
#pragma once
#include <Adafruit_MCP23X17.h>
#include "MCPConfig.h"

class MCPHandler
{
public:
    MCPHandler(Adafruit_MCP23X17 &mcp, MCPConfig &config);
    void begin(uint8_t i2c_addr = (uint8_t)32U);
    void printGpioStates();
    void printConfiguredGpioStates(bool verbose = false);

private:
    Adafruit_MCP23X17 &mcp;
    uint16_t prevGpioState;
    MCPConfig &config;
};
#endif // MCPHANDLER_H