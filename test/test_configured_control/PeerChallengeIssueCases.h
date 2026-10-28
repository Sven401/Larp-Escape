#pragma once
#include "PeerChallengeReadCases.h"
void issueDispatch(PeerChallengeIssueDispatch expected, const PeerChallengeIssueResult &actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual.dispatch));
}
struct ChallengeIssueFixture : SecurityFixture {
    FreeRtosPeerProofGrantRuntime grant{source, publication, clock, f->boot};
    PeerNonceModel nonce;
    PeerProofOwner proof;
    FreeRtosPeerChallengeReadRuntime reader{source, proof, grant, clock};
    FreeRtosPeerChallengeIssueRuntime issuer{source, proof, grant, publication, clock};
    struct RecordingIngress : IPeerChallengeIssueIngress {
        FreeRtosPeerChallengeIssueRuntime &worker;
        PeerChallengeIssueRequest copied;
        unsigned calls = 0;
        explicit RecordingIngress(FreeRtosPeerChallengeIssueRuntime &value) : worker(value) {}
        bool copyIssue(const PeerChallengeIssueRequest &value) noexcept override {
            copied = value;
            ++calls;
            return worker.copyIssue(value);
        }
    } recording{issuer};
    PeerProofReceiver authReceiver{proof};
    FreeRtosPeerAuthRuntime auth{source, proof, authReceiver, grant, clock};
    struct AuthRecording : IPeerAuthIngress {
        FreeRtosPeerAuthRuntime &worker;
        PeerProofWriteRequest copied;
        PeerChallengeReadRequest readCopied;
        unsigned proofCopies = 0;
        explicit AuthRecording(FreeRtosPeerAuthRuntime &w) : worker(w) {}
        bool copyProof(const PeerProofWriteRequest &v) noexcept override {
            copied = v;
            ++proofCopies;
            return worker.copyProof(v);
        }
        bool copyResultRead(const PeerChallengeReadRequest &v) noexcept override {
            readCopied = v;
            return worker.copyResultRead(v);
        }
        bool workerNow() const noexcept override {
            return worker.workerNow();
        }
    } authRecording{auth};
    std::uint32_t transaction = 0;
    explicit ChallengeIssueFixture(IPeerProofCrypto *actual = nullptr, std::uint32_t reg = 0,
                                   bool full = false, bool gm = true, bool output = false)
        : SecurityFixture(false, gm, actual, reg),
          proof(grant, actual ? *actual : crypto, nonce, clock) {
        worker();
        TEST_ASSERT_TRUE(grant.beginWorker() && reader.beginWorker() && issuer.beginWorker());
        actor();
        TEST_ASSERT_TRUE(source.configurePeerProofGrantNow(f->boot, grant));
        TEST_ASSERT_TRUE(source.configureChallengeReadNow(0x44, reader));
        TEST_ASSERT_TRUE(source.configureChallengeIssueNow(0x45, recording));
        if (full) {
            worker();
            TEST_ASSERT_TRUE(auth.beginWorker());
            actor();
            TEST_ASSERT_TRUE(source.configurePeerAuthNow(0x46, 0x47, authRecording));
        }
        if (output)
            TEST_ASSERT_TRUE(source.configureOutputNow(0x48));
        original.connection = open();
        actor();
        pair();
        worker();
    }
    void worker() {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    }
    void write(const Identity &peer = testIdentity(2), unsigned variant = 0) {
        actor();
        sdk446::Request request = {};
        std::array<std::uint8_t, 18> bytes;
        TEST_ASSERT_TRUE(encodePeerChallengeIssue(peer, bytes));
        request.write.handle = variant == 1 ? 0x46 : 0x45;
        request.write.length = variant == 2 ? 17 : variant == 3 ? 19 : 18;
        request.write.prepared = variant == 4;
        request.write.offset = variant == 5 ? 1 : 0;
        request.write.needResponse = variant == 6;
        std::memcpy(request.write.value, bytes.data(), bytes.size());
        if (variant == 7)
            request.write.value[0] = 2;
        host_sdk_source::registeredCallbacks.request(variant == 8 ? 513 : 257, ++transaction, 2,
                                                     &request);
        std::memset(&request, 0xaa, sizeof(request));
        worker();
    }
    PeerChallengeIssueResult process() {
        worker();
        return issuer.service();
    }
    void read() {
        actor();
        sdk446::Request request = {};
        request.read = {0x44, 0, false, true};
        host_sdk_source::registeredCallbacks.request(257, ++transaction, 1, &request);
        worker();
        readDispatch(PeerChallengeReadDispatch::Posted, reader.service());
        actor();
        host_sdk_source::drainOne();
        worker();
    }
};
void test_issue_codec_exact_selector_no_credentials_and_failed_destination_preserved() {
    const auto peer = testIdentity(2);
    std::array<std::uint8_t, 18> bytes;
    TEST_ASSERT_TRUE(encodePeerChallengeIssue(peer, bytes));
    TEST_ASSERT_EQUAL(1, bytes[0]);
    TEST_ASSERT_EQUAL(11, bytes[1]);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peer.bytes.data(), bytes.data() + 2, 16);
    Identity out = testIdentity(9);
    for (unsigned n = 0; n < 21; ++n) {
        if (n == 18)
            continue;
        TEST_ASSERT_FALSE(decodePeerChallengeIssue(bytes.data(), n, out));
        TEST_ASSERT_TRUE(out == testIdentity(9));
    }
    TEST_ASSERT_FALSE(decodePeerChallengeIssue(nullptr, 18, out));
    for (unsigned index = 0; index < 2; ++index) {
        auto bad = bytes;
        ++bad[index];
        TEST_ASSERT_FALSE(decodePeerChallengeIssue(bad.data(), bad.size(), out));
    }
    auto zero = bytes;
    std::fill(zero.begin() + 2, zero.end(), 0);
    TEST_ASSERT_FALSE(decodePeerChallengeIssue(zero.data(), zero.size(), out));
    auto preserved = bytes;
    TEST_ASSERT_FALSE(encodePeerChallengeIssue(Identity{}, preserved));
    TEST_ASSERT_TRUE(preserved == bytes);
    TEST_ASSERT_TRUE(decodePeerChallengeIssue(bytes.data(), 18, out) && out == peer);
}
void test_issue_actual_sdk_copy_worker_registry_and_read_without_gm() {
    ChallengeIssueFixture fixture;
    fixture.write();
    TEST_ASSERT_EQUAL(0, fixture.nonce.calls);
    const auto result = fixture.process();
    issueDispatch(PeerChallengeIssueDispatch::Processed, result);
    peerOutcome(PeerProofOutcome::Issued, result.outcome);
    fixture.read();
    const auto &read = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(0, read.status);
    TEST_ASSERT_EQUAL(108, read.response.value.length);
    PeerChallenge challenge;
    TEST_ASSERT_TRUE(
        decodePeerChallenge(read.response.value.value, read.response.value.length, challenge));
    TEST_ASSERT_TRUE(challenge.original == fixture.original && challenge.peer == testIdentity(2));
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    TEST_ASSERT_TRUE(host_sdk_source::sent.empty());
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_issue_invalid_write_modes_sizes_context_and_missing_peer_never_call_crypto() {
    for (unsigned variant = 1; variant <= 8; ++variant) {
        ChallengeIssueFixture fixture;
        fixture.write(testIdentity(2), variant);
        const auto result = fixture.process();
        issueDispatch(variant == 7 ? PeerChallengeIssueDispatch::Processed
                                   : PeerChallengeIssueDispatch::Empty,
                      result);
        TEST_ASSERT_EQUAL(0, fixture.nonce.calls);
    }
    ChallengeIssueFixture fixture;
    fixture.write(testIdentity(9));
    peerOutcome(PeerProofOutcome::InvalidCredential, fixture.process().outcome);
    TEST_ASSERT_EQUAL(0, fixture.nonce.calls);
}
void test_issue_four_static_copies_one_current_operation_and_no_duplicate_nonce_refresh() {
    ChallengeIssueFixture fixture;
    for (unsigned n = 0; n < 5; ++n)
        fixture.write();
    const auto issuedAt = f->now;
    for (unsigned n = 0; n < 4; ++n) {
        const auto result = fixture.process();
        issueDispatch(PeerChallengeIssueDispatch::Processed, result);
        peerOutcome(n == 0 ? PeerProofOutcome::Issued : PeerProofOutcome::Consumed, result.outcome);
        ++f->now;
    }
    issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
    PeerChallengeSnapshot snapshot;
    TEST_ASSERT_TRUE(fixture.proof.challengeSnapshot(fixture.original, snapshot));
    TEST_ASSERT_EQUAL(issuedAt, snapshot.issuedAt);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_issue_wrong_worker_and_wrong_sdk_actor_cannot_consume_or_rebind() {
    ChallengeIssueFixture fixture;
    fixture.write();
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    issueDispatch(PeerChallengeIssueDispatch::Unavailable, fixture.issuer.service());
    TEST_ASSERT_FALSE(fixture.issuer.beginWorker());
    FreeRtosPeerChallengeIssueRuntime foreign(fixture.source, fixture.proof, fixture.grant,
                                              fixture.publication, fixture.clock);
    TEST_ASSERT_FALSE(foreign.beginWorker());
    fixture.worker();
    TEST_ASSERT_FALSE(foreign.beginWorker());
    peerOutcome(PeerProofOutcome::Issued, fixture.process().outcome);
    fixture.actor();
    sdk446::Request request = {};
    request.write.handle = 0x45;
    request.write.length = 18;
    std::array<std::uint8_t, 18> bytes;
    TEST_ASSERT_TRUE(encodePeerChallengeIssue(testIdentity(2), bytes));
    std::memcpy(request.write.value, bytes.data(), 18);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    host_sdk_source::registeredCallbacks.request(257, 88, 2, &request);
    issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
}
void test_issue_expiry_wrap_and_reconnect_drop_old_work_without_adopting_fresh_link() {
    ChallengeIssueFixture fixture;
    f->now = UINT32_MAX - 100;
    fixture.write();
    f->now += 1000;
    issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
    TEST_ASSERT_EQUAL(0, fixture.nonce.calls);
    fixture.write();
    fixture.close();
    fixture.original.connection = fixture.open();
    issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
    fixture.actor();
    fixture.pair();
    fixture.write();
    peerOutcome(PeerProofOutcome::Issued, fixture.process().outcome);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
}
void test_issue_sdk_borrow_and_original_scope_frozen_before_external_clock() {
    ChallengeIssueFixture fixture;
    fixture.actor();
    sdk446::Request request = {};
    request.write.handle = 0x45;
    request.write.length = 18;
    std::array<std::uint8_t, 18> bytes;
    TEST_ASSERT_TRUE(encodePeerChallengeIssue(testIdentity(2), bytes));
    std::memcpy(request.write.value, bytes.data(), 18);
    static sdk446::Request *borrowed;
    borrowed = &request;
    fixture.clock.before = []() { std::memset(borrowed, 0xaa, sizeof(*borrowed)); };
    host_sdk_source::registeredCallbacks.request(257, 55, 2, &request);
    peerOutcome(PeerProofOutcome::Issued, fixture.process().outcome);
    // A source clock callback closes/recycles the link after its original capture.
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    static ChallengeIssueFixture *active;
    active = &fixture;
    fixture.clock.before = []() {
        active->close();
        active->original.connection = active->open();
        active->actor();
    };
    fixture.write();
    issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
}
void test_issue_registry_revoke_entropy_failure_and_expired_challenge_do_not_retry() {
    {
        ChallengeIssueFixture fixture;
        fixture.write();
        fixture.io();
        registryOutcome(PeerRegistryOutcome::Pending,
                        fixture.owner.replaceTrusted(1, registryValue(2, false)));
        fixture.actor();
        host_sdk_source::drainOne();
        issueDispatch(PeerChallengeIssueDispatch::Empty, fixture.process());
        TEST_ASSERT_EQUAL(0, fixture.nonce.calls);
    }
    {
        ChallengeIssueFixture fixture;
        fixture.nonce.fail = true;
        fixture.write();
        peerOutcome(PeerProofOutcome::EntropyFailure, fixture.process().outcome);
        fixture.nonce.fail = false;
        fixture.write();
        peerOutcome(PeerProofOutcome::Consumed, fixture.process().outcome);
        TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    }
    {
        ChallengeIssueFixture fixture;
        fixture.write();
        peerOutcome(PeerProofOutcome::Issued, fixture.process().outcome);
        f->now += 30000;
        fixture.write();
        peerOutcome(PeerProofOutcome::Consumed, fixture.process().outcome);
        fixture.read();
        TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
        TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    }
}
struct IssueSetupModel : ChallengeSetupModel, IPeerChallengeIssueIngress {
    bool copyIssue(const PeerChallengeIssueRequest &) noexcept override {
        return false;
    }
};
void test_issue_optional_service_installs_nine_original_handles_before_start() {
    IssueSetupModel ports;
    PeerChallengeReadSetup setup;
    setup.boot = f->boot;
    setup.context = &ports;
    setup.ingress = &ports;
    setup.issue = &ports;
    GattsServiceFixture sdk(setup);
    sdk.pending();
    auto p = sdk.attributeParameter(true);
    p.add_char.attr_handle = 0x26;
    auto uuid = GattsServiceDefinition::challenge();
    std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuid.data(), 16);
    sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
    sdk.worker();
    sdk.phase(GattsServicePhase::Issue);
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
    TEST_ASSERT_EQUAL(9, host_service_sdk::calls.front().handles);
    const auto &issue = host_service_sdk::calls.back();
    TEST_ASSERT_EQUAL(64, issue.permission);
    TEST_ASSERT_EQUAL(4, issue.property);
    TEST_ASSERT_EQUAL(18, issue.maximum);
    TEST_ASSERT_TRUE(issue.uuid == GattsServiceDefinition::issue());
    p.add_char.attr_handle = 0x28;
    uuid = GattsServiceDefinition::issue();
    std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuid.data(), 16);
    sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
    sdk.worker();
    sdk.phase(GattsServicePhase::Installing);
    sdk.install();
    sdk.worker();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(0x28, sdk.owner.snapshot().setup.issue);
}
void test_issue_setup_rejects_incomplete_bundle_wrong_handle_order_and_late_configuration() {
    ChallengeIssueFixture fixture;
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.source.configureChallengeIssueNow(0x46, fixture.issuer));
    fixture.worker();
    TEST_ASSERT_FALSE(fixture.source.configureChallengeIssueNow(0x46, fixture.issuer));
    for (unsigned mode = 0; mode < 4; ++mode) {
        IssueSetupModel ports;
        GattsServiceFixture sdk;
        auto job = sdk.job();
        PeerChallengeReadSetup setup;
        job.challenge = 0x26;
        job.issue = 0x28;
        setup.boot = f->boot;
        setup.context = &ports;
        setup.ingress = &ports;
        setup.issue = &ports;
        if (mode == 0)
            setup.issue = nullptr;
        if (mode == 1)
            job.issue = 0x26;
        if (mode == 2)
            job.issue = 0x29;
        if (mode == 3)
            job.challenge = 0;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(GattsEndpointSetupPost::Rejected),
            static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses, setup)));
    }
}
#if defined(LARP_REAL_PEER_CRYPTO)
void test_issue_actual_crypto_original_sdk_write_to_read_and_existing_proof_grant() {
    Esp32PeerProofCrypto crypto;
    ChallengeIssueFixture fixture(&crypto, 0x01020304);
    fixture.write();
    peerOutcome(PeerProofOutcome::Issued, fixture.process().outcome);
    fixture.read();
    const auto read = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, read.response.value.value, 108);
    TEST_ASSERT_EQUAL(2, read.transaction);
    PeerProof proof;
    TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, 72, proof));
    peerOutcome(PeerProofOutcome::Verified, fixture.proof.verify(fixture.original, proof));
    grantState(PeerProofGrantState::Pending,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.worker();
    TrustedAuthority ack;
    grantState(PeerProofGrantState::Confirmed, fixture.grant.service(ack));
    TEST_ASSERT_TRUE(ack.gameMaster && ack.principal == 7);
    std::array<std::uint8_t, 18> request;
    TEST_ASSERT_TRUE(encodePeerChallengeIssue(testIdentity(2), request));
    const char *hex = "0123456789abcdef";
    auto hexString = [&](const std::uint8_t *bytes, std::size_t count) {
        std::string result;
        for (std::size_t i = 0; i < count; ++i) {
            result += hex[bytes[i] >> 4];
            result += hex[bytes[i] & 15];
        }
        return result;
    };
    const std::string observation =
        "{\n  \"scope\": \"original_sdk_issue_model_v1\",\n  \"issueHex\": \"" +
        hexString(fixture.recording.copied.bytes.data(), 18) + "\",\n  \"challengeHex\": \"" +
        hexString(read.response.value.value, read.response.value.length) +
        "\",\n  \"readTransaction\": " + std::to_string(read.transaction) +
        ",\n  \"readConnection\": " + std::to_string(read.connection) +
        ",\n  \"readAttribute\": " + std::to_string(read.response.value.handle) +
        ",\n  \"readStatus\": " + std::to_string(read.status) +
        ",\n  \"nonceCalls\": " + std::to_string(fixture.nonce.calls) +
        ",\n  \"actorAckPrincipal\": " + std::to_string(ack.principal) +
        ",\n  \"actorAckGameMaster\": true,\n  \"rfMeasured\": false\n}\n";
    const char *path = "docs/companion/fixtures/peer-challenge-issue/observation.json";
    if (std::getenv("LARP_EXPORT_PEER_CHALLENGE_ISSUE_FIXTURE")) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(observation.size(),
                          std::fwrite(observation.data(), 1, observation.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char bytes[2048];
    const auto count = std::fread(bytes, 1, sizeof(bytes), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(observation.c_str(), std::string(bytes, count).c_str());
}
#endif
