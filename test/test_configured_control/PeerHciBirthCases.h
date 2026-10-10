#pragma once
#include "Companion/PeerHciBirthWitness.h"
#include "PeerLinkSecurityCases.h"

void test_hci_birth_witness_rejects_old_and_recycled_handles() {
    PeerHciBirthWitness witness;
    auto first = securityTransport();
    TEST_ASSERT_FALSE(witness.capture({}, 0x123));
    TEST_ASSERT_FALSE(witness.capture(first, 0x0fff));
    TEST_ASSERT_FALSE(witness.capture(first, 0xffff));
    TEST_ASSERT_TRUE(witness.capture(first, 0x123));
    TEST_ASSERT_FALSE(witness.capture(first, 0x124));
    TEST_ASSERT_TRUE(witness.matches(first, 0x123));
    const auto old = first;
    ++first.lifecycle;
    ++first.connection.token;
    TEST_ASSERT_FALSE(witness.matches(first, 0x123));
    witness.close();
    TEST_ASSERT_FALSE(witness.matches(old, 0x123));
    TEST_ASSERT_FALSE(witness.capture(first, 0x123));
    TEST_ASSERT_TRUE(witness.capture(first, 0x124));
    TEST_ASSERT_TRUE(witness.matches(first, 0x124));
    TEST_ASSERT_FALSE(witness.matches(old, 0x124));
}

void test_security_source_hci_observation_never_grants_security_or_roles() {
    SecurityFixture fixture;
    TEST_ASSERT_TRUE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    TEST_ASSERT_FALSE(fixture.read());
    fixture.event(7); // Restored-looking completion lacks an original pairing start.
    TEST_ASSERT_FALSE(fixture.read());
    TrustedAuthority authority;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, authority));
    // Even a later original pairing must still pass through current proof and role ACK.
    fixture.pair();
    TEST_ASSERT_TRUE(fixture.read());
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, authority));
    fixture.io();
    const auto reads = host_sdk_security::hciReads;
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    TEST_ASSERT_EQUAL(reads, host_sdk_security::hciReads);
}

void test_security_source_hci_birth_rejects_changed_sdk_origin() {
    for (unsigned variant = 0; variant < 6; ++variant) {
        SecurityFixture fixture(false);
        if (variant == 0)
            host_sdk_security::connectionResult = false;
        if (variant == 1)
            ++host_sdk_security::interface;
        if (variant == 2)
            host_sdk_security::transport = 1;
        if (variant == 3)
            ++host_sdk_security::address[0];
        if (variant == 4)
            host_sdk_security::hci = 0xffff;
        if (variant == 5)
            host_sdk_security::hci = 0x0fff;
        fixture.original.connection = fixture.open();
        fixture.actor();
        TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
        TEST_ASSERT_FALSE(fixture.read());
    }
}

void test_security_source_hci_birth_rejects_sdk_mutation() {
    SecurityFixture fixture;
    host_sdk_security::beforeHci = [](std::uint8_t *address) { ++address[0]; };
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    host_sdk_security::beforeHci = [](std::uint8_t *) {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(2);
    };
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.read());
}
