#include "GameRuntime/CardUidCopy.h"
#include <unity.h>

using larp::domain::CardUid;
using larp::runtime::tryCopyCardUid;
void setUp() {}
void tearDown() {}
void test_exact_seven_byte_uid_is_copied_as_a_value() {
    std::uint8_t bytes[] = {4, 16, 209, 202, 110, 103, 128};
    CardUid uid{};
    TEST_ASSERT_TRUE(tryCopyCardUid(bytes, sizeof(bytes), uid));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, uid.data(), uid.size());
    bytes[1] = 0U;
    TEST_ASSERT_EQUAL_UINT8(16U, uid[1]);
}
void test_null_and_every_other_reported_length_clear_old_uid() {
    std::uint8_t bytes[256] = {};
    for (std::size_t length = 0; length < 256; ++length) {
        if (length == 7U)
            continue;
        CardUid uid{{1, 2, 3, 4, 5, 6, 7}};
        TEST_ASSERT_FALSE(tryCopyCardUid(bytes, length, uid));
        const CardUid empty{};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(empty.data(), uid.data(), uid.size());
    }
    CardUid uid{{1, 2, 3, 4, 5, 6, 7}};
    TEST_ASSERT_FALSE(tryCopyCardUid(nullptr, 7U, uid));
    TEST_ASSERT_EQUAL_UINT8(0U, uid[0]);
}
void test_copy_preserves_adjacent_bytes_and_does_not_pad_short_cards() {
    struct GuardedUid {
        std::uint8_t before;
        CardUid uid;
        std::uint8_t after;
    };
    GuardedUid guarded{0xA5U, {}, 0x5AU};
    const std::uint8_t card[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    TEST_ASSERT_TRUE(tryCopyCardUid(card, 7U, guarded.uid));
    TEST_ASSERT_EQUAL_UINT8(0xA5U, guarded.before);
    TEST_ASSERT_EQUAL_UINT8(0x5AU, guarded.after);
    TEST_ASSERT_FALSE(tryCopyCardUid(card, 4U, guarded.uid));
    TEST_ASSERT_EQUAL_UINT8(0U, guarded.uid[6]);
    TEST_ASSERT_FALSE(tryCopyCardUid(card, 10U, guarded.uid));
    TEST_ASSERT_EQUAL_UINT8(0x5AU, guarded.after);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exact_seven_byte_uid_is_copied_as_a_value);
    RUN_TEST(test_null_and_every_other_reported_length_clear_old_uid);
    RUN_TEST(test_copy_preserves_adjacent_bytes_and_does_not_pad_short_cards);
    return UNITY_END();
}
