// Compiled only by the explicit offline --legacy-snapshot probe. The normal
// host-mcp suite never treats these intentionally failing checks as success.
#ifdef MCP_LEGACY_PROBE
#include "../../src/GameDomain/GameDefinitions.cpp"
#include "../../src/GameDomain/GameProgress.cpp"
#include "../../src/GameDomain/KeystoneMonitor.cpp"
#include "../../src/GameDomain/KeystoneTransition.cpp"
#include "../../src/GameDomain/PuzzleProgress.cpp"
#include "Fixture.h"
#include "Gamestate.h"
#include <unity.h>
void legacy_nack_must_not_wake_the_real_game() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x20].nack = true;
    rig.tick();
    TEST_ASSERT_TRUE_MESSAGE(rig.standby(), "safe behavior required: NACK must retain Standby");
}
void legacy_zero_read_must_not_wake_the_real_game() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x20].maxRead = 0;
    rig.tick();
    TEST_ASSERT_TRUE_MESSAGE(rig.standby(),
                             "safe behavior required: zero read must retain Standby");
}
void legacy_failed_write_must_not_report_success() {
    McpRig rig;
    TEST_ASSERT_TRUE(rig.ready);
    rig.bus.devices[0x21].nack = true;
    TEST_ASSERT_FALSE_MESSAGE(rig.columnHandler.writeGPIO(MCP_A0, LOW),
                              "NACK is not write success");
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(legacy_nack_must_not_wake_the_real_game);
    RUN_TEST(legacy_zero_read_must_not_wake_the_real_game);
    RUN_TEST(legacy_failed_write_must_not_report_success);
    return UNITY_END();
}
#endif
