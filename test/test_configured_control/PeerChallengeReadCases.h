#pragma once
#include "PeerProofGrantCases.h"
void readDispatch(PeerChallengeReadDispatch expected, PeerChallengeReadDispatch actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
struct ChallengeReadFixture : SecurityFixture {
    FreeRtosPeerProofGrantRuntime grant{source, publication, clock, f->boot};
    PeerNonceModel nonce;
    PeerProofOwner proof;
    FreeRtosPeerChallengeReadRuntime reader{source, proof, grant, clock};
    std::uint32_t issuedAt = 0, transaction = 0;
    explicit ChallengeReadFixture(IPeerProofCrypto *actual = nullptr,
                                  std::uint32_t registration = 0)
        : SecurityFixture(false, true, actual, registration),
          proof(grant, actual ? *actual : crypto, nonce, clock) {
        worker();
        TEST_ASSERT_TRUE(grant.beginWorker() && reader.beginWorker());
        actor();
        TEST_ASSERT_TRUE(source.configurePeerProofGrantNow(f->boot, grant));
        TEST_ASSERT_TRUE(source.configureChallengeReadNow(0x44, reader));
        original.connection = open();
        actor();
        pair();
        worker();
    }
    void worker() {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    }
    void issue() {
        worker();
        PeerCredential credential;
        TEST_ASSERT_TRUE(publication.credential(testIdentity(2), credential));
        issuedAt = f->now;
        peerOutcome(PeerProofOutcome::Issued, proof.issue(original, credential));
    }
    void read(std::uint16_t offset = 0, bool isLong = false, bool need = true) {
        actor();
        sdk446::Request request = {};
        request.read.handle = 0x44;
        request.read.offset = offset;
        request.read.isLong = isLong;
        request.read.needResponse = need;
        host_sdk_source::registeredCallbacks.request(257, ++transaction, 1, &request);
        std::memset(&request, 0xaa, sizeof(request)); // Borrow lifetime ended.
        worker();
    }
    void send() {
        readDispatch(PeerChallengeReadDispatch::Posted, reader.service());
        actor();
        host_sdk_source::drainOne();
        worker();
    }
    PeerChallengeReadReply reply() {
        actor();
        PeerRegistryActorOrigin scope;
        TEST_ASSERT_TRUE(source.readPeerProofGrantOrigin(original, scope));
        PeerChallengeReadReply value;
        value.request.original = original;
        value.request.lifecycle = scope.lifecycle;
        value.request.address = {{1, 2, 3, 4, 5, 6}};
        value.request.attribute = 0x44;
        value.request.transaction = 999;
        value.request.receivedAt = f->now;
        worker();
        TEST_ASSERT_TRUE(proof.challengeSnapshot(original, value.snapshot));
        value.status = 0;
        return value;
    }
};
void test_challenge_read_original_blob_values_keep_one_nonce_and_no_gm() {
    ChallengeReadFixture fixture;
    fixture.issue();
    PeerTranscript expected;
    PeerChallenge challenge;
    TEST_ASSERT_TRUE(fixture.proof.challenge(fixture.original, challenge));
    TEST_ASSERT_TRUE(encodePeerChallenge(challenge, expected));
    for (unsigned offset = 0; offset < 108; offset += 22) {
        fixture.read(offset, offset != 0);
        fixture.send();
        const auto &read = host_sdk_source::readSent.back();
        TEST_ASSERT_EQUAL(257, read.connection);
        TEST_ASSERT_EQUAL(offset / 22 + 1, read.transaction);
        TEST_ASSERT_EQUAL(0, read.status);
        TEST_ASSERT_EQUAL(0x44, read.response.value.handle);
        TEST_ASSERT_EQUAL(offset, read.response.value.offset);
        TEST_ASSERT_EQUAL(108 - offset, read.response.value.length);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data() + offset, read.response.value.value,
                                     108 - offset);
    }
    fixture.read(108, true);
    fixture.send();
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_challenge_read_missing_challenge_and_bad_offset_have_bounded_att_errors() {
    ChallengeReadFixture fixture;
    fixture.read();
    fixture.send();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    fixture.issue();
    fixture.read(1);
    fixture.send();
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    fixture.read(109, true);
    fixture.send();
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
}
void test_challenge_read_wrong_worker_no_response_unknown_reads_forward_and_no_rsp_is_dropped() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.read();
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    readDispatch(PeerChallengeReadDispatch::Unavailable, fixture.reader.service());
    TEST_ASSERT_FALSE(fixture.reader.beginWorker());
    fixture.worker();
    fixture.send();
    fixture.read(0, false, false);
    readDispatch(PeerChallengeReadDispatch::Empty, fixture.reader.service());
    fixture.actor();
    sdk446::Request request = {};
    request.read.handle = 0x45;
    host_sdk_source::registeredCallbacks.request(257, 9, 1, &request);
    TEST_ASSERT_EQUAL(1, host_sdk_source::passedRequests);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    request.read.handle = 0x44;
    request.read.needResponse = true;
    host_sdk_source::registeredCallbacks.request(257, 10, 1, &request);
    fixture.worker();
    readDispatch(PeerChallengeReadDispatch::Empty, fixture.reader.service());
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
}
void test_challenge_read_queue_is_four_static_entries_one_response_per_service() {
    ChallengeReadFixture fixture;
    fixture.issue();
    for (unsigned i = 0; i < 5; ++i)
        fixture.read();
    for (unsigned i = 0; i < 4; ++i)
        fixture.send();
    readDispatch(PeerChallengeReadDispatch::Empty, fixture.reader.service());
    TEST_ASSERT_EQUAL(4, host_sdk_source::readSent.size());
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_challenge_read_queue_deadline_wrap_never_refreshes_and_stale_drain_is_bounded() {
    ChallengeReadFixture fixture;
    f->now = UINT32_MAX - 100;
    fixture.issue();
    for (unsigned i = 0; i < 4; ++i)
        fixture.read();
    f->now += 1000;
    readDispatch(PeerChallengeReadDispatch::Empty, fixture.reader.service());
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
    fixture.read();
    f->now += 999;
    fixture.send();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
}
void test_challenge_read_post_deadline_and_absolute_issue_deadline_drop_sdk_output() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        ChallengeReadFixture fixture;
        fixture.issue();
        if (variant == 1)
            f->now += 29999;
        fixture.read();
        readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
        f->now += variant == 0 ? 1000 : variant == 1 ? 1 : 999;
        fixture.actor();
        host_sdk_source::drainOne();
        TEST_ASSERT_EQUAL(variant == 2 ? 1 : 0, host_sdk_source::readSent.size());
        fixture.worker();
    }
}
void test_challenge_read_reconnect_and_old_opaque_callback_cannot_consume_new_slot() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.read();
    readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
    const auto replay = host_sdk_source::work.front();
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
    fixture.issue();
    fixture.read();
    readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
    fixture.actor();
    replay.function(replay.context);
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
    TEST_ASSERT_EQUAL(2, fixture.nonce.calls);
}
void test_challenge_read_sdk_metadata_loss_never_emits_positive_data() {
    for (unsigned variant = 0; variant < 5; ++variant) {
        ChallengeReadFixture fixture;
        fixture.issue();
        fixture.read();
        readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
        if (variant == 0)
            host_sdk_security::flags = 0;
        if (variant == 1)
            host_sdk_security::running = true;
        if (variant == 2)
            host_sdk_security::address[0]++;
        if (variant == 3)
            btu_thread = reinterpret_cast<void *>(99);
        if (variant == 4) {
            fixture.actor();
            fixture.event(2);
        }
        fixture.actor();
        host_sdk_source::drainOne();
        TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
        btu_thread = reinterpret_cast<void *>(2);
        fixture.worker();
    }
}
void test_challenge_read_registry_revoke_while_pending_drops_original_answer() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.read();
    readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
}
void test_challenge_read_post_failure_and_busy_slot_have_no_retry_or_duplicate_posts() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.read();
    host_sdk_source::rejectPost = true;
    readDispatch(PeerChallengeReadDispatch::Rejected, fixture.reader.service());
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
    host_sdk_source::rejectPost = false;
    fixture.read();
    readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
    fixture.read();
    const auto posts = host_sdk_source::posts;
    readDispatch(PeerChallengeReadDispatch::Rejected, fixture.reader.service());
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
}
void test_challenge_read_frozen_reply_and_invalid_scope_never_changes_original() {
    ChallengeReadFixture fixture;
    fixture.issue();
    for (unsigned variant = 0; variant < 7; ++variant) {
        auto value = fixture.reply();
        if (variant == 0)
            value.request.original.boot.low++;
        if (variant == 1)
            value.request.original.connection.token++;
        if (variant == 2)
            value.request.lifecycle++;
        if (variant == 3)
            value.snapshot.challenge.original.registration++;
        if (variant == 4)
            value.snapshot.credential.generation++;
        if (variant == 5)
            value.snapshot.challenge.peer = testIdentity(3);
        if (variant == 6)
            value.status = 255;
        TEST_ASSERT_FALSE(fixture.source.postChallengeReadReply(value));
    }
    auto value = fixture.reply();
    static PeerChallengeReadReply *borrowed;
    borrowed = &value;
    fixture.clock.before = []() {
        borrowed->request.transaction = 123;
        borrowed->snapshot.challenge.nonce.fill(0);
    };
    TEST_ASSERT_TRUE(fixture.source.postChallengeReadReply(value));
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(999, host_sdk_source::readSent.back().transaction);
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.back().response.value.value[76]);
}
void test_challenge_read_request_frozen_before_external_clock_and_late_security_recheck() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.actor();
    sdk446::Request request = {};
    request.read = {0x44, 0, false, true};
    static sdk446::Request *borrowed;
    borrowed = &request;
    fixture.clock.before = []() {
        borrowed->read.handle = 0x45;
        borrowed->read.offset = 99;
        borrowed->read.needResponse = false;
    };
    host_sdk_source::registeredCallbacks.request(257, 77, 1, &request);
    fixture.worker();
    fixture.send();
    TEST_ASSERT_EQUAL(77, host_sdk_source::readSent.back().transaction);
    TEST_ASSERT_EQUAL(108, host_sdk_source::readSent.back().response.value.length);
    fixture.read();
    readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
    static ChallengeReadFixture *active;
    active = &fixture;
    host_sdk_security::beforeConnection = []() {
        active->close();
        active->actor();
    };
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
}
struct ChallengeSetupModel : IPeerChallengeReadIngress, IPeerProofContext {
    bool copyRead(const PeerChallengeReadRequest &) noexcept override {
        return false;
    }
    bool workerNow() const noexcept override {
        return false;
    }
    bool originalLive(const PeerProofBinding &) const noexcept override {
        return false;
    }
    bool credentialCurrent(const PeerCredential &) const noexcept override {
        return false;
    }
};
void test_challenge_read_service_installs_original_read_attribute_before_start() {
    ChallengeSetupModel ports;
    PeerChallengeReadSetup setup;
    setup.boot = f->boot;
    setup.context = &ports;
    setup.ingress = &ports;
    GattsServiceFixture sdk(setup);
    sdk.pending();
    sdk.phase(GattsServicePhase::Challenge);
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
    auto p = sdk.attributeParameter(true);
    p.add_char.attr_handle = 0x26;
    const auto uuid = GattsServiceDefinition::challenge();
    std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuid.data(), uuid.size());
    sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
    sdk.worker();
    sdk.phase(GattsServicePhase::Installing);
    TEST_ASSERT_EQUAL(7, host_service_sdk::calls.front().handles);
    const auto &read = host_service_sdk::calls.back();
    TEST_ASSERT_EQUAL(4, read.permission);
    TEST_ASSERT_EQUAL(2, read.property);
    TEST_ASSERT_EQUAL(128, read.maximum);
    TEST_ASSERT_TRUE(read.uuid == uuid);
    sdk.install();
    sdk.worker();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(0x26, sdk.owner.snapshot().setup.challenge);
}
void test_challenge_read_nonce_snapshot_failure_preserves_destination_and_deadline() {
    ChallengeReadFixture fixture;
    fixture.issue();
    auto value = fixture.reply();
    const auto issued = value.snapshot.issuedAt;
    f->now += 29999;
    PeerChallengeSnapshot out;
    TEST_ASSERT_TRUE(fixture.proof.challengeSnapshot(fixture.original, out));
    TEST_ASSERT_EQUAL(issued, out.issuedAt);
    ++f->now;
    out.issuedAt = 99;
    TEST_ASSERT_FALSE(fixture.proof.challengeSnapshot(fixture.original, out));
    TEST_ASSERT_EQUAL(99, out.issuedAt);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
}

void test_challenge_read_exhausted_callback_counter_and_foreign_caller_cannot_post() {
    ChallengeReadFixture fixture;
    fixture.issue();
    auto value = fixture.reply();
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    TEST_ASSERT_FALSE(fixture.source.postChallengeReadReply(value));
    fixture.worker();
    const auto saved = larp::companion::challengeReadPostCounter;
    larp::companion::challengeReadPostCounter = UINT32_MAX; // Test-only exhausted boot.
    const auto posts = host_sdk_source::posts;
    TEST_ASSERT_FALSE(fixture.source.postChallengeReadReply(value));
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    larp::companion::challengeReadPostCounter = saved;
}
void test_challenge_read_revoke_or_deadline_during_sdk_metadata_never_sends() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        ChallengeReadFixture fixture;
        fixture.issue();
        fixture.read();
        readDispatch(PeerChallengeReadDispatch::Posted, fixture.reader.service());
        static ChallengeReadFixture *active;
        static unsigned mode;
        active = &fixture;
        mode = variant;
        host_sdk_security::beforeConnection = []() {
            if (mode == 0)
                f->now += 1000;
            if (mode == 1)
                f->now += 30000;
            if (mode == 2) {
                active->io();
                registryOutcome(PeerRegistryOutcome::Pending,
                                active->owner.replaceTrusted(1, registryValue(2, false)));
                active->actor();
            }
        };
        fixture.actor();
        host_sdk_source::drainOne();
        TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
        fixture.worker();
    }
}
void test_challenge_read_borrowed_clock_closure_cannot_adopt_recycled_link() {
    ChallengeReadFixture fixture;
    fixture.issue();
    fixture.actor();
    static ChallengeReadFixture *active;
    active = &fixture;
    fixture.clock.before = []() {
        active->close();
        active->original.connection = active->open();
        active->actor();
    };
    sdk446::Request read = {};
    read.read = {0x44, 0, false, true};
    host_sdk_source::registeredCallbacks.request(257, 1, 1, &read);
    fixture.worker();
    readDispatch(PeerChallengeReadDispatch::Empty, fixture.reader.service());
    TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
}
void test_challenge_read_invalid_installation_bundle_and_order_fail_before_sdk_start() {
    for (unsigned variant = 0; variant < 4; ++variant) {
        ChallengeSetupModel ports;
        PeerChallengeReadSetup setup;
        setup.boot = f->boot;
        setup.context = &ports;
        setup.ingress = &ports;
        GattsServiceFixture sdk(setup);
        sdk.pending();
        auto p = sdk.attributeParameter(true);
        p.add_char.attr_handle = 0x26;
        const auto uuid = GattsServiceDefinition::challenge();
        std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuid.data(), uuid.size());
        if (variant == 0)
            p.add_char.attr_handle = 0x24;
        if (variant == 1)
            p.add_char.attr_handle = 0x27;
        if (variant == 2)
            p.add_char.char_uuid.uuid.uuid128[0]++;
        if (variant == 3)
            p.add_char.status = 1;
        sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_TRUE(host_sdk_source::work.empty());
        TEST_ASSERT_FALSE(sdk.owner.ready());
    }
    for (unsigned variant = 0; variant < 3; ++variant) {
        ChallengeSetupModel ports;
        PeerChallengeReadSetup setup;
        setup.boot = f->boot;
        setup.context = &ports;
        setup.ingress = &ports;
        if (variant == 0)
            setup.boot = {};
        if (variant == 1)
            setup.context = nullptr;
        if (variant == 2)
            setup.ingress = nullptr;
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
        GattsServiceFixture base;
        Esp32GattsService invalid(base.source, base.responses, base.clock, base.driver, setup);
        TEST_ASSERT_FALSE(invalid.begin());
        TEST_ASSERT_FALSE(invalid.begin());
        TEST_ASSERT_FALSE(invalid.ready());
    }
}
void test_challenge_read_requires_the_same_existing_crypto_worker_before_any_owner_getter() {
    ChallengeReadFixture fixture;
    FreeRtosPeerChallengeReadRuntime foreign(fixture.source, fixture.proof, fixture.grant,
                                             fixture.clock);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    TEST_ASSERT_FALSE(foreign.beginWorker());
    TEST_ASSERT_FALSE(foreign.workerNow());
    readDispatch(PeerChallengeReadDispatch::Unavailable, foreign.service());
    fixture.worker();
    TEST_ASSERT_FALSE(foreign.beginWorker()); // No recovery after failed one-time binding.
    fixture.issue();
    fixture.read();
    fixture.send();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
}
#if defined(LARP_REAL_PEER_CRYPTO)
void test_challenge_read_actual_crypto_from_original_sdk_blob_snapshot_to_grant_ack() {
    Esp32PeerProofCrypto crypto;
    ChallengeReadFixture fixture(&crypto, 0x01020304);
    fixture.issue();
    std::string observation =
        "{\n  \"scope\": \"actual_crypto_original_sdk_read_model_v1\",\n  \"mtuModel\": 23,\n  "
        "\"queueMilliseconds\": 1000,\n  \"proofMilliseconds\": 30000,\n  \"sdkReadCallbackType\": "
        "1,\n  \"attribute\": 68,\n  \"reads\": [\n";
    const char *hex = "0123456789abcdef";
    for (unsigned offset = 0; offset < 108; offset += 22) {
        fixture.read(offset, offset != 0);
        fixture.send();
        const auto &read = host_sdk_source::readSent.back();
        TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript + offset, read.response.value.value,
                                     108 - offset);
        TEST_ASSERT_EQUAL(0, read.status);
        observation += "    {\"packedConnection\": " + std::to_string(read.connection) +
                       ", \"transaction\": " + std::to_string(read.transaction) +
                       ", \"offset\": " + std::to_string(read.response.value.offset) +
                       ", \"sdkBytes\": " + std::to_string(read.response.value.length) +
                       ", \"remainingHex\": \"";
        for (unsigned i = 0; i < read.response.value.length; ++i) {
            const auto byte = read.response.value.value[i];
            observation += hex[byte >> 4];
            observation += hex[byte & 15];
        }
        observation += "\"}";
        observation += offset + 22 < 108 ? ",\n" : "\n";
    }
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    fixture.worker();
    PeerProof value;
    TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, 72, value));
    peerOutcome(PeerProofOutcome::Verified, fixture.proof.verify(fixture.original, value));
    grantState(PeerProofGrantState::Pending,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.worker();
    grantState(PeerProofGrantState::Confirmed, fixture.grant.service(route));
    TEST_ASSERT_TRUE(route.gameMaster && route.principal == 7);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    observation += "  ],\n  \"nonceCalls\": 1,\n  \"actualSignatureVerified\": true,\n  "
                   "\"actorAckGameMaster\": true,\n  \"actorAckPrincipal\": 7,\n  "
                   "\"peerDelivered\": false,\n  \"rfMeasured\": false\n}\n";
    const char *path = "docs/companion/fixtures/peer-challenge-read/observation.json";
    if (std::getenv("LARP_EXPORT_PEER_CHALLENGE_READ_FIXTURE")) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(observation.size(),
                          std::fwrite(observation.data(), 1, observation.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char bytes[4096];
    const auto count = std::fread(bytes, 1, sizeof(bytes), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(observation.c_str(), std::string(bytes, count).c_str());
}
#endif
