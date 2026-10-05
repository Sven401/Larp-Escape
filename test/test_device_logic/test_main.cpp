#include "GameRuntime/WakePulseSchedule.h"
#include <cstdint>
#include <unity.h>

using larp::runtime::WakePulseSchedule;

void setUp() {}
void tearDown() {}

void test_wake_sequence_keeps_all_eight_holds_and_completes_without_a_final_edge() {
    WakePulseSchedule schedule;
    TEST_ASSERT_TRUE(schedule.requestWake(0U));
    TEST_ASSERT_TRUE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(99U, 250U));
    for (std::uint32_t edge = 1U; edge <= 7U; ++edge) {
        TEST_ASSERT_TRUE(schedule.advance(edge * 100U, 250U));
        TEST_ASSERT_EQUAL(edge % 2U == 0U, schedule.isHigh());
        TEST_ASSERT_TRUE(schedule.isWaking());
    }
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(799U, 250U));
    TEST_ASSERT_TRUE(schedule.isWaking());
    TEST_ASSERT_TRUE(schedule.advance(800U, 250U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.isWaking());
    TEST_ASSERT_FALSE(schedule.advance(1049U, 200U));
    TEST_ASSERT_TRUE(schedule.advance(1050U, 200U));
    TEST_ASSERT_TRUE(schedule.isHigh());
}

void test_delayed_service_never_compresses_pulses_or_toggles_twice() {
    WakePulseSchedule schedule;
    schedule.requestWake(0U);
    TEST_ASSERT_TRUE(schedule.advance(500U, 300U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(500U, 300U));
    TEST_ASSERT_FALSE(schedule.advance(599U, 300U));
    TEST_ASSERT_TRUE(schedule.advance(600U, 300U));
    TEST_ASSERT_TRUE(schedule.isHigh());
    TEST_ASSERT_TRUE(schedule.isWaking());
}

void test_repeated_wake_requests_cannot_shorten_the_final_low_hold() {
    WakePulseSchedule schedule;
    schedule.requestWake(0U);
    TEST_ASSERT_TRUE(schedule.advance(100U, 300U));
    TEST_ASSERT_FALSE(schedule.requestWake(150U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    for (std::uint32_t now = 200U; now <= 700U; now += 100U)
        schedule.advance(now, 300U);
    TEST_ASSERT_TRUE(schedule.isWaking());
    TEST_ASSERT_FALSE(schedule.requestWake(750U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(799U, 300U));
    TEST_ASSERT_TRUE(schedule.advance(800U, 300U));
    TEST_ASSERT_FALSE(schedule.isWaking());
    TEST_ASSERT_TRUE(schedule.requestWake(850U));
    TEST_ASSERT_TRUE(schedule.isHigh());
    TEST_ASSERT_TRUE(schedule.isWaking());
}

void test_entire_wake_sequence_survives_millisecond_rollover() {
    WakePulseSchedule schedule;
    schedule.requestWake(UINT32_MAX - 50U);
    TEST_ASSERT_FALSE(schedule.advance(48U, 300U));
    TEST_ASSERT_TRUE(schedule.advance(49U, 300U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    for (std::uint32_t edge = 2U; edge <= 7U; ++edge)
        TEST_ASSERT_TRUE(schedule.advance(edge * 100U - 51U, 300U));
    TEST_ASSERT_TRUE(schedule.isWaking());
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(748U, 300U));
    TEST_ASSERT_TRUE(schedule.advance(749U, 300U));
    TEST_ASSERT_FALSE(schedule.isWaking());
    TEST_ASSERT_FALSE(schedule.isHigh());
}

void test_periodic_holds_follow_the_physical_level() {
    WakePulseSchedule schedule;
    TEST_ASSERT_FALSE(schedule.advance(1000U, 200U));
    schedule.startPeriodic(0U, 100U);
    TEST_ASSERT_FALSE(schedule.advance(99U, 200U));
    TEST_ASSERT_TRUE(schedule.advance(100U, 200U));
    TEST_ASSERT_TRUE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(299U, 350U));
    TEST_ASSERT_TRUE(schedule.advance(300U, 350U));
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_TRUE(schedule.advance(650U, 200U));
    TEST_ASSERT_TRUE(schedule.isHigh());
}

void test_delayed_final_hold_stays_low_and_restarts_periodic_timing_from_service() {
    WakePulseSchedule schedule;
    schedule.requestWake(0U);
    for (std::uint32_t now = 100U; now <= 700U; now += 100U)
        schedule.advance(now, 300U);
    TEST_ASSERT_TRUE(schedule.isWaking());
    TEST_ASSERT_TRUE(schedule.advance(5000U, 300U));
    TEST_ASSERT_FALSE(schedule.isWaking());
    TEST_ASSERT_FALSE(schedule.isHigh());
    TEST_ASSERT_FALSE(schedule.advance(5299U, 200U));
    TEST_ASSERT_TRUE(schedule.advance(5300U, 200U));
    TEST_ASSERT_TRUE(schedule.isHigh());
}

void test_supplied_periodic_hold_cannot_replace_the_final_wake_hold() {
    WakePulseSchedule schedule;
    schedule.requestWake(0U);
    for (std::uint32_t now = 100U; now <= 700U; now += 100U)
        TEST_ASSERT_TRUE(schedule.advance(now, 0U));
    TEST_ASSERT_FALSE(schedule.advance(700U, 300U));
    TEST_ASSERT_FALSE(schedule.advance(799U, 300U));
    TEST_ASSERT_TRUE(schedule.advance(800U, 300U));
    TEST_ASSERT_FALSE(schedule.advance(1099U, 200U));
    TEST_ASSERT_TRUE(schedule.advance(1100U, 200U));
    TEST_ASSERT_TRUE(schedule.isHigh());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wake_sequence_keeps_all_eight_holds_and_completes_without_a_final_edge);
    RUN_TEST(test_delayed_service_never_compresses_pulses_or_toggles_twice);
    RUN_TEST(test_repeated_wake_requests_cannot_shorten_the_final_low_hold);
    RUN_TEST(test_entire_wake_sequence_survives_millisecond_rollover);
    RUN_TEST(test_periodic_holds_follow_the_physical_level);
    RUN_TEST(test_delayed_final_hold_stays_low_and_restarts_periodic_timing_from_service);
    RUN_TEST(test_supplied_periodic_hold_cannot_replace_the_final_wake_hold);
    return UNITY_END();
}