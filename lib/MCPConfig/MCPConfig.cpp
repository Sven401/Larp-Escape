#include "MCPConfig.h"

MCPConfig mcpConfigs[MAX_INSTANCES] = {
    {
        .i2c_addr = 0x20,
        .pinConfigs = {
            {MCP_B7, INPUT_PULLUP_MODE},
            {MCP_B6, INPUT_PULLUP_MODE},
            {MCP_B5, INPUT_PULLUP_MODE},
            {MCP_B4, INPUT_PULLUP_MODE},
            {MCP_B3, INPUT_PULLUP_MODE},
            {MCP_B2, INPUT_PULLUP_MODE},
            {MCP_B1, INPUT_PULLUP_MODE},
            {MCP_B0, INPUT_PULLUP_MODE},
            // Configure additional pins as needed
        },
        .numPins = 8 // Number of configured pins for the first instance
    },
    {
        .i2c_addr = 0x21, .pinConfigs = {
                              {MCP_B0, INPUT_PULLUP_MODE}, {MCP_B1, OUTPUT_MODE}, {MCP_A0, OUTPUT_MODE}, {MCP_A1, OUTPUT_MODE}, {MCP_A2, OUTPUT_MODE}, {MCP_A3, OUTPUT_MODE}, {MCP_A4, OUTPUT_MODE}, {MCP_A5, OUTPUT_MODE}, {MCP_A6, OUTPUT_MODE}, {MCP_A7, OUTPUT_MODE},
                              // Configure additional pins as needed
                          },
        .numPins = 10 // Number of configured pins for the second instance
    }};