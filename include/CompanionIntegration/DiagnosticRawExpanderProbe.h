#pragma once

#include <cstdint>

namespace larp {
namespace diagnostic {

// A register read is an electrical observation of an I2C transaction. It is
// never evidence that a named button is wired to the selected bit.
enum class RawRegisterStatus : std::uint8_t {
    Valid = 0,
    RegisterSelectFailed = 1,
    ShortRead = 2,
    ReadFailed = 3,
};

struct RawRegisterRead {
    RawRegisterRead() noexcept = default;
    RawRegisterRead(RawRegisterStatus observed, std::uint8_t bytes,
                    std::uint8_t code) noexcept
        : status(observed), value(bytes), busCode(code) {}
    RawRegisterStatus status = RawRegisterStatus::ReadFailed;
    std::uint8_t value = 0;
    std::uint8_t busCode = 0;
};

class IRawRegisterReader {
  public:
    virtual ~IRawRegisterReader() = default;
    virtual RawRegisterRead read(std::uint8_t address,
                                 std::uint8_t reg) noexcept = 0;
};

struct RawExpanderObservation {
    std::uint8_t address = 0, reg = 0, bit = 0;
    RawRegisterStatus status = RawRegisterStatus::ReadFailed;
    std::uint8_t rawByte = 0, busCode = 0;
    bool rawHigh = false;
    bool physicalChannelQualified = false;
    bool readable() const noexcept { return status == RawRegisterStatus::Valid; }
};

// The first exploratory source is address 0x20, register 0x13, bit 6. It is
// GPB6 only if the actual chip is an MCP23017 in BANK=0 mode. No pin mode or
// output latch is changed. A successful read does not qualify Wake 2.
class RawExpanderProbe {
  public:
    explicit RawExpanderProbe(IRawRegisterReader &reader) noexcept : reader_(reader) {}
    RawExpanderObservation sample() noexcept;

  private:
    IRawRegisterReader &reader_;
};

} // namespace diagnostic
} // namespace larp
