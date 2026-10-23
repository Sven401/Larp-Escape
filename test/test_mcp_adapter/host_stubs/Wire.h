#pragma once
#include "Arduino.h"
#include <algorithm>
#include <array>
#include <vector>

// Register-level MCP23017/BANK=0 transport model, not a fake digitalRead.
// Faults are injected at Wire ACK/byte-count boundaries. It models exclusive
// synchronous RX consumption, not concurrent read() theft or an electrical bus.
class TwoWire {
  public:
    struct Device {
        bool present = false, nack = false;
        std::array<uint8_t, 0x16> registers = {};
        uint16_t inputLevels = 0xffff;
        uint8_t maxRead = 0xff;
        int failReadRegister = -1, goodReadsBeforeFailure = 0;
        Device() {
            registers[0] = registers[1] = 0xff;
        }
    };
    struct WriteOperation {
        uint8_t address, reg, value;
        bool committed, acknowledged;
    };
    struct Contact {
        uint8_t rowAddress, rowPin, columnAddress, columnPin;
    };
    struct WriteFault {
        int address = -1, reg = -1, remaining = 0;
        uint8_t mask = 0xff, value = 0;
        bool applyBeforeNack = false;
    };
    std::array<Device, 128> devices;
    std::vector<WriteOperation> writes;
    std::vector<Contact> contacts;
    WriteFault writeFault;
    unsigned beginCalls = 0;

    void begin() {
        ++beginCalls;
    }
    void end() {}
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t address) {
        address_ = address;
        tx_.clear();
    }
    std::size_t write(uint8_t value) {
        tx_.push_back(value);
        return 1;
    }
    std::size_t write(const uint8_t *data, std::size_t count) {
        tx_.insert(tx_.end(), data, data + count);
        return count;
    }
    uint8_t endTransmission(bool = true) {
        Device &device = devices[address_];
        if (!device.present || device.nack)
            return 2;
        if (tx_.empty())
            return 0;
        pointer_ = tx_[0];
        bool reject = false;
        if (tx_.size() > 1 && writeFault.remaining != 0 && writeFault.address == address_ &&
            writeFault.reg == pointer_ && (tx_[1] & writeFault.mask) == writeFault.value) {
            reject = true;
            if (writeFault.remaining > 0)
                --writeFault.remaining;
        }
        const bool commit = !reject || writeFault.applyBeforeNack;
        for (std::size_t i = 1; i < tx_.size(); ++i) {
            const uint8_t reg = static_cast<uint8_t>(pointer_ + i - 1);
            writes.push_back({address_, reg, tx_[i], commit, !reject});
            if (commit && reg < device.registers.size()) {
                if (reg == 0x12 || reg == 0x13)
                    device.registers[reg + 2] = tx_[i];
                else
                    device.registers[reg] = tx_[i];
            }
        }
        return reject ? 3 : 0;
    }
    uint8_t requestFrom(uint8_t address, uint8_t count, uint8_t = 1) {
        rx_.clear();
        cursor_ = 0;
        Device &device = devices[address];
        if (!device.present || device.nack)
            return 0;
        if (device.failReadRegister == pointer_) {
            if (device.goodReadsBeforeFailure == 0)
                return 0;
            --device.goodReadsBeforeFailure;
        }
        const uint8_t available = std::min(count, device.maxRead);
        for (uint8_t i = 0; i < available; ++i) {
            const uint8_t reg = static_cast<uint8_t>(pointer_ + i);
            if (reg == 0x12 || reg == 0x13)
                rx_.push_back(static_cast<uint8_t>(gpio(address) >> (8 * (reg - 0x12))));
            else
                rx_.push_back(device.registers.at(reg));
        }
        return available;
    }
    int read() {
        return cursor_ < rx_.size() ? rx_[cursor_++] : -1;
    }
    int available() const {
        return static_cast<int>(rx_.size() - cursor_);
    }
    uint16_t gpio(uint8_t address) const {
        const Device &device = devices[address];
        const uint16_t direction = device.registers[0] | (device.registers[1] << 8);
        const uint16_t latch = device.registers[0x14] | (device.registers[0x15] << 8);
        uint16_t input = device.inputLevels;
        for (const auto &contact : contacts) {
            if (contact.rowAddress != address)
                continue;
            const Device &column = devices[contact.columnAddress];
            const uint8_t port = contact.columnPin / 8;
            const uint8_t mask = static_cast<uint8_t>(1U << (contact.columnPin % 8));
            if (!(column.registers[port] & mask) && !(column.registers[0x14 + port] & mask))
                input &= static_cast<uint16_t>(~(1U << contact.rowPin));
        }
        return static_cast<uint16_t>((input & direction) | (latch & ~direction));
    }

  private:
    uint8_t address_ = 0, pointer_ = 0;
    std::vector<uint8_t> tx_, rx_;
    std::size_t cursor_ = 0;
};
extern TwoWire Wire;
