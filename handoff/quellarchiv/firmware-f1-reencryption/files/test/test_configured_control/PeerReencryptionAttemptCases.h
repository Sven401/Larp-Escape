#pragma once
#include "Companion/PeerReencryptionAttempt.h"
#include "Companion/PeerHciBirthWitness.h"
#include "PeerLinkSecurityCases.h"

void test_reencryption_attempt_never_borrows_old_birth_or_recycled_hci_handle() {
    PeerReencryptionAttempt gate;
    auto first = securityTransport();
    const auto old = first;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Started),
                      static_cast<unsigned>(gate.begin(first, 0x123, 0x0d, 100)));
    TEST_ASSERT_FALSE(gate.observed(first, 101));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Mismatch),
                      static_cast<unsigned>(gate.observe(first, 0x124, 0, 1, 102)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Observed),
                      static_cast<unsigned>(gate.observe(first, 0x123, 0, 1, 103)));
    TEST_ASSERT_TRUE(gate.observed(first, 104));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Consumed),
                      static_cast<unsigned>(gate.begin(first, 0x124, 0x1d, 104)));
    gate.close();
    ++first.lifecycle;
    ++first.connection.token;
    TEST_ASSERT_FALSE(gate.observed(old, 105));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Consumed),
                      static_cast<unsigned>(gate.begin(first, 0x123, 0x0d, 106)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Started),
                      static_cast<unsigned>(gate.begin(first, 0x124, 0x1d, 107)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Mismatch),
                      static_cast<unsigned>(gate.observe(old, 0x124, 0, 1, 108)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Observed),
                      static_cast<unsigned>(gate.observe(first, 0x124, 0, 1, 109)));
    TEST_ASSERT_TRUE(gate.observed(first, 110));
}

void test_reencryption_attempt_rejects_weak_mode_failure_and_absolute_timeout() {
    auto birth = securityTransport();
    for (unsigned mode = 0; mode < 256; ++mode) {
        PeerReencryptionAttempt gate;
        const auto outcome = gate.begin(birth, 1, static_cast<std::uint8_t>(mode), 1);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(mode == 13 || mode == 29
                              ? PeerReencryptionAttempt::Outcome::Started
                              : PeerReencryptionAttempt::Outcome::Invalid),
                          static_cast<unsigned>(outcome));
    }
    PeerReencryptionAttempt gate;
    const auto start = UINT32_MAX - 100;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Started),
                      static_cast<unsigned>(gate.begin(birth, 2, 13, start)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Duplicate),
                      static_cast<unsigned>(gate.begin(birth, 2, 13, start + 9999)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Expired),
                      static_cast<unsigned>(gate.observe(birth, 2, 0, 1, start + 10000)));
    TEST_ASSERT_FALSE(gate.observed(birth, start + 10001));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Started),
                      static_cast<unsigned>(gate.begin(birth, 3, 13, 42)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Failed),
                      static_cast<unsigned>(gate.observe(birth, 3, 1, 1, 43)));
    TEST_ASSERT_FALSE(gate.observed(birth, 44));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Started),
                      static_cast<unsigned>(gate.begin(birth, 4, 13, 45)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerReencryptionAttempt::Outcome::Failed),
                      static_cast<unsigned>(gate.observe(birth, 4, 0, 0, 46)));
    TEST_ASSERT_FALSE(gate.observed(birth, 47));
}

void test_hci_birth_witness_rejects_old_and_recycled_handles() {
    PeerHciBirthWitness witness;
    auto first = securityTransport();
    TEST_ASSERT_TRUE(witness.capture(first, 0x123));
    TEST_ASSERT_TRUE(witness.matches(first, 0x123));
    TEST_ASSERT_FALSE(witness.matches(first, 0x124));
    witness.close();
    TEST_ASSERT_FALSE(witness.matches(first, 0x123));
    ++first.lifecycle;
    ++first.connection.token;
    TEST_ASSERT_FALSE(witness.capture(first, 0x123));
    TEST_ASSERT_TRUE(witness.capture(first, 0x124));
    TEST_ASSERT_TRUE(witness.matches(first, 0x124));
    TEST_ASSERT_FALSE(witness.matches(first, 0x123));
}
