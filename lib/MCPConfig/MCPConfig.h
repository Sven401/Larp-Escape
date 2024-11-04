#include <Arduino.h>

#ifndef MCP_CONFIG_H
#define MCP_CONFIG_H

#define MAX_INSTANCES 8 // Maximum number of MCP instances
#define NUM_PINS 16     // Maximum number of pins per instance

enum MCP_Pins
{
    MCP_A0,
    MCP_A1,
    MCP_A2,
    MCP_A3,
    MCP_A4,
    MCP_A5,
    MCP_A6,
    MCP_A7,
    MCP_B0,
    MCP_B1,
    MCP_B2,
    MCP_B3,
    MCP_B4,
    MCP_B5,
    MCP_B6,
    MCP_B7
};

enum PinMode
{
    INPUT_MODE,        // Use INPUT_MODE instead of INPUT to avoid conflict
    INPUT_PULLUP_MODE, // Will be 1
    OUTPUT_MODE        // Will be 2
};

// Structure to hold pin configurations
struct PinConfig
{
    uint8_t pin;  // Pin number
    PinMode mode; // Pin mode (INPUT_PULLUP, INPUT, OUTPUT)
};

// Structure for MCP instance configuration
struct MCPConfig
{
    uint8_t i2c_addr;               // I2C address for the MCP instance
    PinConfig pinConfigs[NUM_PINS]; // Pin configurations for this instance
    uint8_t numPins;                // Number of configured pins
};

// Define configurations for multiple MCP instances
extern MCPConfig mcpConfigs[MAX_INSTANCES];

#endif