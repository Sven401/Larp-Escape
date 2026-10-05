#include "GamePorts/CrystalInputBatch.h"
#include <unity.h>

using larp::domain::GridColumn;
using larp::ports::CrystalInputBatch;
void setUp() {}
void tearDown() {}
void test_empty_batch_has_fixed_full_matrix_capacity() {
    CrystalInputBatch batch;
    TEST_ASSERT_TRUE(batch.empty());
    TEST_ASSERT_EQUAL_UINT(81U, batch.capacity());
    TEST_ASSERT_EQUAL_PTR(batch.begin(), batch.end());
}
void test_every_matrix_contact_fits_and_overflow_preserves_all_values() {
    CrystalInputBatch batch;
    for (unsigned row = 1U; row <= 9U; ++row)
        for (unsigned column = 0U; column < 9U; ++column)
            TEST_ASSERT_TRUE(
                batch.push_back({static_cast<std::uint8_t>(row), static_cast<GridColumn>(column)}));
    TEST_ASSERT_EQUAL_UINT(81U, batch.size());
    TEST_ASSERT_FALSE(batch.push_back({0U, GridColumn::A}));
    for (std::size_t index = 0; index < batch.size(); ++index) {
        TEST_ASSERT_EQUAL_UINT8(index / 9U + 1U, batch[index].row);
        TEST_ASSERT_EQUAL_UINT8(index % 9U, static_cast<unsigned>(batch[index].column));
    }
}
void test_input_order_duplicates_and_invalid_coordinates_are_preserved() {
    CrystalInputBatch batch{{3U, GridColumn::C}, {0U, GridColumn::A}, {3U, GridColumn::C}};
    TEST_ASSERT_EQUAL_UINT(3U, batch.size());
    TEST_ASSERT_EQUAL_UINT8(0U, batch[1].row);
    TEST_ASSERT_TRUE(batch[0] == batch[2]);
}
void test_value_copies_and_clear_do_not_expose_old_active_entries() {
    CrystalInputBatch batch{{2U, GridColumn::B}};
    const auto copy = batch;
    batch.clear();
    TEST_ASSERT_TRUE(batch.empty());
    TEST_ASSERT_EQUAL_UINT(1U, copy.size());
    batch = {{9U, GridColumn::I}, {1U, GridColumn::A}};
    TEST_ASSERT_EQUAL_UINT(2U, batch.size());
    TEST_ASSERT_EQUAL_UINT8(9U, batch[0].row);
    TEST_ASSERT_EQUAL_UINT8(2U, copy[0].row);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_batch_has_fixed_full_matrix_capacity);
    RUN_TEST(test_every_matrix_contact_fits_and_overflow_preserves_all_values);
    RUN_TEST(test_input_order_duplicates_and_invalid_coordinates_are_preserved);
    RUN_TEST(test_value_copies_and_clear_do_not_expose_old_active_entries);
    return UNITY_END();
}
