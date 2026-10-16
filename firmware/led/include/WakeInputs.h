#pragma once
#include <cstdint>

namespace larp {
namespace wake {
// Logical test labels; HIGH is reported, not a verified electrical activation.
// MCP numbering: Wake1=15(B7), Wake2=14(B6), Wake3=12(B4), Wake4=11(B3).
struct Snapshot {
    bool valid = false;
    std::uint8_t activeMask = 0;
};
class IRegisterBus {
  public:
    virtual ~IRegisterBus() = default;
    virtual bool begin() noexcept = 0;
    virtual bool read(std::uint8_t address, std::uint8_t reg,
                      std::uint8_t &value) noexcept = 0;
    virtual bool write(std::uint8_t address, std::uint8_t reg,
                       std::uint8_t value) noexcept = 0;
};
class WakeInputs {
  public:
    explicit WakeInputs(IRegisterBus &bus) : bus_(bus) {}
    bool begin(std::uint32_t now) noexcept;
    void service(std::uint32_t now) noexcept;
    Snapshot snapshot() const noexcept { return state_; }
    static constexpr std::uint8_t Address = 0x20;
    static constexpr std::uint8_t PinMask = 0xd8;
    static constexpr std::uint32_t PollMs = 20, RetryMs = 1000;
  private:
    IRegisterBus &bus_;
    Snapshot state_;
    bool initialized_ = false;
    std::uint32_t lastPoll_ = 0, lastInit_ = 0;
    bool configure() noexcept;
    bool update(std::uint8_t reg, std::uint8_t set, std::uint8_t clear) noexcept;
};
// One owner, hardware transport with bounded single-register transactions.
IRegisterBus &mcpWakeBus() noexcept;
} // namespace wake
} // namespace larp
