
#ifndef CONFIG_H
#define CONFIG_H

// Config.h
#pragma once

#include <Adafruit_PN532.h>
#include <Adafruit_MCP23X17.h>
#include "MCPConfig.h"
#include <map>
#include "OptionConfig.h"
#include <FastLED.h>

#define PN532_SCK  (16)
#define PN532_MOSI (23)
#define PN532_SS   (4)
#define PN532_MISO (17)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (27)
#define PN532_RESET (34)
#define DELAY_BETWEEN_CARDS 500

#define DFBUSY 14 // Define the pin number for DFBUSY
#define DFWAKEUP 13 // Define the pin number for DFWAKEUP

const uint8_t VOL = 25; // Volume level

#define PUPPENSPIELERID 1

extern Adafruit_PN532 nfc;
extern Adafruit_MCP23X17 mcp1, mcp2;

#endif // CONFIG_H

const std::map<int, std::pair<int, MCP_Pins>> ROWMAP = {
    {1, {1, MCP_B7}},
    {2, {1, MCP_B6}},
    {3, {1, MCP_B5}},
    {4, {1, MCP_B4}},
    {5, {1, MCP_B3}},
    {6, {1, MCP_B2}},
    {7, {1, MCP_B1}},
    {8, {1, MCP_B0}},
    {9, {0, MCP_B0}},
};

const std::map<ColLetter, std::pair<int, MCP_Pins>> COLMAP = {
    {A, {0, MCP_B1}},
    {B, {0, MCP_A0}},
    {C, {0, MCP_A1}},
    {D, {0, MCP_A2}},
    {E, {0, MCP_A3}},
    {F, {0, MCP_A4}},
    {G, {0, MCP_A5}},
    {H, {0, MCP_A6}},
    {I, {0, MCP_A7}},
};

