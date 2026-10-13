#include "../../lib/LedFx/burst.cpp"
#include "../../lib/LedFx/ripple.cpp"
#include "FastLED.h"
#include <unity.h>
HostSerial Serial;

struct GuardedLeds {
    CRGB before{17U, 23U, 41U};
    CRGB leds[23]{};
    CRGB after{71U, 83U, 97U};
};
void assertGuards(const GuardedLeds &buffer) {
    TEST_ASSERT_EQUAL_UINT8(17U, buffer.before.r);
    TEST_ASSERT_EQUAL_UINT8(41U, buffer.before.b);
    TEST_ASSERT_EQUAL_UINT8(71U, buffer.after.r);
    TEST_ASSERT_EQUAL_UINT8(97U, buffer.after.b);
}
void setUp() {}
void tearDown() {}
void test_actual_ripple_wraps_left_of_origin_two_without_touching_canaries() {
    GuardedLeds buffer;
    Ripple ripple(2U, buffer.leds, 30U, 23);
    ripple.fire(30U, 2U);
    for (unsigned frame = 0U; frame < 30U; ++frame) {
        ripple.draw();
        assertGuards(buffer);
    }
    TEST_ASSERT_GREATER_THAN_UINT8(0U, buffer.leds[22].b);
    const auto last = buffer.leds[2].b;
    ripple.draw();
    TEST_ASSERT_EQUAL_UINT8(last, buffer.leds[2].b);
}
void test_invalid_origins_and_empty_buffers_do_not_draw() {
    GuardedLeds buffer;
    Ripple ripple(2U, buffer.leds, 30U, 23);
    ripple.fire(30U, 23U);
    ripple.draw();
    TEST_ASSERT_EQUAL_UINT8(0U, buffer.leds[2].b);
    Ripple nullRipple(2U, nullptr, 30U, 23);
    nullRipple.fire(30U, 2U);
    nullRipple.draw();
    Ripple empty(0U, buffer.leds, 30U, 0);
    empty.fire(30U, 0U);
    empty.draw();
    assertGuards(buffer);
}
void test_small_and_zero_durations_are_bounded_and_effects_can_restart() {
    for (std::uint8_t frames = 0U; frames <= 5U; ++frames) {
        GuardedLeds buffer;
        Ripple ripple(2U, buffer.leds, 30U, 23);
        Burst burst(30U, buffer.leds, 23);
        ripple.fire(frames, 2U);
        burst.fire(frames);
        for (unsigned frame = 0U; frame < 8U; ++frame) {
            ripple.draw();
            burst.draw();
        }
        assertGuards(buffer);
        for (auto &led : buffer.leds)
            led = CRGB{};
        ripple.fire(1U, 2U);
        ripple.draw();
        TEST_ASSERT_GREATER_THAN_UINT8(0U, buffer.leds[2].b);
    }
}
void test_rejected_white_burst_preserves_the_active_green_color() {
    GuardedLeds buffer;
    Burst burst(30U, buffer.leds, 23);
    burst.fire(120U, CRGB(0U, 255U, 0U));
    burst.fire(15U, CRGB(255U, 255U, 255U));
    burst.draw();
    for (const auto &led : buffer.leds) {
        TEST_ASSERT_EQUAL_UINT8(0U, led.r);
        TEST_ASSERT_EQUAL_UINT8(0U, led.b);
        TEST_ASSERT_GREATER_THAN_UINT8(0U, led.g);
    }
    assertGuards(buffer);
}
void test_burst_null_or_empty_buffers_and_completed_frames_are_safe() {
    Burst nullBurst(30U, nullptr, 23);
    nullBurst.fire(1U);
    nullBurst.draw();
    GuardedLeds buffer;
    Burst empty(30U, buffer.leds, 0);
    empty.fire(1U);
    empty.draw();
    Burst burst(30U, buffer.leds, 23);
    burst.fire(1U);
    burst.draw();
    for (auto &led : buffer.leds)
        led = CRGB{};
    burst.draw();
    TEST_ASSERT_EQUAL_UINT8(0U, buffer.leds[0].r);
    assertGuards(buffer);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_actual_ripple_wraps_left_of_origin_two_without_touching_canaries);
    RUN_TEST(test_invalid_origins_and_empty_buffers_do_not_draw);
    RUN_TEST(test_small_and_zero_durations_are_bounded_and_effects_can_restart);
    RUN_TEST(test_rejected_white_burst_preserves_the_active_green_color);
    RUN_TEST(test_burst_null_or_empty_buffers_and_completed_frames_are_safe);
    return UNITY_END();
}
