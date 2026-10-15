#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
class TwoWire {
 public:
    bool beginOk = true;
    int sda = -1, scl = -1, beginCalls = 0;
    std::uint32_t frequency = 0;
    std::uint16_t timeout = 0;
    std::uint8_t address = 0, endResult = 0;
    bool lastStop = false;
    int endCalls = 0, requestCalls = 0, availableCount = 1, readValue = 0xd8;
    std::size_t writeCount = 1, requestCount = 1;
    std::vector<std::uint8_t> sent;
    void setTimeOut(std::uint16_t value) { timeout = value; }
    bool begin(int data, int clock, std::uint32_t hz) {
        sda = data; scl = clock; frequency = hz; ++beginCalls; return beginOk;
    }
    void beginTransmission(std::uint8_t target) { address = target; sent.clear(); }
    std::size_t write(std::uint8_t value) { sent.push_back(value); return writeCount; }
    std::uint8_t endTransmission(bool stop) { ++endCalls; lastStop = stop; return endResult; }
    std::size_t requestFrom(std::uint8_t target, std::size_t size, bool stop) {
        if (target != 0x20 || size != 1 || !stop) return 0;
        ++requestCalls; return requestCount;
    }
    int available() { return availableCount; }
    int read() { return readValue; }
};
extern TwoWire Wire;
