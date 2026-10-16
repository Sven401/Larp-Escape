#include "WakeInputs.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>

using namespace larp::wake;
class Bus final : public IRegisterBus {
 public:
    std::array<std::uint8_t, 0x16> registers{};
    std::vector<std::pair<std::uint8_t, std::uint8_t>> writes;
    unsigned begins = 0, reads = 0, gpioReads = 0;
    int failRead = -1, failWrite = -1;
    bool beginOk = true, ignoreWrite = false;
    Bus() {
        registers.fill(0x25);
        registers[0x05] = 2; registers[0x15] = 4;
        registers[0x0a] = registers[0x0b] = 0;
        registers[0x03] = 0xed; registers[0x0d] = 3;
    }
    bool begin() noexcept override { ++begins; return beginOk; }
    bool read(std::uint8_t address, std::uint8_t reg, std::uint8_t &value) noexcept override {
        assert(address == 0x20 && reg < registers.size());
        if (reg == 0x13) ++gpioReads;
        if (static_cast<int>(reads++) == failRead) return false;
        value = registers[reg]; return true;
    }
    bool write(std::uint8_t address, std::uint8_t reg, std::uint8_t value) noexcept override {
        assert(address == 0x20 && reg < registers.size());
        const bool fail = static_cast<int>(writes.size()) == failWrite;
        writes.emplace_back(reg, value);
        if (fail) return false;
        if (!ignoreWrite) registers[reg] = value;
        return true;
    }
};
static void preservedConfiguration() {
    Bus bus; const auto before = bus.registers; WakeInputs reader(bus);
    assert(!reader.snapshot().valid && reader.snapshot().activeMask == 0);
    assert(reader.begin(10) && !reader.snapshot().valid);
    assert(bus.writes.size() == 3);
    assert(bus.registers[0x01] == (before[0x01] | 0xd8));
    assert(bus.registers[0x03] == (before[0x03] & ~0xd8));
    assert(bus.registers[0x0d] == (before[0x0d] | 0xd8));
    for (unsigned reg = 0; reg < before.size(); ++reg) {
        if (reg != 0x01 && reg != 0x03 && reg != 0x0d) assert(bus.registers[reg] == before[reg]);
        else assert((bus.registers[reg] & 0x27) == (before[reg] & 0x27));
    }
    reader.service(10);
    assert(reader.snapshot().valid && bus.gpioReads == 1);
}
static void simultaneousMapping() {
    Bus bus; WakeInputs reader(bus); assert(reader.begin(0));
    const unsigned pins[] = {7, 6, 4, 3};
    for (unsigned raw = 0; raw < 256; ++raw) {
        bus.registers[0x13] = static_cast<std::uint8_t>(raw);
        reader.service(raw * 20);
        unsigned expected = 0;
        for (unsigned i = 0; i < 4; ++i) if (raw & (1U << pins[i])) expected |= 1U << i;
        assert(reader.snapshot().valid && reader.snapshot().activeMask == expected);
        assert(bus.gpioReads == raw + 1);
    }
    assert(bus.writes.size() == 3); // polling never writes or scans another port
}
static void pollingAndClockWrap() {
    Bus bus; WakeInputs reader(bus); assert(reader.begin(0xfffffff0U));
    bus.registers[0x13] = 0xd8; reader.service(0xfffffff0U);
    bus.registers[0x13] = 0; reader.service(3);
    assert(reader.snapshot().activeMask == 15 && bus.gpioReads == 1);
    reader.service(4);
    assert(reader.snapshot().valid && reader.snapshot().activeMask == 0 && bus.gpioReads == 2);
}
static void readFailureAndRecovery() {
    Bus bus; WakeInputs reader(bus); assert(reader.begin(0));
    bus.registers[0x13] = 0xd8; reader.service(0);
    bus.failRead = static_cast<int>(bus.reads); reader.service(20);
    assert(!reader.snapshot().valid && reader.snapshot().activeMask == 0);
    reader.service(1019); assert(bus.begins == 1 && !reader.snapshot().valid);
    bus.registers[0x13] = 0x90; reader.service(1020);
    assert(bus.begins == 2 && reader.snapshot().valid && reader.snapshot().activeMask == 5);
}
static void initializationFailures() {
    for (int failed = 0; failed < 10; ++failed) {
        Bus bus; bus.failRead = failed; WakeInputs reader(bus);
        assert(!reader.begin(0) && !reader.snapshot().valid && reader.snapshot().activeMask == 0);
    }
    for (int failed = 0; failed < 3; ++failed) {
        Bus bus; bus.failWrite = failed; WakeInputs reader(bus);
        assert(!reader.begin(0) && !reader.snapshot().valid);
    }
    {
        Bus bus; bus.beginOk = false; WakeInputs reader(bus);
        assert(!reader.begin(0) && bus.reads == 0 && bus.writes.empty());
    }
    {
        Bus bus; bus.ignoreWrite = true; WakeInputs reader(bus);
        assert(!reader.begin(0) && bus.writes.size() == 1 && !reader.snapshot().valid);
    }
    for (unsigned variant = 0; variant < 3; ++variant) {
        Bus bus;
        if (variant == 0) bus.registers[0x05] = bus.registers[0x15] = 0x80;
        else if (variant == 1) bus.registers[0x0a] = 0x20;
        else bus.registers[0x0a] = bus.registers[0x0b] = 0x80;
        WakeInputs reader(bus);
        assert(!reader.begin(0) && bus.writes.empty());
    }
}
int main() {
    preservedConfiguration(); simultaneousMapping(); pollingAndClockWrap();
    readFailureAndRecovery(); initializationFailures();
    std::puts("Wake reader: mapping for all 256 GPIOB values, preservation, timing, failures and recovery passed");
}
