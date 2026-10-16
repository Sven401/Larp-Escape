#include "WakeInputs.h"
#include <Wire.h>

namespace larp {
namespace wake {
namespace {
class McpWakeBus final : public IRegisterBus {
  public:
    bool begin() noexcept override {
        // Wake and the optional matrix test share this one owner. Retrying an
        // input must not reinitialize a bus currently used by the other reader.
        if (started_) return true;
        Wire.setTimeOut(5);
        started_ = Wire.begin(21, 22, 100000);
        return started_;
    }
    bool read(std::uint8_t address, std::uint8_t reg,
              std::uint8_t &value) noexcept override {
        Wire.beginTransmission(address);
        if (Wire.write(reg) != 1) {
            Wire.endTransmission(true);
            return false;
        }
        if (Wire.endTransmission(false) != 0) return false;
        const auto count = Wire.requestFrom(address, static_cast<std::size_t>(1), true);
        if (count != 1 || Wire.available() != 1) return false;
        const int byte = Wire.read();
        if (byte < 0) return false;
        value = static_cast<std::uint8_t>(byte);
        return true;
    }
    bool write(std::uint8_t address, std::uint8_t reg,
               std::uint8_t value) noexcept override {
        Wire.beginTransmission(address);
        const bool queued = Wire.write(reg) == 1 && Wire.write(value) == 1;
        return Wire.endTransmission(true) == 0 && queued;
    }
  private:
    bool started_ = false;
} bus;
} // namespace
IRegisterBus &mcpWakeBus() noexcept { return bus; }
} // namespace wake
} // namespace larp
