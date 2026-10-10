#pragma once
#include "../../src/GattsAbiProbe/F1EncryptAbi.cpp"
#include "PeerLinkSecurityCases.h"

namespace host_hci_event {
struct Packet {
    sdk446::HciHeader header = {0x1000, 6, 0, 0};
    std::uint8_t bytes[6] = {8, 4, 0, 0x23, 1, 1};
};
static_assert(offsetof(Packet, bytes) == sizeof(sdk446::HciHeader), "original packet prefix");
unsigned forwarded = 0, securityForwarded = 0;
std::uint8_t controller = 0, status = 0, enabled = 0;
std::uint16_t handle = 0;
sdk446::HciHeader *packet = nullptr;
void (*duringDispatch)() = nullptr;
} // namespace host_hci_event

extern "C" void __real_btu_hcif_process_event(std::uint8_t controller, sdk446::HciHeader *packet) {
    ++host_hci_event::forwarded;
    host_hci_event::controller = controller;
    host_hci_event::packet = packet;
    if (host_hci_event::duringDispatch)
        host_hci_event::duringDispatch();
}
extern "C" void __real_btm_sec_encrypt_change(std::uint16_t handle, std::uint8_t status,
                                              std::uint8_t enabled) {
    ++host_hci_event::securityForwarded;
    host_hci_event::handle = handle;
    host_hci_event::status = status;
    host_hci_event::enabled = enabled;
}

void test_hci_event_only_original_change_scope_consumes_once_without_rights() {
    SecurityFixture fixture;
    host_hci_event::Packet packet;
    TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
    TEST_ASSERT_TRUE(fixture.source.enterHciEventNow(0, &packet.header));
    std::memset(packet.bytes, 0, sizeof(packet.bytes)); // The borrowed bytes were copied.
    TEST_ASSERT_TRUE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
    TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
    fixture.source.leaveHciEventNow();
    TEST_ASSERT_FALSE(fixture.read());
    fixture.event(7);
    TEST_ASSERT_FALSE(fixture.read());
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
}

void test_hci_event_rejects_other_kinds_lengths_controllers_status_and_handles() {
    for (unsigned variant = 0; variant < 14; ++variant) {
        SecurityFixture fixture;
        host_hci_event::Packet packet;
        std::uint8_t controller = 0;
        if (variant == 0)
            packet.bytes[0] = 0x30; // Encryption Key Refresh Complete.
        if (variant == 1)
            packet.bytes[0] = 0x0f; // Command Status.
        if (variant == 2)
            packet.bytes[1] = 3;
        if (variant == 3)
            packet.header.length = 5;
        if (variant == 4)
            packet.header.length = 7;
        if (variant == 5)
            packet.header.offset = 1;
        if (variant == 6)
            packet.header.event = 0x1001;
        if (variant == 7)
            controller = 1;
        if (variant == 8)
            packet.bytes[2] = 1;
        if (variant == 9)
            packet.bytes[5] = 0;
        if (variant == 10)
            packet.bytes[5] = 2;
        if (variant == 11)
            packet.bytes[3] = 0x24;
        if (variant == 12)
            packet.bytes[4] = 0x11;
        TEST_ASSERT_TRUE(
            fixture.source.enterHciEventNow(controller, variant == 13 ? nullptr : &packet.header));
        TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
        fixture.source.leaveHciEventNow();
    }
}

void test_hci_event_mismatch_end_nested_and_new_birth_cannot_reuse_observation() {
    for (unsigned variant = 0; variant < 7; ++variant) {
        SecurityFixture fixture;
        host_hci_event::Packet packet;
        TEST_ASSERT_TRUE(fixture.source.enterHciEventNow(0, &packet.header));
        if (variant == 0)
            TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x124, 0, 1));
        if (variant == 1)
            TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 1, 1));
        if (variant == 2)
            TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 0));
        if (variant == 3)
            fixture.source.leaveHciEventNow();
        if (variant == 4)
            TEST_ASSERT_FALSE(fixture.source.enterHciEventNow(0, &packet.header));
        if (variant == 5) {
            fixture.close();
            (void)fixture.open();
            fixture.actor();
        }
        if (variant == 6)
            host_sdk_security::hci = 0x124;
        TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
        fixture.source.leaveHciEventNow();
    }
}

void test_hci_event_nested_sdk_read_invalidates_entry_and_consumption() {
    SecurityFixture fixture;
    host_hci_event::Packet packet;
    host_sdk_security::beforeHci = [](std::uint8_t *) {
        host_hci_event::Packet nested;
        TEST_ASSERT_FALSE(enterGattsHciEventNow(0, &nested.header));
    };
    TEST_ASSERT_TRUE(fixture.source.enterHciEventNow(0, &packet.header));
    TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
    fixture.source.leaveHciEventNow();
    TEST_ASSERT_TRUE(fixture.source.enterHciEventNow(0, &packet.header));
    host_sdk_security::beforeHci = [](std::uint8_t *) {
        TEST_ASSERT_FALSE(enterGattsHciEventNow(0, nullptr));
    };
    TEST_ASSERT_FALSE(fixture.source.consumeHciEncryptionEventNow(0x123, 0, 1));
    fixture.source.leaveHciEventNow();
}

void test_hci_event_actual_wrappers_forward_original_arguments_and_close_scope() {
    SecurityFixture fixture;
    host_hci_event::Packet packet;
    const auto forwarded = host_hci_event::forwarded;
    const auto securityForwarded = host_hci_event::securityForwarded;
    host_hci_event::duringDispatch = []() {
        TEST_ASSERT_TRUE(observeGattsHciEncryptionNow(0x123, 0, 1));
        __wrap_btm_sec_encrypt_change(0x123, 0, 1);
        TEST_ASSERT_FALSE(observeGattsHciEncryptionNow(0x123, 0, 1));
    };
    __wrap_btu_hcif_process_event(0, &packet.header);
    host_hci_event::duringDispatch = nullptr;
    TEST_ASSERT_EQUAL(forwarded + 1, host_hci_event::forwarded);
    TEST_ASSERT_EQUAL_PTR(&packet.header, host_hci_event::packet);
    TEST_ASSERT_EQUAL(0, host_hci_event::controller);
    TEST_ASSERT_EQUAL(securityForwarded + 1, host_hci_event::securityForwarded);
    TEST_ASSERT_EQUAL(0x123, host_hci_event::handle);
    TEST_ASSERT_FALSE(observeGattsHciEncryptionNow(0x123, 0, 1));
    __wrap_btu_hcif_process_event(1, &packet.header);
    TEST_ASSERT_EQUAL(1, host_hci_event::controller);
    TEST_ASSERT_EQUAL_PTR(&packet.header, host_hci_event::packet);
    __wrap_btm_sec_encrypt_change(0x456, 0x06, 0);
    TEST_ASSERT_EQUAL(0x456, host_hci_event::handle);
    TEST_ASSERT_EQUAL(0x06, host_hci_event::status);
    TEST_ASSERT_EQUAL(0, host_hci_event::enabled);
}
