#pragma once
#include "PeerHciEventCases.h"
namespace host_smp_origin {
// Explicit host byte model. Original target layout is independently checked by
// check_f1_smp_prereset_abi.py; this fixture is not ABI evidence.
struct Control {
    std::uint8_t bytes[720] = {};
    Control() {
        auto callback = host_sdk_security::callback;
        std::memcpy(bytes, &callback, sizeof(callback));
        const std::uint8_t address[] = {1, 2, 3, 4, 5, 6};
        std::memcpy(bytes + 37, address, sizeof(address));
        bytes[61] = 4;
    }
    sdk446::SmpControlBlock *get() {
        return reinterpret_cast<sdk446::SmpControlBlock *>(bytes);
    }
};
sdk446::SmpData completion(std::uint8_t mode = 13) {
    sdk446::SmpData data = {};
    const std::uint8_t bytes[] = {0, 4, 0, 0, mode};
    std::memcpy(&data, bytes, sizeof(bytes));
    return data;
}
void begin(SecurityFixture &fixture) {
    host_hci_event::Packet packet;
    TEST_ASSERT_TRUE(fixture.source.enterHciEventNow(0, &packet.header));
    TEST_ASSERT_TRUE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
}
unsigned forwarded = 0;
sdk446::SmpControlBlock *last = nullptr;
void (*duringCompletion)(sdk446::SmpControlBlock *) = nullptr;
} // namespace host_smp_origin
extern "C" void __real_smp_proc_pairing_cmpl(sdk446::SmpControlBlock *control) {
    ++host_smp_origin::forwarded;
    host_smp_origin::last = control;
    if (host_smp_origin::duringCompletion)
        host_smp_origin::duringCompletion(control);
}
void test_smp_origin_copy_before_reset_consumes_once_without_security_or_roles() {
    for (const auto mode : {13, 29}) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        host_smp_origin::begin(fixture);
        TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        std::memset(control.bytes, 0xa5, sizeof(control.bytes)); // No pointer retained.
        auto data = host_smp_origin::completion(mode);
        TEST_ASSERT_TRUE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
        TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
        fixture.event(7);
        TEST_ASSERT_FALSE(fixture.read());
        TrustedAuthority route;
        TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
        fixture.source.leaveSmpCompletionNow();
        fixture.source.leaveHciEventNow();
    }
}
void test_smp_origin_rejects_foreign_control_pairing_and_failure_before_reset() {
    for (unsigned variant = 0; variant < 9; ++variant) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        host_smp_origin::begin(fixture);
        if (variant == 0)
            std::memset(control.bytes, 0, 4);
        if (variant == 1)
            ++control.bytes[37];
        if (variant == 2)
            control.bytes[53] = 1;
        if (variant == 3)
            control.bytes[56] = 1;
        if (variant == 4)
            control.bytes[598] = 13;
        if (variant == 5)
            control.bytes[598] = 29;
        if (variant == 6)
            control.bytes[702] = 1;
        if (variant == 7)
            control.bytes[702] = 255;
        TEST_ASSERT_TRUE(
            fixture.source.enterSmpCompletionNow(variant == 8 ? nullptr : control.get()));
        auto data = host_smp_origin::completion();
        TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
    }
}
void test_smp_origin_rejects_callback_mismatch_and_all_unreviewed_mode_bytes() {
    for (unsigned mode = 0; mode < 256; ++mode) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        host_smp_origin::begin(fixture);
        TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        auto data = host_smp_origin::completion(mode);
        TEST_ASSERT_EQUAL(mode == 13 || mode == 29,
                          fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
    }
    for (unsigned variant = 0; variant < 8; ++variant) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        host_smp_origin::begin(fixture);
        TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        auto data = host_smp_origin::completion();
        if (variant < 4)
            ++reinterpret_cast<std::uint8_t *>(&data)[variant];
        auto address = std::array<std::uint8_t, 6>{{1, 2, 3, 4, 5, 6}};
        if (variant == 4)
            ++address[0];
        TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(
            variant == 5 ? 1 : 7, variant == 6 ? nullptr : address.data(),
            variant == 7 ? nullptr : &data));
    }
}
void test_smp_origin_requires_live_original_hci_scope_actor_and_birth() {
    for (unsigned variant = 0; variant < 9; ++variant) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        auto data = host_smp_origin::completion();
        if (variant != 0)
            host_smp_origin::begin(fixture);
        TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        if (variant == 1)
            fixture.source.leaveSmpCompletionNow();
        if (variant == 2)
            fixture.source.leaveHciEventNow();
        if (variant == 3)
            TEST_ASSERT_FALSE(fixture.source.enterSmpCompletionNow(control.get()));
        if (variant == 4)
            TEST_ASSERT_FALSE(fixture.source.enterHciEventNow(0, nullptr));
        if (variant == 5) {
            fixture.close();
            (void)fixture.open();
            fixture.actor();
        }
        if (variant == 6)
            fixture.io();
        if (variant == 7)
            host_sdk_security::hci = 0x124;
        if (variant == 8)
            host_sdk_security::beforeHci = [](std::uint8_t *) {
                TEST_ASSERT_FALSE(enterGattsSmpCompletionNow(nullptr));
            };
        TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
    }
}
void test_smp_origin_wrong_actor_cannot_read_control_or_steal_scope() {
    SecurityFixture fixture;
    host_smp_origin::Control control;
    host_smp_origin::begin(fixture);
    fixture.io();
    TEST_ASSERT_FALSE(fixture.source.enterSmpCompletionNow(control.get()));
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
    fixture.io();
    fixture.source.leaveSmpCompletionNow();
    fixture.actor();
    auto data = host_smp_origin::completion();
    TEST_ASSERT_TRUE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
}
void test_smp_origin_actual_wrapper_forwards_pointer_callback_and_ends_scope() {
    SecurityFixture fixture;
    host_smp_origin::Control control;
    host_smp_origin::begin(fixture);
    const auto calls = host_smp_origin::forwarded;
    host_smp_origin::duringCompletion = [](sdk446::SmpControlBlock *control) {
        auto data = host_smp_origin::completion();
        std::uint8_t address[] = {1, 2, 3, 4, 5, 6};
        std::memset(control, 0, 720); // Original reset precedes original callback.
        TEST_ASSERT_EQUAL(23, host_sdk_security::callback(7, address, &data));
    };
    __wrap_smp_proc_pairing_cmpl(control.get());
    host_smp_origin::duringCompletion = nullptr;
    TEST_ASSERT_EQUAL(calls + 1, host_smp_origin::forwarded);
    TEST_ASSERT_EQUAL_PTR(control.get(), host_smp_origin::last);
    auto data = host_smp_origin::completion();
    TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
    TEST_ASSERT_FALSE(fixture.read());
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
}

void test_smp_origin_hci_duplicate_and_second_completion_cannot_reuse_match() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        SecurityFixture fixture;
        host_smp_origin::Control control;
        host_smp_origin::begin(fixture);
        auto data = host_smp_origin::completion();
        TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        if (variant == 0) {
            TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x124, 1, 0));
        } else {
            TEST_ASSERT_TRUE(
                fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
            fixture.source.leaveSmpCompletionNow();
            TEST_ASSERT_TRUE(fixture.source.enterSmpCompletionNow(control.get()));
        }
        TEST_ASSERT_FALSE(fixture.source.consumeStoredSmpCompletionNow(7, fixture.address, &data));
    }
}
