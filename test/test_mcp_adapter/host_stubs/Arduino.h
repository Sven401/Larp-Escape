#pragma once
// Only the Arduino platform boundary is replaced. Adafruit MCP/BusIO source
// files are compiled unchanged from the pinned dependency directories.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
using String = std::string;
constexpr uint8_t LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
constexpr uint8_t CHANGE = 3, FALLING = 4, RISING = 5;
constexpr int BIN = 2, DEC = 10, HEX = 16;
enum BitOrder { LSBFIRST = 0, MSBFIRST = 1 };
class Stream {
  public:
    template <typename T> void print(const T &, int = DEC) {}
    template <typename T> void println(const T &, int = DEC) {}
    void println() {}
};
extern Stream Serial;
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) {
    return LOW;
}
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
#define F(value) value
