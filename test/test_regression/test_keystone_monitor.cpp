#include <unity.h>

#include "GameDomain/KeystoneMonitor.h"
#include "../../src/GameDomain/KeystoneMonitor.cpp"

using larp::domain::KeystoneMonitor;

void test_keystone_monitor_raises_error_after_five_consecutive_misses()
{
    KeystoneMonitor monitor;
    for (int miss = 1; miss < KeystoneMonitor::ErrorThreshold; ++miss)
    {
        monitor.observe(false);
        TEST_ASSERT_FALSE(monitor.hasError());
    }
    monitor.observe(false);
    TEST_ASSERT_TRUE(monitor.hasError());

    monitor.clearError();
    for (int miss = 0; miss < 4; ++miss)
    {
        monitor.observe(false);
    }
    monitor.observe(false);
    TEST_ASSERT_TRUE(monitor.hasError());
}

void test_keystone_monitor_match_breaks_miss_streak_and_clears_error()
{
    KeystoneMonitor monitor;
    for (int miss = 0; miss < 4; ++miss)
    {
        monitor.observe(false);
    }
    monitor.observe(true);
    monitor.observe(false);
    TEST_ASSERT_FALSE(monitor.hasError());
    for (int miss = 0; miss < 3; ++miss)
    {
        monitor.observe(false);
    }
    TEST_ASSERT_FALSE(monitor.hasError());
    monitor.observe(false);
    TEST_ASSERT_TRUE(monitor.hasError());

    monitor.observe(true);
    TEST_ASSERT_FALSE(monitor.hasError());
}
