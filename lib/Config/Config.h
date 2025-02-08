
#ifndef CONFIG_H
#define CONFIG_H

// Config.h
#pragma once

#include <Adafruit_PN532.h>
#include <Adafruit_MCP23X17.h>

// PN532 (NFC) Settings
#define PN532_SCK 18
#define PN532_MOSI 23
#define PN532_MISO 19
#define PN532_SS 5
#define PN532_IRQ 27
#define DELAY_BETWEEN_CARDS 500

#define PUPPENSPIELERID 1

extern Adafruit_PN532 nfc;
extern Adafruit_MCP23X17 mcp1, mcp2;
#endif // CONFIG_H