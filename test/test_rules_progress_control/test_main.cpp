// Canonical representation tests only; authorization bytes are inert markers.
#include "DeviceRuntime/RulesProgressControl.h"
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
    Control control, decoded;
    std::uint8_t marker[2048] = {1}, encoded[3072], owned[2048];
    xr::Pool<std::uint8_t> bytes, evidence;
    Fixture() {
        control.operation = id(5);
        control.context.event = id(1);
        control.context.run = id(2);
        control.context.station = id(3);
        control.context.box = id(4);
        control.context.sourceEpoch = id(6);
        control.clock.boot = id(7);
        control.before = control.after = xr::Mode::Running;
        control.kind = ControlKind::Checkpoint;
        control.expected.activeTime = control.clock.activeBefore = 3;
        control.clock.activeAfter = 7;
        control.clock.monotonicBefore = 100;
        control.clock.monotonicAfter = 104;
        control.targetFence = control.expected;
        control.targetFence.activeTime = control.clock.activeAfter;
        control.authorization.data = marker;
        control.authorization.count = 1;
        bytes.data = encoded;
        bytes.capacity = sizeof(encoded);
        evidence.data = owned;
        evidence.capacity = sizeof(owned);
    }
};
void test_fixed_domain_bytes_full_clock_and_owned_roundtrip() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeControl(f.control, f.bytes).valid);
    const std::uint8_t prefix[] = {0xac, 0x00, 0x75, 'l', 'a', 'r', 'p',  '.', 'r',
                                   'u',  'l',  'e',  's', '.', 'c', 'o',  'n', 't',
                                   'r',  'o',  'l',  '.', 'v', '1', 0x01, 0x50};
    TEST_ASSERT_EQUAL_MEMORY(prefix, f.encoded, sizeof(prefix));
    TEST_ASSERT_TRUE(controlDomain(f.encoded, f.bytes.count));
    TEST_ASSERT_TRUE(decodeControl(f.encoded, f.bytes.count, f.decoded, f.evidence).valid);
    TEST_ASSERT_EQUAL(3, f.decoded.clock.activeBefore);
    TEST_ASSERT_EQUAL(7, f.decoded.clock.activeAfter);
    TEST_ASSERT_TRUE(f.decoded.authorization.data == f.owned);
    TEST_ASSERT_EQUAL(1, f.owned[0]);
    f.encoded[f.bytes.count - 1] = 2;
    TEST_ASSERT_EQUAL(1, f.owned[0]);
}
void test_every_shorter_prefix_and_capacity_fail_without_partial_publication_then_reuse() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeControl(f.control, f.bytes).valid);
    const auto length = f.bytes.count;
    for (std::size_t i = 0; i < length; ++i) {
        f.decoded.operation = id(88);
        f.evidence.count = 1;
        TEST_ASSERT_FALSE(decodeControl(f.encoded, i, f.decoded, f.evidence).valid);
        TEST_ASSERT_EQUAL(0, f.evidence.count);
        TEST_ASSERT_EQUAL(0, f.decoded.operation.bytes[15]);
        TEST_ASSERT_TRUE(decodeControl(f.encoded, length, f.decoded, f.evidence).valid);
    }
    std::uint8_t staged[3072];
    xr::Pool<std::uint8_t> destination;
    destination.data = staged;
    for (std::size_t i = 0; i < length; ++i) {
        destination.capacity = i;
        destination.count = 0;
        TEST_ASSERT_FALSE(encodeControl(f.control, destination).valid);
        TEST_ASSERT_EQUAL(0, destination.count);
    }
    destination.capacity = length;
    TEST_ASSERT_TRUE(encodeControl(f.control, destination).valid);
    TEST_ASSERT_EQUAL_MEMORY(f.encoded, staged, length);
}
void test_modes_time_delta_and_missing_authorization_are_structurally_rejected() {
    for (unsigned mode = 0; mode < 7; ++mode) {
        Fixture f;
        if (mode == 0)
            f.control.authorization.count = 0;
        if (mode == 1)
            f.control.clock.activeAfter = 6;
        if (mode == 2)
            f.control.clock.monotonicAfter = 99;
        if (mode == 3)
            f.control.kind = static_cast<ControlKind>(8);
        if (mode == 4)
            f.control.kind = ControlKind::Start;
        if (mode == 5)
            f.control.after = xr::Mode::Paused;
        if (mode == 6)
            f.control.context.event = {};
        TEST_ASSERT_FALSE(encodeControl(f.control, f.bytes).valid);
        TEST_ASSERT_EQUAL(0, f.bytes.count);
    }
}
void test_uint64_and_maximum_annotation_are_not_truncated() {
    Fixture f;
    f.control.context.bindingGeneration = UINT64_MAX;
    f.control.expected.activeTime = f.control.clock.activeBefore = f.control.clock.activeAfter =
        UINT64_MAX;
    f.control.clock.monotonicBefore = f.control.clock.monotonicAfter = UINT64_MAX;
    f.control.authorization.count = sizeof(f.marker);
    f.control.targetFence = f.control.expected;
    f.control.targetFence.activeTime = f.control.clock.activeAfter;
    TEST_ASSERT_TRUE(encodeControl(f.control, f.bytes).valid);
    TEST_ASSERT_TRUE(f.bytes.count <= 3072);
    TEST_ASSERT_TRUE(decodeControl(f.encoded, f.bytes.count, f.decoded, f.evidence).valid);
    TEST_ASSERT_TRUE(f.decoded.context.bindingGeneration == UINT64_MAX);
    TEST_ASSERT_TRUE(f.decoded.clock.activeAfter == UINT64_MAX);
    TEST_ASSERT_EQUAL(2048, f.evidence.count);
}
void test_unknown_domain_trailing_and_source_alias_never_publish() {
    Fixture f;
    TEST_ASSERT_TRUE(encodeControl(f.control, f.bytes).valid);
    const auto count = f.bytes.count;
    f.encoded[count] = 0;
    TEST_ASSERT_FALSE(decodeControl(f.encoded, count + 1, f.decoded, f.evidence).valid);
    TEST_ASSERT_EQUAL(0, f.evidence.count);
    f.encoded[3] = 'x';
    TEST_ASSERT_FALSE(controlDomain(f.encoded, count));
    TEST_ASSERT_FALSE(decodeControl(f.encoded, count, f.decoded, f.evidence).valid);
    f.encoded[3] = 'l';
    auto aliased = f.evidence;
    aliased.data = f.encoded;
    aliased.capacity = sizeof(f.encoded);
    TEST_ASSERT_FALSE(decodeControl(f.encoded, count, f.decoded, aliased).valid);
    TEST_ASSERT_TRUE(decodeControl(f.encoded, count, f.decoded, f.evidence).valid);
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fixed_domain_bytes_full_clock_and_owned_roundtrip);
    RUN_TEST(test_every_shorter_prefix_and_capacity_fail_without_partial_publication_then_reuse);
    RUN_TEST(test_modes_time_delta_and_missing_authorization_are_structurally_rejected);
    RUN_TEST(test_uint64_and_maximum_annotation_are_not_truncated);
    RUN_TEST(test_unknown_domain_trailing_and_source_alias_never_publish);
    return UNITY_END();
}
