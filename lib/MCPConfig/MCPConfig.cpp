#include "MCPConfig.h"

MCPConfig mcpConfigs[MAX_INSTANCES] = {
    {
        .i2c_addr = 0x20,
        .pinConfigs = {
            {MCP_A6, INPUT_MODE},
            {MCP_A5, INPUT_MODE},
            {MCP_B3, INPUT_MODE},
            {MCP_A0, INPUT_MODE},
            {MCP_B0, INPUT_MODE},
            // Configure additional pins as needed
        },
        .numPins = 5 // Number of configured pins for the first instance
    },
    {
        .i2c_addr = 0x21, .pinConfigs = {
                              {MCP_A0, INPUT_MODE}, {MCP_B0, INPUT_MODE},
                              // Configure additional pins as needed
                          },
        .numPins = 2 // Number of configured pins for the second instance
    }};