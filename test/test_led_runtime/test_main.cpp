#include <cstddef>
#include <cstdint>
#include <limits>
#include <unity.h>

#include "GameRuntime/BrightnessStepSchedule.h"
#include "GameRuntime/LedIndex.h"

void setUp() {}
void tearDown() {}

void test_ripple_left_edge_wraps_inside_the_actual_23_led_buffer() {
    std::size_t index = 0U;
    TEST_ASSERT_TRUE(larp::runtime::tryWrapLedIndex(2 - 3, 23U, index));
    TEST_ASSERT_EQUAL_UINT32(22U, index);
    TEST_ASSERT_TRUE(larp::runtime::tryWrapLedIndex(22 + 3, 23U, index));
    TEST_ASSERT_EQUAL_UINT32(2U, index);
}

void test_ripple_wrapping_keeps_extreme_signed_positions_inside_buffer() {
    std::size_t index = 0U;
    TEST_ASSERT_TRUE(
        larp::runtime::tryWrapLedIndex(std::numeric_limits<std::int32_t>::min(), 23U, index));
    TEST_ASSERT_TRUE(index < 23U);
    TEST_ASSERT_TRUE(
        larp::runtime::tryWrapLedIndex(std::numeric_limits<std::int32_t>::max(), 23U, index));
    TEST_ASSERT_TRUE(index < 23U);
}

void test_empty_or_unrepresentable_led_buffers_are_rejected() {
    std::size_t index = 123U;
    TEST_ASSERT_FALSE(larp::runtime::tryWrapLedIndex(-1, 0U, index));
    TEST_ASSERT_EQUAL_UINT32(0U, index);
    TEST_ASSERT_FALSE(larp::runtime::tryWrapLedIndex(
        0, static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) + 1U, index));
    TEST_ASSERT_FALSE(larp::runtime::isLedOriginValid(0U, 0U));
    TEST_ASSERT_TRUE(larp::runtime::isLedOriginValid(22U, 23U));
    TEST_ASSERT_FALSE(larp::runtime::isLedOriginValid(23U, 23U));
}

void test_brightness_schedule_preserves_initial_delay() {
    larp::runtime::BrightnessStepSchedule schedule;
    TEST_ASSERT_FALSE(schedule.due(100U, 150U));
    TEST_ASSERT_FALSE(schedule.due(249U, 150U));
    TEST_ASSERT_TRUE(schedule.due(250U, 150U));
    schedule.markQueued(250U);
    TEST_ASSERT_FALSE(schedule.due(399U, 150U));
    TEST_ASSERT_TRUE(schedule.due(400U, 150U));
}

void test_brightness_step_is_retried_after_a_full_queue() {
    larp::runtime::BrightnessStepSchedule schedule;
    TEST_ASSERT_FALSE(schedule.due(100U, 150U));
    TEST_ASSERT_TRUE(schedule.due(250U, 150U));
    // No markQueued: the queue refused the step.
    TEST_ASSERT_TRUE(schedule.due(251U, 150U));
    schedule.markQueued(251U);
    TEST_ASSERT_FALSE(schedule.due(400U, 150U));
    TEST_ASSERT_TRUE(schedule.due(401U, 150U));
}

void test_brightness_schedule_remains_due_across_millisecond_wrap() {
    larp::runtime::BrightnessStepSchedule schedule;
    const std::uint32_t started = std::numeric_limits<std::uint32_t>::max() - 50U;
    TEST_ASSERT_FALSE(schedule.due(started, 150U));
    TEST_ASSERT_FALSE(schedule.due(98U, 150U));
    TEST_ASSERT_TRUE(schedule.due(99U, 150U));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ripple_left_edge_wraps_inside_the_actual_23_led_buffer);
    RUN_TEST(test_ripple_wrapping_keeps_extreme_signed_positions_inside_buffer);
    RUN_TEST(test_empty_or_unrepresentable_led_buffers_are_rejected);
    RUN_TEST(test_brightness_schedule_preserves_initial_delay);
    RUN_TEST(test_brightness_step_is_retried_after_a_full_queue);
    RUN_TEST(test_brightness_schedule_remains_due_across_millisecond_wrap);
    return UNITY_END();
}
