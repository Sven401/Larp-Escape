#include <unity.h>

#include "GameDomain/KeystoneTransition.h"
#include "../../src/GameDomain/KeystoneTransition.cpp"

using larp::domain::CardUid;
using larp::domain::KeystoneTransition;
using larp::domain::classifyKeystoneTransition;
using larp::domain::hasEnoughEmptyKeystoneReads;

void test_empty_keystone_read_never_starts_a_role_transition()
{
    const CardUid empty = {};
    const CardUid knownRole = {{4, 0x9D, 0xD1, 0xCA, 0x6E, 0x67, 0x80}};

    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::EmptyRead),
                          static_cast<int>(classifyKeystoneTransition(empty, empty, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::EmptyRead),
                          static_cast<int>(classifyKeystoneTransition(empty, knownRole, true)));
}

void test_five_empty_keystone_reads_are_required_to_clear_board_state()
{
    for (std::uint8_t readCount = 0; readCount < 5; ++readCount)
    {
        TEST_ASSERT_FALSE(hasEnoughEmptyKeystoneReads(readCount));
    }
    TEST_ASSERT_TRUE(hasEnoughEmptyKeystoneReads(5));
}

void test_keystone_transition_requires_board_clear_and_known_role()
{
    const CardUid empty = {};
    const CardUid roleA = {{4, 0x9D, 0xD1, 0xCA, 0x6E, 0x67, 0x80}};
    const CardUid roleB = {{4, 0xB1, 0xD1, 0xCA, 0x6E, 0x67, 0x80}};
    const CardUid unknown = {{4, 1, 2, 3, 4, 5, 6}};

    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::RoleAccepted),
                          static_cast<int>(classifyKeystoneTransition(roleA, empty, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::UnknownRole),
                          static_cast<int>(classifyKeystoneTransition(unknown, empty, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::RepeatedPreviousKeystone),
                          static_cast<int>(classifyKeystoneTransition(roleA, roleA, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(KeystoneTransition::BoardNotCleared),
                          static_cast<int>(classifyKeystoneTransition(roleB, roleA, true)));
}
