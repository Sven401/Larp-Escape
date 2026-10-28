#pragma once
#include "Arduino.h"
constexpr uint8_t SPI_MODE0 = 0, SPI_MODE1 = 1, SPI_MODE2 = 2, SPI_MODE3 = 3;
class SPISettings {
  public:
    SPISettings(uint32_t, BitOrder, uint8_t) {}
};
class SPIClass {
  public:
    void begin() {}
    void beginTransaction(const SPISettings &) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t value) {
        return value;
    }
    void transfer(uint8_t *, std::size_t) {}
};
extern SPIClass SPI;
