#include "MatrixInputs.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

using larp::matrix::MatrixInputs;
using larp::matrix::Snapshot;

// Explicit physical wiring fixture, independent of the scanner implementation.
// Normal diodes have their cathode/ring on the lettered column. Conventional
// current flows row -> column, so LOW propagates column -> row only. This also
// models reversed/bypassed diodes and physically absent contacts.
class Bus final : public larp::wake::IRegisterBus {
  public:
    struct Operation { bool write; std::uint8_t address, reg, value; };
    struct Pin { unsigned device, port, bit; };
    enum class Diode { ColumnCathode, RowCathode, Bypassed };
    static constexpr std::array<Pin, 18> wiring{{
        {0, 1, 1}, {0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {0, 0, 3},
        {0, 0, 4}, {0, 0, 5}, {0, 0, 6}, {0, 1, 5}, // A..I
        {1, 1, 7}, {1, 1, 6}, {1, 1, 5}, {1, 1, 4}, {1, 1, 3},
        {1, 1, 2}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}  // 1..9
    }};
    std::array<std::array<std::uint8_t, 0x16>, 2> regs{};
    std::array<bool, 81> keys{}, present{};
    std::array<Diode, 81> diodes{};
    std::vector<Operation> operations;
    unsigned begins = 0, gpioReads = 0;
    int failAt = -1, ignoreWriteAt = -1, blockedAddress = -1;
    bool beginOk = true, enforceScannerSafety = true, blockDirectionA = false;
    std::uint8_t wakeRaw = 0xd8;

    Bus() {
        present.fill(true);
        diodes.fill(Diode::ColumnCathode);
        for (auto &device : regs) {
            device.fill(0xa4);
            device[0] = device[1] = 0xff;
            device[0x05] = 0;
            device[0x0a] = device[0x0b] = 0;
        }
    }
    bool begin() noexcept override { ++begins; return beginOk; }
    bool rejects(bool writing, std::uint8_t address, std::uint8_t reg,
                 std::uint8_t value = 0) {
        assert((address == 0x20 || address == 0x21) && reg < 0x16);
        const bool rejected = static_cast<int>(operations.size()) == failAt || address == blockedAddress ||
                              (blockDirectionA && address == 0x20 && reg == 0);
        operations.push_back({writing, address, reg, value});
        return rejected;
    }
    int activeColumn() const {
        int selected = -1;
        for (unsigned column = 0; column < 9; ++column) {
            const auto pin = wiring[column];
            if (!(regs[pin.device][pin.port] & (1U << pin.bit))) {
                assert(!(regs[pin.device][0x14 + pin.port] & (1U << pin.bit))); // no HIGH drive
                assert(selected == -1); // at most one LOW column, others high-Z
                selected = static_cast<int>(column);
            }
        }
        return selected;
    }
    void safeDirections() const {
        if (!enforceScannerSafety) return;
        activeColumn();
        for (unsigned row = 9; row < wiring.size(); ++row) {
            const auto pin = wiring[row];
            assert(regs[pin.device][pin.port] & (1U << pin.bit)); // rows always inputs
        }
    }
    std::array<bool, 18> lowNodes() const {
        std::array<bool, 18> low{};
        for (unsigned node = 0; node < wiring.size(); ++node) {
            const auto pin = wiring[node];
            low[node] = !(regs[pin.device][pin.port] & (1U << pin.bit)) &&
                        !(regs[pin.device][0x14 + pin.port] & (1U << pin.bit));
        }
        for (unsigned pass = 0; pass < wiring.size(); ++pass) {
            for (unsigned row = 0; row < 9; ++row) {
                for (unsigned column = 0; column < 9; ++column) {
                    const unsigned key = row * 9 + column;
                    if (!present[key] || !keys[key]) continue;
                    if (diodes[key] != Diode::RowCathode && low[column]) low[9 + row] = true;
                    if (diodes[key] != Diode::ColumnCathode && low[9 + row]) low[column] = true;
                }
            }
        }
        return low;
    }
    bool read(std::uint8_t address, std::uint8_t reg, std::uint8_t &value) noexcept override {
        if (rejects(false, address, reg)) return false;
        if (reg == 0x12 || reg == 0x13) {
            ++gpioReads;
            safeDirections();
            const auto low = lowNodes();
            const unsigned device = address - 0x20, port = reg - 0x12;
            value = 0xff; // nondriven nodes read HIGH in this deterministic fixture
            if (device == 0 && port == 1) value = static_cast<std::uint8_t>(0x27 | wakeRaw);
            for (unsigned node = 0; node < wiring.size(); ++node) {
                const auto pin = wiring[node];
                if (pin.device == device && pin.port == port && low[node])
                    value &= static_cast<std::uint8_t>(~(1U << pin.bit));
            }
            value ^= regs[device][2 + port];
        } else value = regs[address - 0x20][reg];
        return true;
    }
    bool write(std::uint8_t address, std::uint8_t reg, std::uint8_t value) noexcept override {
        if (rejects(true, address, reg, value)) return false;
        if (static_cast<int>(operations.size() - 1) != ignoreWriteAt) regs[address - 0x20][reg] = value;
        safeDirections();
        return true;
    }
};

constexpr std::array<Bus::Pin, 18> Bus::wiring;

static bool pressed(const Snapshot &sample, unsigned bit) {
    return (sample.bits[bit / 8] & (1U << (bit % 8))) != 0;
}
static void emptyInvalid(const Snapshot &sample) {
    assert(!sample.valid && !sample.ambiguous);
    for (auto byte : sample.bits) assert(byte == 0);
}
static void complete(MatrixInputs &reader, std::uint32_t &now) {
    // Start once, then Select/SettleStart/Read/Release for each of nine columns.
    for (unsigned step = 0; step < 37; ++step) reader.service(now++);
    assert(reader.snapshot(now).valid);
}
static void diodeFixtureRejectsOldScanDirection() {
    Bus bus;
    bus.enforceScannerSafety = false;
    for (auto &device : bus.regs) device[2] = device[3] = 0;
    bus.keys[0] = true; // A1: 0x20 B1 cathode, 0x21 B7 anode
    bus.regs[0][0x15] &= static_cast<std::uint8_t>(~0x02U);
    bus.regs[0][1] &= static_cast<std::uint8_t>(~0x02U); // column A LOW
    std::uint8_t value = 0;
    assert(bus.read(0x21, 0x13, value) && !(value & 0x80));
    bus.regs[0][1] |= 0x02;
    bus.regs[1][0x15] &= static_cast<std::uint8_t>(~0x80U);
    bus.regs[1][1] &= static_cast<std::uint8_t>(~0x80U); // old incorrect row drive
    assert(bus.read(0x20, 0x13, value) && (value & 0x02)); // diode blocks it
    bus.diodes[0] = Bus::Diode::RowCathode;
    assert(bus.read(0x20, 0x13, value) && !(value & 0x02));
    bus.keys[0] = false;
    assert(bus.read(0x20, 0x13, value) && (value & 0x02));
}
static void preservedConfiguration() {
    Bus bus;
    const auto before = bus.regs;
    MatrixInputs reader(bus);
    emptyInvalid(reader.snapshot(0));
    assert(reader.begin(0));
    emptyInvalid(reader.snapshot(0));
    assert(bus.activeColumn() == -1);
    for (unsigned device = 0; device < 2; ++device) {
        for (unsigned reg = 0; reg < 0x16; ++reg) {
            unsigned mask = 0;
            if (device == 0) {
                if (reg == 0 || reg == 2 || reg == 0x0c || reg == 0x14) mask = 0x7f;
                if (reg == 1 || reg == 3 || reg == 0x0d) mask = 0x23;
                if (reg == 0x15) mask = 0x22;
            } else if (reg == 1 || reg == 3 || reg == 0x0d) mask = 0xff;
            assert((bus.regs[device][reg] & ~mask) == (before[device][reg] & ~mask));
        }
    }
    assert((bus.regs[0][0x0c] & 0x7f) == 0);
    assert((bus.regs[0][0x0d] & 0x23) == 0x01);
    assert(bus.regs[1][0x0d] == 0xff && bus.regs[1][1] == 0xff);
    assert((bus.regs[0][0x14] & 0x7f) == 0 && (bus.regs[0][0x15] & 0x22) == 0);
    std::uint32_t now = 0;
    const auto scanStart = bus.operations.size();
    complete(reader, now);
    assert(bus.activeColumn() == -1 && !reader.snapshot(now).ambiguous);
    unsigned reads = 0;
    for (std::size_t index = scanStart; index < bus.operations.size(); ++index) {
        const auto op = bus.operations[index];
        if (!op.write && (op.reg == 0x12 || op.reg == 0x13)) {
            assert(op.reg == 0x13 && op.address == (reads % 2 == 0 ? 0x21 : 0x20));
            ++reads;
        }
    }
    assert(reads == 18);
}
static void everyContactAndFullCapacity() {
    for (unsigned selected = 0; selected < 81; ++selected) {
        Bus bus; bus.keys[selected] = true;
        MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0;
        complete(reader, now);
        const auto sample = reader.snapshot(now);
        assert(!sample.ambiguous);
        for (unsigned bit = 0; bit < 81; ++bit) assert(pressed(sample, bit) == (selected == bit));
        assert((sample.bits[10] & 0xfe) == 0 && bus.activeColumn() == -1);
    }
    Bus bus; bus.keys.fill(true);
    MatrixInputs reader(bus); assert(reader.begin(0));
    std::uint32_t now = 0; complete(reader, now);
    assert(reader.snapshot(now).ambiguous);
    for (unsigned bit = 0; bit < 81; ++bit) assert(pressed(reader.snapshot(now), bit));
    assert(reader.snapshot(now).bits[10] == 1);
}
static void sparsePhysicalFixtureAndReversedDiode() {
    Bus bus;
    // Illustrative holes, not claims about this box's still-unmapped positions.
    for (auto bit : {10U, 24U, 50U, 80U}) bus.present[bit] = false;
    bus.keys.fill(true);
    bus.keys[30] = false;
    MatrixInputs reader(bus); assert(reader.begin(0));
    std::uint32_t now = 0; complete(reader, now);
    for (unsigned bit = 0; bit < 81; ++bit)
        assert(pressed(reader.snapshot(now), bit) == (bus.present[bit] && bus.keys[bit]));
    // Isolate reversed polarity: mixed reversed diodes and many closed switches
    // can create additional real current paths, so they are a different test.
    bus.keys.fill(false);
    bus.keys[7] = true;
    bus.diodes[7] = Bus::Diode::RowCathode;
    complete(reader, now);
    for (unsigned bit = 0; bit < 81; ++bit) assert(!pressed(reader.snapshot(now), bit));
    bus.diodes[7] = Bus::Diode::ColumnCathode;
    complete(reader, now);
    assert(pressed(reader.snapshot(now), 7));
    bus.keys[7] = false;
    complete(reader, now);
    for (unsigned bit = 0; bit < 81; ++bit) assert(!pressed(reader.snapshot(now), bit));
}
static void settleAtomicPublicationAndWrap() {
    Bus bus; bus.keys[0] = true;
    MatrixInputs reader(bus); assert(reader.begin(0));
    reader.service(0); // validate/start
    reader.service(0); // select column
    reader.service(100); // settling starts after selection completed
    reader.service(100);
    assert(bus.gpioReads == 0);
    reader.service(101);
    assert(bus.gpioReads == 2);
    emptyInvalid(reader.snapshot(0));
    reader.service(102);
    std::uint32_t now = 103;
    for (unsigned step = 0; step < 32; ++step) reader.service(now++);
    assert(reader.snapshot(now).valid && pressed(reader.snapshot(now), 0));
    bus.keys.fill(false); bus.keys[80] = true;
    for (unsigned step = 0; step < 36; ++step) {
        reader.service(now++);
        assert(pressed(reader.snapshot(now), 0) && !pressed(reader.snapshot(now), 80));
    }
    reader.service(now++);
    assert(!pressed(reader.snapshot(now), 0) && pressed(reader.snapshot(now), 80));

    Bus wrapBus; MatrixInputs wrap(wrapBus); now = 0xfffffff0U;
    assert(wrap.begin(now)); complete(wrap, now);
    assert(wrap.snapshot(now).valid && now < 100);
}
static void rectanglesAreConservative() {
    const std::array<std::array<unsigned, 3>, 4> patterns{{
        {{0, 1, 2}}, {{0, 9, 18}}, {{0, 10, 20}}, {{0, 1, 9}}
    }};
    for (unsigned test = 0; test < patterns.size(); ++test) {
        for (bool bypass : {false, true}) {
            Bus bus;
            if (bypass) bus.diodes.fill(Bus::Diode::Bypassed);
            for (auto bit : patterns[test]) bus.keys[bit] = true;
            MatrixInputs reader(bus); assert(reader.begin(0));
            std::uint32_t now = 0; complete(reader, now);
            assert(reader.snapshot(now).ambiguous == (test == 3));
            for (unsigned bit = 0; bit < 81; ++bit) {
                bool expected = test == 3 && bypass && bit == 10;
                for (auto selected : patterns[test]) expected |= bit == selected;
                assert(pressed(reader.snapshot(now), bit) == expected);
            }
        }
    }
}
static void everyTransactionFailure() {
    Bus reference; MatrixInputs good(reference); assert(good.begin(0));
    const auto initOperations = reference.operations.size();
    std::uint32_t now = 0; complete(good, now);
    const auto allOperations = reference.operations.size();
    for (std::size_t failed = 0; failed < allOperations; ++failed) {
        Bus bus; bus.failAt = static_cast<int>(failed);
        MatrixInputs reader(bus);
        const bool began = reader.begin(0);
        if (failed < initOperations) assert(!began);
        else {
            assert(began);
            for (unsigned step = 0; step < 37; ++step) reader.service(step);
        }
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == -1); // single failure's cleanup succeeds
        bus.failAt = -1;
        reader.service(1037); // retry alone is not a fresh nine-column sample
        emptyInvalid(reader.snapshot(0));
        now = 1038;
        for (unsigned step = 0; step < 36; ++step) reader.service(now++);
        assert(reader.snapshot(now).valid && bus.activeColumn() == -1);
    }
}
static void missingDeviceBankResetAndCleanupFailure() {
    {
        Bus bus; bus.beginOk = false; MatrixInputs reader(bus);
        assert(!reader.begin(0) && bus.operations.empty());
    }
    for (unsigned device = 0; device < 2; ++device) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            Bus bus;
            if (variant == 0) bus.regs[device][0x05] = bus.regs[device][0x15] = 0x80;
            else if (variant == 1) bus.regs[device][0x0a] = 0x20;
            else bus.regs[device][0x0a] = bus.regs[device][0x0b] = 0x80;
            MatrixInputs reader(bus); assert(!reader.begin(0));
            emptyInvalid(reader.snapshot(0));
            for (const auto &op : bus.operations) assert(!op.write || op.address != 0x20 + device);
        }
        Bus missing; missing.blockedAddress = 0x20 + device;
        MatrixInputs reader(missing); assert(!reader.begin(0));
        emptyInvalid(reader.snapshot(0));
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0; complete(reader, now);
        bus.regs[1][0x0d] = 0; // expander reset: row pull-ups lost
        reader.service(now++);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == -1);
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        reader.service(0); reader.service(1); // column A is LOW
        assert(bus.activeColumn() == 0);
        bus.blockedAddress = 0x20;
        reader.service(2); reader.service(3); // read then cleanup fail
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == 0); // failed bus cannot prove physical release
        reader.service(1002);
        emptyInvalid(reader.snapshot(0));
        bus.blockedAddress = -1;
        reader.service(1003); // reinitialize/release, then start fresh scan
        assert(bus.activeColumn() == -1);
        emptyInvalid(reader.snapshot(0));
        std::uint32_t now = 1004;
        for (unsigned step = 0; step < 36; ++step) reader.service(now++);
        assert(reader.snapshot(now).valid);
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        reader.service(0); reader.service(1);
        assert(bus.activeColumn() == 0); // B1 is active; A direction reads fail
        bus.blockDirectionA = true;
        bus.failAt = static_cast<int>(bus.operations.size());
        reader.service(2); reader.service(3);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == -1); // B cleanup must run despite A failure
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        reader.service(0);
        bus.ignoreWriteAt = static_cast<int>(bus.operations.size()) + 1;
        reader.service(1); // ignored select write must be caught by readback
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == -1);
    }
}
static void scanDeadlineAndWakeCoexistence() {
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0; complete(reader, now);
        reader.service(now++); reader.service(now++);
        assert(bus.activeColumn() == 0);
        reader.service(now + 251);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeColumn() == -1);
    }
    {
        Bus bus;
        larp::wake::WakeInputs wake(bus); assert(wake.begin(0));
        const auto wakeConfiguration = bus.regs[0];
        MatrixInputs reader(bus); assert(reader.begin(0));
        bus.keys[0] = bus.keys[80] = true;
        for (unsigned now = 0; now < 100; ++now) {
            reader.service(now);
            wake.service(now);
            assert(wake.snapshot().valid && wake.snapshot().activeMask == 15);
            for (unsigned reg = 0; reg < 0x16; ++reg)
                if (reg != 0 && reg != 2 && reg != 0x0c && reg != 0x14)
                    assert((bus.regs[0][reg] & 0xd8) == (wakeConfiguration[reg] & 0xd8));
        }
        assert(reader.snapshot(100).valid && pressed(reader.snapshot(100), 0) && pressed(reader.snapshot(100), 80));
    }
}
static void staleCompletedSampleAndMidScanReset() {
    {
        Bus bus; bus.keys[0] = true;
        MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0; complete(reader, now);
        assert(reader.snapshot(250).valid);
        emptyInvalid(reader.snapshot(251));
        bus.keys.fill(false); bus.keys[80] = true;
        reader.service(1000);
        emptyInvalid(reader.snapshot(1000));
        for (unsigned tick = 1001; tick <= 1036; ++tick) reader.service(tick);
        assert(reader.snapshot(1036).valid && pressed(reader.snapshot(1036), 80));
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        for (unsigned now = 0; now < 36; ++now) reader.service(now);
        bus.regs[0][0x0d] &= static_cast<std::uint8_t>(~0x01U); // row9 pullup reset before publication
        reader.service(36);
        emptyInvalid(reader.snapshot(36));
        assert(bus.activeColumn() == -1);
    }
}
int main() {
    diodeFixtureRejectsOldScanDirection(); preservedConfiguration();
    everyContactAndFullCapacity(); sparsePhysicalFixtureAndReversedDiode();
    settleAtomicPublicationAndWrap(); rectanglesAreConservative();
    everyTransactionFailure(); missingDeviceBankResetAndCleanupFailure();
    scanDeadlineAndWakeCoexistence(); staleCompletedSampleAndMidScanReset();
    std::puts("Matrix: directed diodes, historical pins, sparse fixture, 81 coordinates, high-Z scan, preservation, settle/atomicity, rectangle warnings, transaction failures, recovery and Wake coexistence passed");
}
