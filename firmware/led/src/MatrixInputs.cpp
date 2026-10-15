#include "MatrixInputs.h"

namespace larp {
namespace matrix {
namespace {
constexpr std::uint8_t First = 0x20, Second = 0x21;
constexpr std::uint8_t DirectionA = 0x00, DirectionB = 0x01;
constexpr std::uint8_t PolarityA = 0x02, PolarityB = 0x03;
constexpr std::uint8_t PullupA = 0x0c, PullupB = 0x0d;
constexpr std::uint8_t GpioA = 0x12, GpioB = 0x13, LatchB = 0x15;
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

bool MatrixInputs::releaseRows() noexcept {
    // Attempt both expanders even if the first transaction fails. Unknown BANK
    // means no writes to that expander: a disconnected bus cannot prove release.
    const bool first = bankZero(First) && update(First, DirectionB, RowNine, 0);
    const bool second = bankZero(Second) && update(Second, DirectionB, 0xff, 0);
    return first && second;
}

bool MatrixInputs::configure() noexcept {
    if (!bankZero(First) || !bankZero(Second) || !releaseRows()) return false;
    // Only owned matrix pins are changed. In particular Wake's B7/B6/B4/B3
    // (0xd8), unused B2/A7 and every port-A pin on 0x21 are preserved.
    // Release directions first, then preload row latches LOW while high-Z.
    return update(First, DirectionA, ColumnsA, 0) &&
           update(First, DirectionB, ColumnsB | RowNine, 0) &&
           update(First, PullupA, ColumnsA, 0) &&
           update(First, PullupB, ColumnsB, RowNine) &&
           update(Second, PullupB, 0, 0xff) &&
           update(First, PolarityA, 0, ColumnsA) &&
           update(First, PolarityB, 0, ColumnsB | RowNine) &&
           update(Second, PolarityB, 0, 0xff) &&
           update(First, LatchB, 0, RowNine) &&
           update(Second, LatchB, 0, 0xff);
}

bool MatrixInputs::configurationIntact() noexcept {
    // Verify once per complete scan so an expander reset cannot silently turn
    // missing pull-ups into an apparently valid released/pressed matrix.
    return bankZero(First) && bankZero(Second) &&
           matches(First, DirectionA, ColumnsA, ColumnsA) &&
           matches(First, DirectionB, ColumnsB | RowNine, ColumnsB | RowNine) &&
           matches(Second, DirectionB, 0xff, 0xff) &&
           matches(First, PullupA, ColumnsA, ColumnsA) &&
           matches(First, PullupB, ColumnsB | RowNine, ColumnsB) &&
           matches(Second, PullupB, 0xff, 0) &&
           matches(First, PolarityA, ColumnsA, 0) &&
           matches(First, PolarityB, ColumnsB | RowNine, 0) &&
           matches(Second, PolarityB, 0xff, 0) &&
           matches(First, LatchB, RowNine, 0) &&
           matches(Second, LatchB, 0xff, 0);
}

bool MatrixInputs::selectRow(bool selected) noexcept {
    const auto address = row_ == 8 ? First : Second;
    const auto pin = static_cast<std::uint8_t>(row_ == 8 ? RowNine : 1U << (7 - row_));
    // Row 1 uses 0x21 B7 only as an output. No column is actively driven HIGH.
    return update(address, DirectionB, selected ? 0 : pin, selected ? pin : 0);
}

bool MatrixInputs::readRow() noexcept {
    std::uint8_t portA = 0, portB = 0;
    if (!bus_.read(First, GpioA, portA) || !bus_.read(First, GpioB, portB)) return false;
    for (unsigned column = 0; column < 9; ++column) {
        const bool high = column == 0 ? (portB & 0x02) != 0 :
                          column == 8 ? (portB & 0x20) != 0 :
                          (portA & (1U << (column - 1))) != 0;
        if (!high) {
            const unsigned bit = row_ * 9 + column;
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
    releaseRows(); // Best effort only; never present a failed cleanup as valid.
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
        row_ = 0;
        roundStarted_ = now;
        phase_ = Phase::Select;
        return;
    case Phase::Select:
        if (!selectRow(true)) { fail(now); return; }
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
        if (!readRow()) { fail(now); return; }
        phase_ = Phase::Release;
        return;
    case Phase::Release:
        if (!selectRow(false)) { fail(now); return; }
        if (++row_ == 9) {
            if (!configurationIntact()) { fail(now); return; }
            state_.bits = pending_;
            state_.ambiguous = ambiguous();
            state_.valid = true;
            // Age from the beginning of the whole scan, not this last row.
            // snapshot(actualNow) also catches time spent in the final I2C call.
            sampledAt_ = roundStarted_;
            phase_ = Phase::Start;
        } else phase_ = Phase::Select;
        return;
    }
}
} // namespace matrix
} // namespace larp
