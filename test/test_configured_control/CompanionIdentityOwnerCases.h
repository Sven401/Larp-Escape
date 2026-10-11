#pragma once
#include "../../lib/Esp32CompanionIdentityOwner/Esp32CompanionIdentityOwner.h"

void assertCompanionClosed(const Esp32CompanionIdentityOwner &owner) {
    auto value = authority(73);
    const auto original = value;
    TEST_ASSERT_FALSE(owner.authority(value));
    TEST_ASSERT_TRUE(value == original);
}
void assertCompanionIdentityUnavailable(const Esp32CompanionIdentityOwner &owner) {
    Identity value;
    value.bytes[0] = 88;
    const auto original = value;
    TEST_ASSERT_FALSE(owner.identity(value));
    TEST_ASSERT_TRUE(value == original);
    BootIdentity boot;
    boot.high = 73;
    boot.low = 74;
    const auto originalBoot = boot;
    TEST_ASSERT_FALSE(owner.bootIdentity(boot));
    TEST_ASSERT_TRUE(boot == originalBoot);
    assertCompanionClosed(owner);
}
void test_product_identity_startup_create_restart_and_no_authority() {
    host_identity_nvs::reset();
    Esp32CompanionIdentityOwner first;
    assertCompanionIdentityUnavailable(first);
    TEST_ASSERT_TRUE(first.begin() == Esp32CompanionIdentityOwner::State::ReadyClosed);
    Identity original;
    TEST_ASSERT_TRUE(first.identity(original));
    TEST_ASSERT_TRUE(original.valid());
    BootIdentity firstBoot;
    TEST_ASSERT_TRUE(first.bootIdentity(firstBoot));
    TEST_ASSERT_TRUE(firstBoot.valid());
    assertCompanionClosed(first);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::sets);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::commits);
    TEST_ASSERT_EQUAL_UINT32(2, host_identity_nvs::fills);
    TEST_ASSERT_FALSE(host_identity_nvs::entropyActive);
    TEST_ASSERT_FALSE(host_identity_nvs::badEntropyOrder);
    TEST_ASSERT_TRUE(first.begin() == Esp32CompanionIdentityOwner::State::ReadyClosed);
    Esp32CompanionIdentityOwner restarted;
    TEST_ASSERT_TRUE(restarted.begin() == Esp32CompanionIdentityOwner::State::ReadyClosed);
    Identity actual;
    TEST_ASSERT_TRUE(restarted.identity(actual));
    TEST_ASSERT_TRUE(actual == original);
    BootIdentity restartedBoot;
    TEST_ASSERT_TRUE(restarted.bootIdentity(restartedBoot));
    TEST_ASSERT_TRUE(restartedBoot.valid());
    TEST_ASSERT_FALSE(restartedBoot == firstBoot);
    assertCompanionClosed(restarted);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::sets);
    TEST_ASSERT_EQUAL_UINT32(3, host_identity_nvs::fills);
}
void test_product_identity_failed_store_is_terminal_without_erase_or_retry() {
    host_identity_nvs::reset();
    host_identity_nvs::initError = 1; // Any non-ESP_OK NVS initialization failure.
    Esp32CompanionIdentityOwner owner;
    TEST_ASSERT_TRUE(owner.begin() == Esp32CompanionIdentityOwner::State::StoreUnavailable);
    assertCompanionIdentityUnavailable(owner);
    host_identity_nvs::initError = ESP_OK;
    TEST_ASSERT_TRUE(owner.begin() == Esp32CompanionIdentityOwner::State::StoreUnavailable);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::inits);
    TEST_ASSERT_EQUAL_UINT32(0, host_identity_nvs::opens);
    TEST_ASSERT_EQUAL_UINT32(0, host_identity_nvs::fills);
}
void test_product_identity_uncertain_commit_and_corrupt_record_never_publish() {
    host_identity_nvs::reset();
    host_identity_nvs::commitError = 1;
    host_identity_nvs::persistCommitError = true;
    Esp32CompanionIdentityOwner uncertain;
    TEST_ASSERT_TRUE(uncertain.begin() == Esp32CompanionIdentityOwner::State::IdentityUnavailable);
    assertCompanionIdentityUnavailable(uncertain);
    host_identity_nvs::commitError = 0;
    TEST_ASSERT_TRUE(uncertain.begin() == Esp32CompanionIdentityOwner::State::IdentityUnavailable);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::sets);
    host_identity_nvs::value[24] ^= 1;
    Esp32CompanionIdentityOwner corrupt;
    TEST_ASSERT_TRUE(corrupt.begin() == Esp32CompanionIdentityOwner::State::IdentityUnavailable);
    assertCompanionIdentityUnavailable(corrupt);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::sets);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::fills);
}
void test_product_boot_entropy_failure_stays_closed_without_rewriting_identity() {
    host_identity_nvs::reset();
    Esp32CompanionIdentityOwner first;
    TEST_ASSERT_TRUE(first.begin() == Esp32CompanionIdentityOwner::State::ReadyClosed);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::commits);
    host_identity_nvs::zeroEntropy = true;
    Esp32CompanionIdentityOwner failed;
    TEST_ASSERT_TRUE(failed.begin() == Esp32CompanionIdentityOwner::State::BootUnavailable);
    assertCompanionIdentityUnavailable(failed);
    TEST_ASSERT_TRUE(failed.begin() == Esp32CompanionIdentityOwner::State::BootUnavailable);
    TEST_ASSERT_EQUAL_UINT32(1, host_identity_nvs::commits);
    TEST_ASSERT_EQUAL_UINT32(3, host_identity_nvs::fills);
    TEST_ASSERT_FALSE(host_identity_nvs::entropyActive);
    TEST_ASSERT_FALSE(host_identity_nvs::badEntropyOrder);
}
Esp32CompanionIdentityOwner *startingCompanion = nullptr;
void observeStartingCompanion() {
    TEST_ASSERT_NOT_NULL(startingCompanion);
    TEST_ASSERT_TRUE(startingCompanion->begin() == Esp32CompanionIdentityOwner::State::Starting);
    assertCompanionIdentityUnavailable(*startingCompanion);
}
void test_product_identity_reentry_and_zero_entropy_stay_closed() {
    host_identity_nvs::reset();
    Esp32CompanionIdentityOwner owner;
    startingCompanion = &owner;
    host_identity_nvs::beforeGet = observeStartingCompanion;
    host_identity_nvs::namespacePresent = true;
    host_identity_nvs::zeroEntropy = true;
    TEST_ASSERT_TRUE(owner.begin() == Esp32CompanionIdentityOwner::State::IdentityUnavailable);
    startingCompanion = nullptr;
    assertCompanionIdentityUnavailable(owner);
    TEST_ASSERT_EQUAL_UINT32(0, host_identity_nvs::sets);
    TEST_ASSERT_FALSE(host_identity_nvs::entropyActive);
}
