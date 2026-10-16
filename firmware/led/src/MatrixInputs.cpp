#include "MatrixInputs.h"

namespace larp {
namespace matrix {
namespace {
constexpr std::uint8_t First = 0x20, Second = 0x21;
constexpr std::uint8_t DirectionA = 0x00, DirectionB = 0x01;
constexpr std::uint8_t PolarityA = 0x02, PolarityB = 0x03;
constexpr std::uint8_t PullupA = 0x0c, PullupB = 0x0d;
constexpr std::uint8_t GpioB = 0x13, LatchA = 0x14, LatchB = 0x15;
constexpr std::uint8_t ColumnsA = 0x7f, ColumnsB = 0x22, RowNine = 0x01;
}

bool MatrixInputs::bankZero(std::uint8_t address) noexcept {
    // BANK=1 aliases IOCON at 0x05/0x15. An ambiguous layout is rejected;
    // never try to repair BANK by writing to a possibly different register.
    std::uint8_t first = 0, second = 0;
    if (!bus_.read(address, 0x05, first) || !bus_.read(address, 0x15, second) ||
        (first == second && (first & 0x80))) return false;
    return bus_.read(address, 0x0a, first) && bus_.read(address, 0x0b, second) &&
           first == second && !(first & 0x80);
}

bool MatrixInputs::update(std::uint8_t address, std::uint8_t reg,
                          std::uint8_t set, std::uint8_t clear) noexcept {
    std::uint8_t before = 0, after = 0;
    if (!bus_.read(address, reg, before)) return false;
    const auto wanted = static_cast<std::uint8_t>((before | set) & ~clear);
    if (wanted != before && !bus_.write(address, reg, wanted)) return false;
    return bus_.read(address, reg, after) && after == wanted;
}

bool MatrixInputs::matches(std::uint8_t address, std::uint8_t reg,
                           std::uint8_t mask, std::uint8_t wanted) noexcept {
    std::uint8_t value = 0;
    return bus_.read(address, reg, value) && (value & mask) == wanted;
}

bool MatrixInputs::releaseColumns() noexcept {
    // Unknown BANK means no writes. Try both ports even if one release fails;
    // a failed bus cannot prove that the selected output was actually released.
    if (!bankZero(First)) return false;
    const bool portA = update(First, DirectionA, ColumnsA, 0);
    const bool portB = update(First, DirectionB, ColumnsB, 0);
    return portA && portB;
}

bool MatrixInputs::configure() noexcept {
    if (!bankZero(First) || !bankZero(Second) || !releaseColumns()) return false;
    // Original COLMAP/ROWMAP direction: columns sink current, rows have pull-ups.
    // Historical diagnostic only: row 1 still reads 0x21 B7. This is NOT a
    // datasheet-compliant IO7 workaround; the test build explicitly opts in.
    // Preserve Wake B7/B6/B4/B3, unused 0x20 B2/A7 and all of 0x21 port A.
    // Release every matrix driver before preloading the column latches LOW.
    return update(First, DirectionB, RowNine, 0) &&
           update(Second, DirectionB, 0xff, 0) &&
           update(First, PullupA, 0, ColumnsA) &&
           update(First, PullupB, RowNine, ColumnsB) &&
           update(Second, PullupB, 0xff, 0) &&
           update(First, PolarityA, 0, ColumnsA) &&
           update(First, PolarityB, 0, ColumnsB | RowNine) &&
           update(Second, PolarityB, 0, 0xff) &&
           update(First, LatchA, 0, ColumnsA) &&
           update(First, LatchB, 0, ColumnsB);
}

bool MatrixInputs::configurationIntact() noexcept {
    // Check before and after a complete scan. A reset must not publish missing
    // pull-ups or incomplete configuration as valid contact observations.
    return bankZero(First) && bankZero(Second) &&
           matches(First, DirectionA, ColumnsA, ColumnsA) &&
           matches(First, DirectionB, ColumnsB | RowNine, ColumnsB | RowNine) &&
           matches(Second, DirectionB, 0xff, 0xff) &&
           matches(First, PullupA, ColumnsA, 0) &&
           matches(First, PullupB, ColumnsB | RowNine, RowNine) &&
           matches(Second, PullupB, 0xff, 0xff) &&
           matches(First, PolarityA, ColumnsA, 0) &&
           matches(First, PolarityB, ColumnsB | RowNine, 0) &&
           matches(Second, PolarityB, 0xff, 0) &&
           matches(First, LatchA, ColumnsA, 0) &&
           matches(First, LatchB, ColumnsB, 0);
}

bool MatrixInputs::selectColumn(bool selected) noexcept {
    // Original COLMAP: A=0x20 B1, B..H=0x20 A0..A6, I=0x20 B5.
    const auto reg = column_ == 0 || column_ == 8 ? DirectionB : DirectionA;
    const auto pin = static_cast<std::uint8_t>(
        column_ == 0 ? 0x02 : column_ == 8 ? 0x20 : 1U << (column_ - 1));
    // Unlike the original trailing HIGH, release to high-Z. This keeps the same
    // diode polarity without driving competing HIGH/LOW outputs through keys.
    return update(First, reg, selected ? 0 : pin, selected ? pin : 0);
}

bool MatrixInputs::readColumn() noexcept {
    std::uint8_t rowsOneToEight = 0, rowNine = 0;
    if (!bus_.read(Second, GpioB, rowsOneToEight) ||
        !bus_.read(First, GpioB, rowNine)) return false;
    for (unsigned row = 0; row < 9; ++row) {
        const bool high = row == 8 ? (rowNine & RowNine) != 0 :
                          (rowsOneToEight & (1U << (7 - row))) != 0;
        if (!high) {
            // Keep the existing wire order despite scanning by column.
            const unsigned bit = row * 9 + column_;
            pending_[bit / 8] |= static_cast<std::uint8_t>(1U << (bit % 8));
        }
    }
    return true;
}

bool MatrixInputs::ambiguous() const noexcept {
    // Three or four corners of any two-row/two-column rectangle are ambiguous
    // without verified isolation diodes. Keep all raw bits; do not invent which
    // contact was real. This flags possible ghosting, not all wiring faults.
    for (unsigned firstRow = 0; firstRow < 8; ++firstRow) {
        for (unsigned secondRow = firstRow + 1; secondRow < 9; ++secondRow) {
            unsigned counts[9] = {};
            for (unsigned column = 0; column < 9; ++column) {
                const unsigned first = firstRow * 9 + column;
                const unsigned second = secondRow * 9 + column;
                counts[column] = ((pending_[first / 8] >> (first % 8)) & 1U) +
                                 ((pending_[second / 8] >> (second % 8)) & 1U);
            }
            for (unsigned firstColumn = 0; firstColumn < 8; ++firstColumn)
                for (unsigned secondColumn = firstColumn + 1; secondColumn < 9; ++secondColumn)
                    if (counts[firstColumn] + counts[secondColumn] >= 3) return true;
        }
    }
    return false;
}

void MatrixInputs::fail(std::uint32_t now) noexcept {
    state_ = {};
    pending_.fill(0);
    initialized_ = false;
    phase_ = Phase::Start;
    lastInit_ = now;
    releaseColumns(); // Best effort only; never present a failed cleanup as valid.
}

bool MatrixInputs::begin(std::uint32_t now) noexcept {
    state_ = {};
    pending_.fill(0);
    initialized_ = false;
    phase_ = Phase::Start;
    lastInit_ = now;
    if (!bus_.begin()) return false;
    if (!configure()) {
        fail(now);
        return false;
    }
    initialized_ = true;
    return true;
}

void MatrixInputs::service(std::uint32_t now) noexcept {
    if (state_.valid && now - sampledAt_ > MaximumScanMs) state_ = {};
    if (!initialized_) {
        if (now - lastInit_ < RetryMs || !begin(now)) return;
    }
    if (phase_ != Phase::Start && now - roundStarted_ > MaximumScanMs) {
        fail(now);
        return;
    }
    switch (phase_) {
    case Phase::Start:
        if (!configurationIntact()) { fail(now); return; }
        pending_.fill(0);
        column_ = 0;
        roundStarted_ = now;
        phase_ = Phase::Select;
        return;
    case Phase::Select:
        if (!selectColumn(true)) { fail(now); return; }
        phase_ = Phase::SettleStart;
        return;
    case Phase::SettleStart:
        // Timestamp in a later invocation, after the completed I2C selection.
        // Thus even a slow selection transaction cannot consume settling time.
        selectedAt_ = now;
        phase_ = Phase::Read;
        return;
    case Phase::Read:
        if (now - selectedAt_ < SettleMs) return;
        if (!readColumn()) { fail(now); return; }
        phase_ = Phase::Release;
        return;
    case Phase::Release:
        if (!selectColumn(false)) { fail(now); return; }
        if (++column_ == 9) {
            if (!configurationIntact()) { fail(now); return; }
            state_.bits = pending_;
            state_.ambiguous = ambiguous();
            state_.valid = true;
            // Age from the beginning of the whole scan, not this last column.
            // snapshot(actualNow) also catches time spent in the final I2C call.
            sampledAt_ = roundStarted_;
            phase_ = Phase::Start;
        } else phase_ = Phase::Select;
        return;
    }
}
} // namespace matrix
} // namespace larp
