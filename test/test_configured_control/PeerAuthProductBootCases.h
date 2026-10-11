#pragma once
#include "PeerAuthCases.h"

void test_product_auth_worker_requires_valid_boot_and_grant_before_read_ingress() {
    RegistryActorFixture fixture;
    PeerNonceModel nonce;
    FreeRtosPeerProofGrantRuntime wrongBoot(fixture.source, fixture.publication,
                                            fixture.clock, BootIdentity{});
    PeerProofOwner wrongProof(wrongBoot, fixture.crypto, nonce, fixture.clock);
    FreeRtosPeerChallengeReadRuntime early(fixture.source, wrongProof, wrongBoot,
                                           fixture.clock);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_FALSE(early.beginWorker());
    TEST_ASSERT_FALSE(wrongBoot.beginWorker());
    FreeRtosPeerProofGrantRuntime grant(fixture.source, fixture.publication,
                                       fixture.clock, f->boot);
    PeerProofOwner proof(grant, fixture.crypto, nonce, fixture.clock);
    FreeRtosPeerChallengeReadRuntime reader(fixture.source, proof, grant, fixture.clock);
    TEST_ASSERT_TRUE(grant.beginWorker());
    TEST_ASSERT_TRUE(reader.beginWorker());
    TEST_ASSERT_FALSE(early.beginWorker());
    PeerRegistrySnapshot snapshot;
    TrustedAuthority authority;
    TEST_ASSERT_FALSE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(authority));
    fixture.ready();
    TEST_ASSERT_TRUE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_EQUAL(0, snapshot.count);
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(authority));
}

void test_product_auth_empty_installed_registry_rejects_selector_without_nonce_or_role() {
    SecurityFixture fixture(false);
    PeerRegistrySnapshot empty = registryValue(2, false);
    empty.count = 0;
    empty.peers = {};
    fixture.save(empty);
    PeerRegistrySnapshot installed;
    TEST_ASSERT_TRUE(fixture.publication.snapshot(installed));
    TEST_ASSERT_EQUAL(0, installed.count);
    FreeRtosPeerProofGrantRuntime grant(fixture.source, fixture.publication,
                                       fixture.clock, f->boot);
    PeerNonceModel nonce;
    PeerProofOwner proof(grant, fixture.crypto, nonce, fixture.clock);
    FreeRtosPeerChallengeReadRuntime reader(fixture.source, proof, grant, fixture.clock);
    FreeRtosPeerChallengeIssueRuntime issuer(fixture.source, proof, grant,
                                             fixture.publication, fixture.clock);
    PeerProofReceiver receiver(proof);
    FreeRtosPeerAuthRuntime auth(fixture.source, proof, receiver, grant, fixture.clock);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_TRUE(grant.beginWorker());
    TEST_ASSERT_TRUE(reader.beginWorker());
    TEST_ASSERT_TRUE(issuer.beginWorker());
    TEST_ASSERT_TRUE(auth.beginWorker());
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.configurePeerProofGrantNow(f->boot, grant));
    TEST_ASSERT_TRUE(fixture.source.configureChallengeReadNow(0x44, reader));
    TEST_ASSERT_TRUE(fixture.source.configureChallengeIssueNow(0x45, issuer));
    TEST_ASSERT_TRUE(fixture.source.configurePeerAuthNow(0x46, 0x47, auth));
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    std::array<std::uint8_t, PeerChallengeIssueLimits::Bytes> selector;
    TEST_ASSERT_TRUE(encodePeerChallengeIssue(testIdentity(2), selector));
    sdk446::Request request = {};
    request.write.handle = 0x45;
    request.write.length = selector.size();
    std::memcpy(request.write.value, selector.data(), selector.size());
    host_sdk_source::registeredCallbacks.request(257, 1, 2, &request);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    const auto outcome = issuer.service();
    issueDispatch(PeerChallengeIssueDispatch::Processed, outcome);
    peerOutcome(PeerProofOutcome::InvalidCredential, outcome.outcome);
    TEST_ASSERT_EQUAL(0, nonce.calls);
    TrustedAuthority authority;
    if (f->channel.actual.readAuthority(authority)) {
        TEST_ASSERT_FALSE(authority.gameMaster);
        TEST_ASSERT_EQUAL(0, authority.principal);
    }
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, authority));
}
