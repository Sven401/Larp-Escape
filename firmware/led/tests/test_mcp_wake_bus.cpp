#include "WakeInputs.h"
#include <Wire.h>
#include <cassert>
#include <cstdio>
TwoWire Wire;
int main() {
    auto &bus = larp::wake::mcpWakeBus();
    assert(bus.begin());
    assert(Wire.sda == 21 && Wire.scl == 22 && Wire.frequency == 100000 && Wire.timeout == 5);
    std::uint8_t value = 0;
    assert(bus.read(0x20, 0x13, value) && value == 0xd8 && Wire.sent[0] == 0x13);
    assert(Wire.address == 0x20 && Wire.requestCalls == 1 && !Wire.lastStop);
    Wire = {}; Wire.endResult = 2; value = 0x55;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55 && Wire.requestCalls == 0);
    Wire = {}; Wire.requestCount = 0;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55);
    Wire = {}; Wire.availableCount = 0;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55);
    Wire = {}; Wire.availableCount = 2;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55);
    Wire = {}; Wire.readValue = -1;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55);
    Wire = {}; Wire.writeCount = 0;
    assert(!bus.read(0x20, 0x13, value) && value == 0x55 && Wire.endCalls == 1 && Wire.lastStop);
    Wire = {};
    assert(bus.write(0x20, 0x01, 0xd8));
    assert(Wire.sent.size() == 2 && Wire.sent[0] == 1 && Wire.sent[1] == 0xd8 && Wire.lastStop);
    Wire = {}; Wire.endResult = 3;
    assert(!bus.write(0x20, 0x01, 0xd8));
    Wire = {}; Wire.writeCount = 0;
    assert(!bus.write(0x20, 0x01, 0xd8) && Wire.endCalls == 1 && Wire.lastStop);
    Wire = {}; Wire.beginOk = false;
    assert(!bus.begin());
    std::puts("Wake I2C adapter: pins/address, timeout, register transaction, NACK and short reads passed");
}
