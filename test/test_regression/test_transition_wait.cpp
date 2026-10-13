#include <cstdint>
#include <unity.h>
#include "GameDomain/TransitionWait.h"

void test_transition_wait_is_nonblocking_and_resets_after_elapsed()
{
    bool started = false;
    std::uint32_t startedMilliseconds = 0U;
    TEST_ASSERT_FALSE(larp::domain::TransitionWait::elapsed(100U, started, startedMilliseconds, 500U));
    TEST_ASSERT_FALSE(larp::domain::TransitionWait::elapsed(599U, started, startedMilliseconds, 500U));
    TEST_ASSERT_TRUE(larp::domain::TransitionWait::elapsed(600U, started, startedMilliseconds, 500U));
    TEST_ASSERT_FALSE(started);
}
