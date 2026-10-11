#include <unity.h>
#include "../../lib/LedFx/LightParkFence.h"

void test_light_park_fence_is_one_way_and_correlated() {
    LightParkFence fence;
    TEST_ASSERT_FALSE(fence.blocksGameCommands());
    TEST_ASSERT_FALSE(fence.request(0U));
    TEST_ASSERT_TRUE(fence.request(17U));
    TEST_ASSERT_TRUE(fence.blocksGameCommands());
    TEST_ASSERT_TRUE(fence.workerShouldSubmitOff());
    TEST_ASSERT_FALSE(fence.request(18U));
    TEST_ASSERT_FALSE(fence.offFrameSubmittedFor(17U));
    fence.workerSubmittedOffFrame();
    TEST_ASSERT_FALSE(fence.workerShouldSubmitOff());
    TEST_ASSERT_FALSE(fence.offFrameSubmittedFor(18U));
    TEST_ASSERT_TRUE(fence.offFrameSubmittedFor(17U));
    TEST_ASSERT_FALSE(fence.request(17U));
}
