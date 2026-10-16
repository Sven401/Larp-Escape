#pragma once
#include "WakeInputs.h"
#include <array>
#include <cstdint>

// The explicit legacy test reads row 1 at 0x21 B7, as the original did.
// MCP23017 DS20001952D specifies IO7 as output-only. This opt-in acknowledges
// that unresolved hardware limitation; it does not establish chip suitability.
#if defined(LARP_MATRIX_TEST) && (!defined(LARP_MATRIX_LEGACY_IO7_INPUT) || LARP_MATRIX_LEGACY_IO7_INPUT != 1)
#error "Historical matrix test requires LARP_MATRIX_LEGACY_IO7_INPUT=1"
#endif

namespace larp {
namespace matrix {
// Row-major A1..I1, A2..I2, ... A9..I9; unused bits 81..87 stay zero.
// These are wiring coordinates, not a physical key identification.
// Some positions are deliberately unpopulated; an open bit cannot identify them.
// valid means a complete fresh register scan, not physical/chip qualification.
struct Snapshot {
    bool valid = false;
    bool ambiguous = false;
    std::array<std::uint8_t, 11> bits{};
};

class MatrixInputs {
  public:
    explicit MatrixInputs(wake::IRegisterBus &bus) : bus_(bus) {}
    bool begin(std::uint32_t now) noexcept;
    void service(std::uint32_t now) noexcept;
    Snapshot snapshot(std::uint32_t now) const noexcept {
        return state_.valid && now - sampledAt_ > MaximumScanMs ? Snapshot{} : state_;
    }
    static constexpr std::uint32_t SettleMs = 1, RetryMs = 1000, MaximumScanMs = 250;
  private:
    enum class Phase : std::uint8_t { Start, Select, SettleStart, Read, Release };
    wake::IRegisterBus &bus_;
    Snapshot state_;
    std::array<std::uint8_t, 11> pending_{};
    bool initialized_ = false;
    Phase phase_ = Phase::Start;
    std::uint8_t column_ = 0;
    std::uint32_t lastInit_ = 0, roundStarted_ = 0, selectedAt_ = 0, sampledAt_ = 0;
    bool bankZero(std::uint8_t address) noexcept;
    bool update(std::uint8_t address, std::uint8_t reg,
                std::uint8_t set, std::uint8_t clear) noexcept;
    bool matches(std::uint8_t address, std::uint8_t reg,
                 std::uint8_t mask, std::uint8_t wanted) noexcept;
    bool configure() noexcept;
    bool configurationIntact() noexcept;
    bool releaseColumns() noexcept;
    bool selectColumn(bool selected) noexcept;
    bool readColumn() noexcept;
    void fail(std::uint32_t now) noexcept;
    bool ambiguous() const noexcept;
};
} // namespace matrix
} // namespace larp
