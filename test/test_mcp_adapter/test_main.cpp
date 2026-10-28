#ifndef MCP_LEGACY_PROBE
#include "Adafruit_BusIO_Register.h"
#include "Fixture.h"
#include "Gamestate.h"
#include <unity.h>

#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/KeystoneTransition.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"

using larp::domain::GridColumn;

void test_real_adafruit_scalar_api_reproduces_failure_high_while_bool_api_rejects() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x20].inputLevels = 0;
    TEST_ASSERT_EQUAL_UINT8(LOW, rig.wakeChip.digitalRead(MCP_B7));
    rig.bus.devices[0x20].nack = true;
    TEST_ASSERT_EQUAL_UINT8(HIGH, rig.wakeChip.digitalRead(MCP_B7));
    Adafruit_I2CDevice device(0x20, &rig.bus);
    Adafruit_BusIO_Register reg(&device, 0x13);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffU, reg.read());
    uint8_t value = 0;
    TEST_ASSERT_FALSE(reg.read(&value));
}

void test_healthy_group_preserves_high_low_and_real_standby_transition() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x20].inputLevels = (1U << MCP_B7) | (1U << MCP_B4);
    const auto sample = rig.matrix.readWakeSignals();
    TEST_ASSERT_TRUE(sample.valid);
    TEST_ASSERT_TRUE(sample.signals[0]);
    TEST_ASSERT_FALSE(sample.signals[1]);
    TEST_ASSERT_TRUE(sample.signals[2]);
    TEST_ASSERT_FALSE(sample.signals[3]);
    rig.tick();
    TEST_ASSERT_TRUE(rig.standby());
    TEST_ASSERT_EQUAL_UINT(2, rig.lights.ripples.size());
    rig.bus.devices[0x20].inputLevels = 0xffff;
    rig.tick();
    TEST_ASSERT_FALSE(rig.standby());
}

void test_nack_zero_and_partial_reads_keep_whole_wake_group_invalid_and_game_standby() {
    for (int fault = 0; fault < 3; ++fault) {
        McpRig rig;
        TEST_ASSERT_TRUE(rig.ready);
        TEST_ASSERT_TRUE(rig.matrix.readWakeSignals().valid);
        rig.bus.devices[0x20].nack = fault == 0;
        rig.bus.devices[0x20].maxRead = fault == 1 ? 0 : fault == 2 ? 1 : 0xff;
        const auto sample = rig.matrix.readWakeSignals();
        TEST_ASSERT_FALSE(sample.valid);
        for (const auto active : sample.signals)
            TEST_ASSERT_FALSE(active);
        rig.tick();
        TEST_ASSERT_TRUE(rig.standby());
        TEST_ASSERT_TRUE(rig.lights.ripples.empty());
        const unsigned begins = rig.bus.beginCalls;
        rig.bus.devices[0x20].nack = false;
        rig.bus.devices[0x20].maxRead = 0xff;
        rig.tick();
        TEST_ASSERT_FALSE(rig.standby());
        TEST_ASSERT_EQUAL_UINT(begins, rig.bus.beginCalls);
    }
}

void test_one_failed_expander_does_not_corrupt_other_device_wake_sample() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x21].nack = true;
    auto wake = rig.matrix.readWakeSignals();
    TEST_ASSERT_TRUE(wake.valid);
    for (const auto active : wake.signals)
        TEST_ASSERT_TRUE(active);
    TEST_ASSERT_FALSE(rig.matrix.readPressedPositions().valid);
    rig.bus.devices[0x21].nack = false;
    rig.bus.devices[0x20].nack = true;
    TEST_ASSERT_FALSE(rig.matrix.readWakeSignals().valid);
    TEST_ASSERT_TRUE(rig.columnHandler.writeGPIO(MCP_A0, HIGH));
}

void test_checked_adapter_reports_failed_mode_read_and_write_without_latch_corruption() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x21].maxRead = 0;
    TEST_ASSERT_FALSE(rig.columnHandler.writeGPIO(MCP_A0, LOW));
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
    rig.bus.devices[0x21].maxRead = 0xff;
    rig.failOutput(1, false);
    TEST_ASSERT_FALSE(rig.columnHandler.writeGPIO(MCP_A0, LOW));
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
    TEST_ASSERT_TRUE(rig.columnHandler.writeGPIO(MCP_A0, LOW));
    TEST_ASSERT_EQUAL_HEX8(0xfe, rig.bus.devices[0x21].registers[0x14]);
    TEST_ASSERT_TRUE(rig.columnHandler.writeGPIO(MCP_A0, HIGH));
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
}

void test_healthy_matrix_scans_real_contacts_and_preserves_other_latch_bits() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.contacts.push_back({0x20, MCP_B0, 0x21, MCP_A0});
    rig.bus.contacts.push_back({0x20, MCP_B1, 0x21, MCP_A1});
    const auto sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_TRUE(sample.valid);
    TEST_ASSERT_EQUAL_UINT(2, sample.positions.size());
    TEST_ASSERT_TRUE((sample.positions[0] == larp::domain::GridPosition{1, GridColumn::A}));
    TEST_ASSERT_TRUE((sample.positions[1] == larp::domain::GridPosition{2, GridColumn::B}));
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
}

void test_failed_later_row_read_discards_an_already_collected_press() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.contacts.push_back({0x20, MCP_B0, 0x21, MCP_A0});
    rig.bus.devices[0x20].failReadRegister = 0x13;
    rig.bus.devices[0x20].goodReadsBeforeFailure = 1;
    const auto sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_FALSE(sample.valid);
    TEST_ASSERT_TRUE(sample.positions.empty());
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
    rig.bus.devices[0x20].failReadRegister = -1;
    const auto recovered = rig.matrix.readPressedPositions();
    TEST_ASSERT_TRUE(recovered.valid);
    TEST_ASSERT_EQUAL_UINT(1, recovered.positions.size());
}

void test_failed_low_with_physical_effect_invalidates_scan_and_requires_recovery() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.contacts.push_back({0x20, MCP_B0, 0x21, MCP_A0});
    rig.failOutput(1, false, false, true);
    const auto sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_FALSE(sample.valid);
    TEST_ASSERT_TRUE(sample.positions.empty());
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
    rig.bus.writes.clear();
    TEST_ASSERT_TRUE(rig.matrix.readPressedPositions().valid);
    unsigned successfulHighsBeforeLow = 0;
    for (const auto &op : rig.bus.writes) {
        if (op.address != 0x21 || op.reg != 0x14)
            continue;
        if ((op.value & 3) != 3)
            break;
        if (op.acknowledged)
            ++successfulHighsBeforeLow;
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(2, successfulHighsBeforeLow);
}

void test_failed_release_blocks_new_selection_until_all_columns_are_released() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.contacts.push_back({0x20, MCP_B0, 0x21, MCP_A0});
    rig.failOutput(1, true, true);
    auto sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_FALSE(sample.valid);
    TEST_ASSERT_TRUE(sample.positions.empty());
    TEST_ASSERT_EQUAL_HEX8(0xfe, rig.bus.devices[0x21].registers[0x14]);
    rig.bus.writes.clear();
    sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_FALSE(sample.valid);
    TEST_ASSERT_TRUE(sample.positions.empty());
    // A1 was never selected LOW while A0 release remained unconfirmed.
    for (const auto &op : rig.bus.writes)
        if (op.reg == 0x14)
            TEST_ASSERT_NOT_EQUAL(0, op.value & 2);
    rig.bus.writeFault.remaining = 0;
    sample = rig.matrix.readPressedPositions();
    TEST_ASSERT_TRUE(sample.valid);
    TEST_ASSERT_EQUAL_UINT(1, sample.positions.size());
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
}

void test_failed_startup_configuration_never_claims_ready_or_reallocates_retry() {
    TwoWire bus;
    bus.devices[0x20].present = true;
    bus.devices[0x20].maxRead = 0;
    Adafruit_MCP23X17 chip;
    MCPConfig config = wakeConfig();
    MCPHandler handler(chip, config, "startup", &bus);
    TEST_ASSERT_FALSE(handler.begin());
    TEST_ASSERT_FALSE(handler.isReady());
    bus.devices[0x20].maxRead = 0xff;
    const auto begins = bus.beginCalls;
    TEST_ASSERT_FALSE(handler.begin());
    TEST_ASSERT_EQUAL_UINT(begins, bus.beginCalls);
}

void test_checked_begin_preserves_existing_output_latch_and_invalid_reads_clear_output() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    TEST_ASSERT_EQUAL_HEX8(0xff, rig.bus.devices[0x21].registers[0x14]);
    for (const auto &op : rig.startupWrites) {
        TEST_ASSERT_TRUE_MESSAGE(op.reg < 0x12 || op.reg > 0x15,
                                 "begin must not write GPIO/OLAT or choose new output levels");
    }
    uint8_t level = HIGH;
    rig.bus.devices[0x20].nack = true;
    TEST_ASSERT_FALSE(rig.wakeHandler.readGPIO(MCP_B7, level));
    TEST_ASSERT_EQUAL_UINT8(LOW, level);
    uint16_t all = 0xffff;
    TEST_ASSERT_FALSE(rig.wakeHandler.readGPIOAB(all));
    TEST_ASSERT_EQUAL_UINT16(0, all);
}

void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_real_adafruit_scalar_api_reproduces_failure_high_while_bool_api_rejects);
    RUN_TEST(test_healthy_group_preserves_high_low_and_real_standby_transition);
    RUN_TEST(test_nack_zero_and_partial_reads_keep_whole_wake_group_invalid_and_game_standby);
    RUN_TEST(test_one_failed_expander_does_not_corrupt_other_device_wake_sample);
    RUN_TEST(test_checked_adapter_reports_failed_mode_read_and_write_without_latch_corruption);
    RUN_TEST(test_healthy_matrix_scans_real_contacts_and_preserves_other_latch_bits);
    RUN_TEST(test_failed_later_row_read_discards_an_already_collected_press);
    RUN_TEST(test_failed_low_with_physical_effect_invalidates_scan_and_requires_recovery);
    RUN_TEST(test_failed_release_blocks_new_selection_until_all_columns_are_released);
    RUN_TEST(test_failed_startup_configuration_never_claims_ready_or_reallocates_retry);
    RUN_TEST(test_checked_begin_preserves_existing_output_latch_and_invalid_reads_clear_output);
    return UNITY_END();
}
#endif
