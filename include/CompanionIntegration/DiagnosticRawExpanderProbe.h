#pragma once

#include <cstdint>

namespace larp {
namespace diagnostic {

// A register read is an electrical observation. The qualification flag is
// supplied by the test image only after its Wake-2 input mode was read back;
// a single level still cannot prove a press/release action.
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
    RawExpanderObservation() noexcept = default;
    RawExpanderObservation(std::uint8_t observedAddress, std::uint8_t observedRegister,
                          std::uint8_t observedBit, RawRegisterStatus observedStatus,
                          std::uint8_t bytes, std::uint8_t code, bool high,
                          bool qualified) noexcept
        : address(observedAddress), reg(observedRegister), bitIndex(observedBit),
          status(observedStatus), rawByte(bytes), busCode(code), rawHigh(high),
          physicalChannelQualified(qualified) {}
    std::uint8_t address = 0, reg = 0, bitIndex = 0;
    RawRegisterStatus status = RawRegisterStatus::ReadFailed;
    std::uint8_t rawByte = 0, busCode = 0;
    bool rawHigh = false;
    bool physicalChannelQualified = false;
    bool readable() const noexcept { return status == RawRegisterStatus::Valid; }
};

// Fixed Wake-2 source from the historically tested game image: MCP1 at 0x20,
// GPB6 in BANK=0 (GPIOB register 0x13). This reader never changes pin modes;
// the test-image entry point configures and verifies INPUT_PULLUP separately.
class RawExpanderProbe {
  public:
    explicit RawExpanderProbe(IRawRegisterReader &reader) noexcept : reader_(reader) {}
    RawExpanderObservation sample() noexcept;

  private:
    IRawRegisterReader &reader_;
};

} // namespace diagnostic
} // namespace larp
