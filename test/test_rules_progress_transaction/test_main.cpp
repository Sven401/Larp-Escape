// Structural envelope tests only; opaque inner marker is not an admitted App.
#include "DeviceRuntime/RulesProgressTransaction.h"
#include <cstring>
#include <unity.h>
using namespace larp::devices::rules_progress;
namespace xr = larp::execution::rules;
namespace {
xr::Identity id(unsigned n) {
    xr::Identity v;
    v.bytes[15] = static_cast<std::uint8_t>(n);
    return v;
}
struct Fixture {
    Transaction transaction, decoded;
    IntentReservation reservations[2], ownedReservations[2];
    std::uint8_t app[32768] = {0xa0}, encoded[32768], owned[32768];
    xr::Pool<std::uint8_t> bytes, application;
    Fixture() {
        transaction.operation = id(1);
        transaction.application.data = app;
        transaction.application.count = 1;
        transaction.intents.data = reservations;
        transaction.intents.count = transaction.intents.capacity = 2;
        for (unsigned i = 0; i < 2; ++i) {
            reservations[i].index = i;
            reservations[i].claim = id(10 + i * 2);
            reservations[i].result = id(11 + i * 2);
        }
        decoded.intents.data = ownedReservations;
        decoded.intents.capacity = 2;
        bytes.data = encoded;
        bytes.capacity = sizeof(encoded);
        application.data = owned;
        application.capacity = sizeof(owned);
    }
};
void test_full_original_application_and_ordered_phase_ids_are_owned_and_immutable() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeTransaction(f.transaction, f.bytes).valid);
    TEST_ASSERT_EQUAL_HEX8(0xa6, f.encoded[0]);
    TEST_ASSERT_TRUE(decodeTransaction(f.encoded, f.bytes.count, f.decoded, f.application).valid);
    TEST_ASSERT_EQUAL(2, f.decoded.intents.count);
    TEST_ASSERT_EQUAL(0, f.decoded.intents.data[0].index);
    TEST_ASSERT_EQUAL(1, f.decoded.intents.data[1].index);
    TEST_ASSERT_TRUE(f.decoded.application.data == f.owned);
    TEST_ASSERT_EQUAL_HEX8(0xa0, f.owned[0]);
    f.app[0] = 0;
    f.encoded[f.bytes.count - 1] ^= 1;
    TEST_ASSERT_EQUAL_HEX8(0xa0, f.owned[0]);
    TEST_ASSERT_EQUAL(13, f.decoded.intents.data[1].result.bytes[15]);
}
void test_all_shorter_prefixes_fail_without_partial_replay_then_healthy_reuse() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeTransaction(f.transaction, f.bytes).valid);
    const auto size = f.bytes.count;
    for (std::size_t n = 0; n < size; ++n) {
        TEST_ASSERT_FALSE(decodeTransaction(f.encoded, n, f.decoded, f.application).valid);
        TEST_ASSERT_EQUAL(0, f.application.count);
        TEST_ASSERT_EQUAL(0, f.decoded.intents.count);
        TEST_ASSERT_EQUAL(0, f.decoded.operation.bytes[15]);
        TEST_ASSERT_TRUE(decodeTransaction(f.encoded, size, f.decoded, f.application).valid);
    }
}
void test_parent_and_claim_result_collision_and_reordering_never_encode() {
    for (unsigned mode = 0; mode < 5; ++mode) {
        Fixture f;
        if (mode == 0)
            f.reservations[0].claim = f.transaction.operation;
        if (mode == 1)
            f.reservations[1].result = f.reservations[0].claim;
        if (mode == 2)
            f.reservations[0].result = f.reservations[0].claim;
        if (mode == 3)
            f.reservations[1].index = 0;
        if (mode == 4)
            f.reservations[0].claim = {};
        TEST_ASSERT_FALSE(encodeTransaction(f.transaction, f.bytes).valid);
        TEST_ASSERT_EQUAL(0, f.bytes.count);
    }
}
void test_profile_covers_whole_envelope_not_just_32768_byte_inner() {
    Fixture f;
    f.transaction.application.count = 32768;
    TEST_ASSERT_FALSE(encodeTransaction(f.transaction, f.bytes).valid);
    TEST_ASSERT_EQUAL(0, f.bytes.count);
    f.transaction.application.count = 32000;
    TEST_ASSERT_TRUE(encodeTransaction(f.transaction, f.bytes).valid);
    TEST_ASSERT_TRUE(f.bytes.count > 32000 && f.bytes.count <= 32768);
    TEST_ASSERT_TRUE(decodeTransaction(f.encoded, f.bytes.count, f.decoded, f.application).valid);
    TEST_ASSERT_EQUAL(32000, f.application.count);
}
void test_source_output_alias_and_capacity_guard_before_any_publication() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeTransaction(f.transaction, f.bytes).valid);
    const auto count = f.bytes.count;
    auto alias = f.application;
    alias.data = f.encoded;
    alias.capacity = sizeof(f.encoded);
    TEST_ASSERT_FALSE(decodeTransaction(f.encoded, count, f.decoded, alias).valid);
    f.application.capacity = 0;
    TEST_ASSERT_FALSE(decodeTransaction(f.encoded, count, f.decoded, f.application).valid);
    TEST_ASSERT_EQUAL(0, f.decoded.intents.count);
    f.application.capacity = sizeof(f.owned);
    TEST_ASSERT_TRUE(decodeTransaction(f.encoded, count, f.decoded, f.application).valid);
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_original_application_and_ordered_phase_ids_are_owned_and_immutable);
    RUN_TEST(test_all_shorter_prefixes_fail_without_partial_replay_then_healthy_reuse);
    RUN_TEST(test_parent_and_claim_result_collision_and_reordering_never_encode);
    RUN_TEST(test_profile_covers_whole_envelope_not_just_32768_byte_inner);
    RUN_TEST(test_source_output_alias_and_capacity_guard_before_any_publication);
    return UNITY_END();
}
