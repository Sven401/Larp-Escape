#include "MatrixInputs.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

using larp::matrix::MatrixInputs;
using larp::matrix::Snapshot;

// Register-level bus plus a passive switch network. Inputs behind a closed
// contact follow the one LOW row; floating rows can propagate a ghost contact.
class Bus final : public larp::wake::IRegisterBus {
  public:
    struct Operation { bool write; std::uint8_t address, reg, value; };
    std::array<std::array<std::uint8_t, 0x16>, 2> regs{};
    std::array<bool, 81> keys{};
    std::vector<Operation> operations;
    unsigned begins = 0, gpioReads = 0;
    int failAt = -1, ignoreWriteAt = -1, blockedAddress = -1;
    bool beginOk = true, propagateGhosts = true;
    std::uint8_t wakeRaw = 0xd8;

    Bus() {
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
        const bool rejected = static_cast<int>(operations.size()) == failAt || address == blockedAddress;
        operations.push_back({writing, address, reg, value});
        return rejected;
    }
    int activeRow() const {
        int selected = -1;
        for (unsigned row = 0; row < 9; ++row) {
            const unsigned device = row == 8 ? 0 : 1;
            const unsigned bit = row == 8 ? 0 : 7 - row;
            if (!(regs[device][1] & (1U << bit))) {
                assert(!(regs[device][0x15] & (1U << bit))); // no active HIGH row
                assert(selected == -1); // never two simultaneous output rows
                selected = static_cast<int>(row);
            }
        }
        return selected;
    }
    std::array<bool, 9> lowColumns() const {
        std::array<bool, 9> rows{}, columns{};
        const int selected = activeRow();
        if (selected < 0) return columns;
        rows[static_cast<unsigned>(selected)] = true;
        for (unsigned pass = 0; pass < (propagateGhosts ? 18U : 1U); ++pass) {
            for (unsigned row = 0; row < 9; ++row)
                for (unsigned column = 0; column < 9; ++column)
                    if (keys[row * 9 + column]) {
                        if (rows[row]) columns[column] = true;
                        if (propagateGhosts && columns[column]) rows[row] = true;
                    }
        }
        return columns;
    }
    bool read(std::uint8_t address, std::uint8_t reg, std::uint8_t &value) noexcept override {
        if (rejects(false, address, reg)) return false;
        if (reg == 0x12 || reg == 0x13) {
            ++gpioReads;
            assert(address == 0x20); // no B7 input read on the row expander
            const auto low = lowColumns();
            value = reg == 0x12 ? 0xff : static_cast<std::uint8_t>(0x27 | wakeRaw);
            if (reg == 0x12) {
                for (unsigned column = 1; column < 8; ++column)
                    if (low[column]) value &= static_cast<std::uint8_t>(~(1U << (column - 1)));
            } else {
                if (low[0]) value &= static_cast<std::uint8_t>(~0x02U);
                if (low[8]) value &= static_cast<std::uint8_t>(~0x20U);
            }
        } else value = regs[address - 0x20][reg];
        return true;
    }
    bool write(std::uint8_t address, std::uint8_t reg, std::uint8_t value) noexcept override {
        if (rejects(true, address, reg, value)) return false;
        if (static_cast<int>(operations.size() - 1) != ignoreWriteAt) regs[address - 0x20][reg] = value;
        activeRow();
        return true;
    }
};

static bool pressed(const Snapshot &sample, unsigned bit) {
    return (sample.bits[bit / 8] & (1U << (bit % 8))) != 0;
}
static void emptyInvalid(const Snapshot &sample) {
    assert(!sample.valid && !sample.ambiguous);
    for (auto byte : sample.bits) assert(byte == 0);
}
static void complete(MatrixInputs &reader, std::uint32_t &now) {
    // Exactly 37 service calls per scan at a millisecond cadence. No in-progress
    // partial row becomes a published snapshot.
    for (unsigned step = 0; step < 37; ++step) reader.service(now++);
    assert(reader.snapshot(now).valid);
}
static void preservedConfiguration() {
    Bus bus;
    const auto before = bus.regs;
    MatrixInputs reader(bus);
    emptyInvalid(reader.snapshot(0));
    assert(reader.begin(0));
    emptyInvalid(reader.snapshot(0));
    assert(bus.activeRow() == -1);
    for (unsigned device = 0; device < 2; ++device) {
        for (unsigned reg = 0; reg < 0x16; ++reg) {
            unsigned mask = 0;
            if (device == 0) {
                if (reg == 0 || reg == 2 || reg == 0x0c) mask = 0x7f;
                if (reg == 1 || reg == 3 || reg == 0x0d) mask = 0x23;
                if (reg == 0x15) mask = 1;
            } else if (reg == 1 || reg == 3 || reg == 0x0d || reg == 0x15) mask = 0xff;
            assert((bus.regs[device][reg] & ~mask) == (before[device][reg] & ~mask));
        }
    }
    assert((bus.regs[0][0x0d] & 0x23) == 0x22);
    assert(bus.regs[1][0x0d] == 0 && bus.regs[1][0x15] == 0);
    std::uint32_t now = 0;
    complete(reader, now);
    assert(bus.activeRow() == -1 && !reader.snapshot(now).ambiguous);
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
        assert((sample.bits[10] & 0xfe) == 0 && bus.activeRow() == -1);
    }
    Bus bus; bus.keys.fill(true);
    MatrixInputs reader(bus); assert(reader.begin(0));
    std::uint32_t now = 0; complete(reader, now);
    assert(reader.snapshot(now).ambiguous);
    for (unsigned bit = 0; bit < 81; ++bit) assert(pressed(reader.snapshot(now), bit));
    assert(reader.snapshot(now).bits[10] == 1);
}
static void settleAtomicPublicationAndWrap() {
    Bus bus; bus.keys[0] = true;
    MatrixInputs reader(bus); assert(reader.begin(0));
    reader.service(0); // validate/start
    reader.service(0); // select row
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
        for (bool ghosts : {false, true}) {
            Bus bus; bus.propagateGhosts = ghosts;
            for (auto bit : patterns[test]) bus.keys[bit] = true;
            MatrixInputs reader(bus); assert(reader.begin(0));
            std::uint32_t now = 0; complete(reader, now);
            assert(reader.snapshot(now).ambiguous == (test == 3));
            for (auto bit : patterns[test]) assert(pressed(reader.snapshot(now), bit));
            if (test == 3) assert(pressed(reader.snapshot(now), 10) == ghosts);
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
        assert(bus.activeRow() == -1); // single failure's cleanup succeeds
        bus.failAt = -1;
        // A retry is not a successful sample: all nine fresh rows are required.
        reader.service(1037);
        emptyInvalid(reader.snapshot(0));
        now = 1038;
        for (unsigned step = 0; step < 36; ++step) reader.service(now++);
        assert(reader.snapshot(now).valid && bus.activeRow() == -1);
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
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0; complete(reader, now);
        bus.regs[0][0x0c] = 0; // expander reset: pull-ups lost
        reader.service(now++);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeRow() == -1);
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        reader.service(0); reader.service(1); // row 1 is LOW
        assert(bus.activeRow() == 0);
        bus.blockedAddress = 0x21;
        reader.service(2); reader.service(3); reader.service(4); // release fails
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeRow() == 0); // physically cannot prove cleanup on a failed bus
        reader.service(1003);
        emptyInvalid(reader.snapshot(0));
        bus.blockedAddress = -1;
        reader.service(1004); // reinitialize; first release old row, then fresh scan
        assert(bus.activeRow() == -1);
        emptyInvalid(reader.snapshot(0));
        std::uint32_t now = 1005;
        for (unsigned step = 0; step < 36; ++step) reader.service(now++);
        assert(reader.snapshot(now).valid);
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        reader.service(0);
        bus.ignoreWriteAt = static_cast<int>(bus.operations.size()) + 1; // select write ignored
        reader.service(1);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeRow() == -1);
    }
}
static void scanDeadlineAndWakeCoexistence() {
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        std::uint32_t now = 0; complete(reader, now);
        reader.service(now++); reader.service(now++);
        assert(bus.activeRow() == 0);
        reader.service(now + 251);
        emptyInvalid(reader.snapshot(0));
        assert(bus.activeRow() == -1);
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
                assert((bus.regs[0][reg] & 0xd8) == (wakeConfiguration[reg] & 0xd8) ||
                       reg == 0 || reg == 2 || reg == 0x0c);
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
        emptyInvalid(reader.snapshot(251)); // also catches late final I2C completion
        bus.keys.fill(false); bus.keys[80] = true;
        reader.service(1000); // old valid sample was left in Phase::Start
        emptyInvalid(reader.snapshot(1000));
        for (unsigned tick = 1001; tick <= 1036; ++tick) reader.service(tick);
        assert(reader.snapshot(1036).valid && pressed(reader.snapshot(1036), 80));
    }
    {
        Bus bus; MatrixInputs reader(bus); assert(reader.begin(0));
        for (unsigned now = 0; now < 36; ++now) reader.service(now);
        bus.regs[0][0x0c] = 0; // reset between last read and publication
        reader.service(36);
        emptyInvalid(reader.snapshot(36));
        assert(bus.activeRow() == -1);
    }
}
int main() {
    preservedConfiguration(); everyContactAndFullCapacity();
    settleAtomicPublicationAndWrap(); rectanglesAreConservative();
    everyTransactionFailure(); missingDeviceBankResetAndCleanupFailure();
    scanDeadlineAndWakeCoexistence(); staleCompletedSampleAndMidScanReset();
    std::puts("Matrix: 81 contacts, low/high-Z scan, preservation, settle/atomicity, ghost rectangles, every transaction failure, recovery and Wake coexistence passed");
}
