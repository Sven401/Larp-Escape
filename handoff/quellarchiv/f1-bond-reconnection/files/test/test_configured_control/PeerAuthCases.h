#pragma once
#include "PeerChallengeIssueCases.h"
void authDispatch(PeerAuthDispatch expected, PeerAuthDispatch actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
struct AuthFixture : ChallengeIssueFixture {
    explicit AuthFixture(IPeerProofCrypto *crypto = nullptr, std::uint32_t reg = 0, bool gm = true)
        : ChallengeIssueFixture(crypto, reg, true, gm) {}
    void issueNow() {
        write();
        peerOutcome(PeerProofOutcome::Issued, process().outcome);
    }
    std::string frame(unsigned offset, unsigned count = 12, std::uint16_t message = 9) {
        char bytes[128];
        std::size_t written = 0;
        TEST_ASSERT_TRUE(
            encodePeerProofFrame(message, 72, static_cast<std::uint16_t>(offset),
                                 reinterpret_cast<const char *>(peerTestProofBytes) + offset, count,
                                 bytes, sizeof(bytes), written));
        return {bytes, written};
    }
    void proofWrite(unsigned offset, unsigned count = 12, std::uint16_t message = 9,
                    unsigned variant = 0) {
        actor();
        auto bytes = frame(offset, count, message);
        sdk446::Request request = {};
        request.write.handle = variant == 1 ? 0x48 : 0x46;
        request.write.length = bytes.size();
        request.write.offset = variant == 2 ? 1 : 0;
        request.write.prepared = variant == 3;
        request.write.needResponse = variant == 4;
        std::memcpy(request.write.value, bytes.data(), bytes.size());
        if (variant == 5)
            request.write.value[0] = 2;
        host_sdk_source::registeredCallbacks.request(variant == 6 ? 513 : 257, ++transaction, 2,
                                                     &request);
        std::memset(&request, 0xaa, sizeof(request));
        worker();
    }
    void complete() {
        for (unsigned offset = 0; offset < 72; offset += 12) {
            proofWrite(offset);
            authDispatch(PeerAuthDispatch::ProofProcessed, auth.service());
        }
    }
    void confirm() {
        actor();
        host_sdk_source::drainOne();
        worker();
        authDispatch(PeerAuthDispatch::GrantObserved, auth.service());
    }
    void resultRead(std::uint16_t offset = 0, bool isLong = false, bool need = true) {
        actor();
        sdk446::Request request = {};
        request.read = {0x47, offset, isLong, need};
        host_sdk_source::registeredCallbacks.request(257, ++transaction, 1, &request);
        std::memset(&request, 0xaa, sizeof(request));
        worker();
    }
    void sendResult() {
        authDispatch(PeerAuthDispatch::ReadPosted, auth.service());
        actor();
        host_sdk_source::drainOne();
        worker();
    }
    PeerAuthResult result() {
        const auto &read = host_sdk_source::readSent.back();
        TEST_ASSERT_EQUAL(0, read.status);
        PeerAuthResult result;
        TEST_ASSERT_TRUE(
            decodePeerAuthResult(read.response.value.value, read.response.value.length, result));
        return result;
    }
};
void test_auth_codec_exact_states_correlations_and_failed_destination() {
    PeerAuthResult value;
    value.message = 9;
    value.sequence = 1;
    value.peer = testIdentity(2);
    std::array<std::uint8_t, 32> bytes;
    for (unsigned state = 1; state <= 5; ++state) {
        value.state = static_cast<PeerAuthState>(state);
        value.received = state == 1 ? 12 : 72;
        value.reason = state == 2 ? PeerAuthReason::ProofRejected : PeerAuthReason::None;
        value.principal = state == 5 ? 7 : 0;
        TEST_ASSERT_TRUE(encodePeerAuthResult(value, bytes));
        PeerAuthResult out;
        TEST_ASSERT_TRUE(decodePeerAuthResult(bytes.data(), 32, out));
        TEST_ASSERT_EQUAL(state, bytes[2]);
        TEST_ASSERT_EQUAL(9, out.message);
        TEST_ASSERT_EQUAL(1, out.sequence);
        TEST_ASSERT_TRUE(out.peer == testIdentity(2));
    }
    PeerAuthResult out;
    out.message = 99;
    for (unsigned length = 0; length < 35; ++length) {
        if (length == 32)
            continue;
        TEST_ASSERT_FALSE(decodePeerAuthResult(bytes.data(), length, out));
        TEST_ASSERT_EQUAL(99, out.message);
    }
    for (unsigned mode = 0; mode < 9; ++mode) {
        auto bad = bytes;
        if (mode == 0)
            bad[0] = 2;
        if (mode == 1)
            bad[1] = 10;
        if (mode == 2)
            bad[2] = 0;
        if (mode == 3)
            bad[3] = 1;
        if (mode == 4)
            bad[4] = 0;
        if (mode == 5)
            bad[6] = 73;
        if (mode == 6)
            bad[8] = 0;
        if (mode == 7)
            bad[12] = 0;
        if (mode == 8)
            std::fill(bad.begin() + 16, bad.end(), 0);
        TEST_ASSERT_FALSE(decodePeerAuthResult(bad.data(), 32, out));
        TEST_ASSERT_EQUAL(99, out.message);
    }
    auto saved = bytes;
    value.message = 0;
    TEST_ASSERT_FALSE(encodePeerAuthResult(value, bytes));
    TEST_ASSERT_TRUE(bytes == saved);
}
void test_auth_sdk_fragments_pending_ack_and_correlated_granted_result() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.complete();
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    fixture.resultRead();
    authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
    fixture.actor();
    host_sdk_source::drainOne();
    host_sdk_source::drainOne();
    fixture.worker();
    auto result = fixture.result();
    TEST_ASSERT_EQUAL(3, static_cast<unsigned>(result.state));
    TEST_ASSERT_EQUAL(0, result.principal);
    authDispatch(PeerAuthDispatch::GrantObserved, fixture.auth.service());
    fixture.resultRead();
    fixture.sendResult();
    result = fixture.result();
    TEST_ASSERT_EQUAL(5, static_cast<unsigned>(result.state));
    TEST_ASSERT_EQUAL(7, result.principal);
    TEST_ASSERT_EQUAL(9, result.message);
    TEST_ASSERT_EQUAL(72, result.received);
    TEST_ASSERT_EQUAL(1, result.sequence);
    TEST_ASSERT_TRUE(result.peer == testIdentity(2));
    TEST_ASSERT_EQUAL(6, fixture.authRecording.proofCopies);
    TEST_ASSERT_TRUE(fixture.authRecording.copied.scope.original == fixture.original);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_auth_result_blob_pins_receiving_snapshot_across_grant_and_new_read_gets_latest() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.proofWrite(0);
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    fixture.resultRead();
    fixture.sendResult();
    const auto before = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(1, static_cast<unsigned>(fixture.result().state));
    for (unsigned offset = 12; offset < 72; offset += 12) {
        fixture.proofWrite(offset);
        authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    }
    fixture.confirm();
    fixture.resultRead(22, true);
    fixture.sendResult();
    const auto &blob = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(10, blob.response.value.length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(before.response.value.value + 22, blob.response.value.value, 10);
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(5, static_cast<unsigned>(fixture.result().state));
}
void test_auth_result_missing_bad_offsets_pin_expiry_and_absolute_challenge_deadline() {
    AuthFixture fixture;
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    fixture.issueNow();
    fixture.proofWrite(0);
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    fixture.resultRead(1);
    fixture.sendResult();
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    fixture.resultRead(33, true);
    fixture.sendResult();
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    fixture.resultRead(22, true);
    fixture.sendResult();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    fixture.resultRead();
    fixture.sendResult();
    f->now += 1000;
    fixture.resultRead(22, true);
    fixture.sendResult();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    fixture.resultRead();
    fixture.sendResult();
    f->now += 30000;
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
}
void test_auth_signature_rejection_is_visible_without_grant_or_nonce_retry() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.crypto.accepted = false;
    fixture.complete();
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
    fixture.resultRead();
    fixture.sendResult();
    const auto result = fixture.result();
    TEST_ASSERT_EQUAL(2, static_cast<unsigned>(result.state));
    TEST_ASSERT_EQUAL(2, static_cast<unsigned>(result.reason));
    TEST_ASSERT_EQUAL(0, result.principal);
    TEST_ASSERT_EQUAL(1, fixture.nonce.calls);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
}
void test_auth_invalid_write_modes_frames_foreign_task_and_first_bad_worker_binding() {
    AuthFixture fixture;
    fixture.issueNow();
    for (unsigned variant = 1; variant <= 6; ++variant) {
        fixture.proofWrite(0, 12, 9, variant);
        authDispatch(variant == 5 ? PeerAuthDispatch::Rejected : PeerAuthDispatch::Empty,
                     fixture.auth.service());
    }
    fixture.proofWrite(0);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    authDispatch(PeerAuthDispatch::Unavailable, fixture.auth.service());
    TEST_ASSERT_FALSE(fixture.auth.beginWorker());
    FreeRtosPeerAuthRuntime foreign(fixture.source, fixture.proof, fixture.authReceiver,
                                    fixture.grant, fixture.clock);
    TEST_ASSERT_FALSE(foreign.beginWorker());
    fixture.worker();
    TEST_ASSERT_FALSE(foreign.beginWorker());
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
}
void test_auth_four_proof_copies_and_reads_full_and_one_item_per_service() {
    AuthFixture fixture;
    fixture.issueNow();
    for (unsigned n = 0; n < 5; ++n)
        fixture.proofWrite(0);
    for (unsigned n = 0; n < 4; ++n)
        authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    authDispatch(PeerAuthDispatch::Empty, fixture.auth.service());
    for (unsigned n = 0; n < 5; ++n)
        fixture.resultRead();
    for (unsigned n = 0; n < 4; ++n)
        fixture.sendResult();
    authDispatch(PeerAuthDispatch::Empty, fixture.auth.service());
    TEST_ASSERT_EQUAL(4, host_sdk_source::readSent.size());
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_auth_proof_and_read_age_wrap_and_reconnect_never_rebind_old_work() {
    AuthFixture fixture;
    f->now = UINT32_MAX - 100;
    fixture.issueNow();
    fixture.proofWrite(0);
    f->now += 1000;
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<unsigned>(PeerAuthDispatch::Empty),
                              static_cast<unsigned>(fixture.auth.service()),
                              "expired proof across wrap");
    fixture.proofWrite(0);
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.worker();
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<unsigned>(PeerAuthDispatch::Empty),
                              static_cast<unsigned>(fixture.auth.service()),
                              "proof from old connection");
    fixture.actor();
    fixture.pair();
    fixture.issueNow();
    fixture.proofWrite(0);
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    fixture.resultRead();
    f->now += 1000;
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<unsigned>(PeerAuthDispatch::Empty),
                              static_cast<unsigned>(fixture.auth.service()), "expired result read");
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(2, fixture.result().sequence);
}
void test_auth_sdk_borrows_frozen_before_clock_and_clock_reconnect_drops_original() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.actor();
    sdk446::Request request = {};
    const auto frame = fixture.frame(0);
    request.write.handle = 0x46;
    request.write.length = frame.size();
    std::memcpy(request.write.value, frame.data(), frame.size());
    static sdk446::Request *borrowed;
    borrowed = &request;
    fixture.clock.before = []() { std::memset(borrowed, 0xaa, sizeof(*borrowed)); };
    host_sdk_source::registeredCallbacks.request(257, 33, 2, &request);
    fixture.worker();
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    TEST_ASSERT_EQUAL(33, fixture.authRecording.copied.scope.transaction);
    fixture.actor();
    request = {};
    request.read = {0x47, 0, false, true};
    borrowed = &request;
    fixture.clock.before = []() { std::memset(borrowed, 0xaa, sizeof(*borrowed)); };
    host_sdk_source::registeredCallbacks.request(257, 34, 1, &request);
    fixture.worker();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(34, host_sdk_source::readSent.back().transaction);
    static AuthFixture *active;
    active = &fixture;
    fixture.clock.before = []() {
        active->close();
        active->original.connection = active->open();
        active->actor();
    };
    fixture.proofWrite(12);
    authDispatch(PeerAuthDispatch::Empty, fixture.auth.service());
}
void test_auth_post_failure_busy_slot_and_old_callback_cannot_send_new_reply() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.proofWrite(0);
    authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    fixture.resultRead();
    host_sdk_source::rejectPost = true;
    authDispatch(PeerAuthDispatch::Rejected, fixture.auth.service());
    host_sdk_source::rejectPost = false;
    fixture.resultRead();
    authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
    const auto old = host_sdk_source::work.front();
    fixture.resultRead();
    authDispatch(PeerAuthDispatch::Rejected, fixture.auth.service());
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.worker();
    fixture.resultRead();
    authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
    fixture.actor();
    old.function(old.context);
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(2, host_sdk_source::readSent.size());
}
void test_auth_positive_reply_rechecks_sdk_security_credential_and_original_connection() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        AuthFixture fixture;
        fixture.issueNow();
        fixture.complete();
        fixture.confirm();
        fixture.resultRead();
        authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
        if (mode == 0)
            host_sdk_security::flags = 0;
        if (mode == 1) {
            fixture.close();
            fixture.original.connection = fixture.open();
        }
        if (mode == 2) {
            fixture.io();
            registryOutcome(PeerRegistryOutcome::Pending,
                            fixture.owner.replaceTrusted(1, registryValue(2, false)));
        }
        if (mode == 3)
            f->now += 1000;
        fixture.actor();
        host_sdk_source::drainOne();
        TEST_ASSERT_TRUE(host_sdk_source::readSent.empty());
    }
}
void test_auth_duplicate_terminal_proof_never_reposts_grant_and_expiry_does_not_revoke() {
    AuthFixture fixture;
    fixture.issueNow();
    fixture.complete();
    fixture.confirm();
    const auto posts = host_sdk_source::posts;
    for (unsigned n = 0; n < 3; ++n) {
        fixture.proofWrite(0, 12, 10 + n);
        authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    }
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(9, fixture.result().message);
    f->now += 30000;
    fixture.resultRead();
    fixture.sendResult();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, route));
    TEST_ASSERT_TRUE(route.gameMaster);
}
void test_auth_non_gm_ack_has_authenticated_result_without_principal() {
    AuthFixture fixture(nullptr, 0, false);
    fixture.issueNow();
    fixture.complete();
    fixture.confirm();
    fixture.resultRead();
    fixture.sendResult();
    const auto result = fixture.result();
    TEST_ASSERT_EQUAL(4, static_cast<unsigned>(result.state));
    TEST_ASSERT_EQUAL(0, result.principal);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
}
void test_auth_forged_positive_job_ack_principal_and_scope_never_post() {
    AuthFixture fixture;
    fixture.issueNow();
    PeerChallengeSnapshot snapshot;
    TEST_ASSERT_TRUE(fixture.proof.challengeSnapshot(fixture.original, snapshot));
    fixture.complete();
    fixture.confirm();
    fixture.resultRead();
    PeerAuthReadReply good;
    good.request = fixture.authRecording.readCopied;
    good.snapshot = snapshot;
    good.frozenAt = f->now;
    good.status = 0;
    good.result.state = PeerAuthState::Granted;
    good.result.reason = PeerAuthReason::None;
    good.result.message = 9;
    good.result.sequence = snapshot.challenge.sequence;
    good.result.peer = snapshot.credential.peer;
    good.result.received = 72;
    good.result.principal = 7;
    TEST_ASSERT_TRUE(fixture.grant.job(good.job));
    grantState(PeerProofGrantState::Confirmed, fixture.grant.service(good.ack));
    for (unsigned mode = 0; mode < 7; ++mode) {
        auto bad = good;
        if (mode == 0)
            ++bad.result.principal;
        if (mode == 1)
            ++bad.ack.epoch;
        if (mode == 2)
            ++bad.job.proof.sequence;
        if (mode == 3)
            ++bad.snapshot.issuedAt;
        if (mode == 4)
            ++bad.request.original.connection.token;
        if (mode == 5)
            ++bad.result.sequence;
        if (mode == 6) {
            bad.result.state = PeerAuthState::Authenticated;
            bad.result.principal = 0;
        }
        TEST_ASSERT_FALSE(fixture.source.postPeerAuthReply(bad));
    }
    TEST_ASSERT_TRUE(fixture.source.postPeerAuthReply(good));
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
    TEST_ASSERT_EQUAL(7, fixture.result().principal);
}
void test_auth_authority_retirement_at_every_reply_clock_observation_drops_old_success() {
    static AuthFixture *active;
    static unsigned observed, retireAt;
    static void (*hook)();
    hook = []() {
        ++observed;
        if (retireAt && observed == retireAt) {
            TrustedAuthority closed;
            TEST_ASSERT_EQUAL(
                static_cast<unsigned>(AuthorityUpdate::Published),
                static_cast<unsigned>(f->channel.actual.advanceAuthority(0, false, closed)));
        } else
            active->clock.before = hook;
    };
    unsigned observations = 0;
    {
        AuthFixture fixture;
        fixture.issueNow();
        fixture.complete();
        fixture.confirm();
        fixture.resultRead();
        authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
        active = &fixture;
        observed = retireAt = 0;
        fixture.actor();
        fixture.clock.before = hook;
        host_sdk_source::drainOne();
        fixture.clock.before = nullptr;
        observations = observed;
        TEST_ASSERT_TRUE(observations > 0);
        TEST_ASSERT_EQUAL(1, host_sdk_source::readSent.size());
    }
    for (unsigned n = 1; n <= observations; ++n) {
        AuthFixture fixture;
        fixture.issueNow();
        fixture.complete();
        fixture.confirm();
        fixture.resultRead();
        authDispatch(PeerAuthDispatch::ReadPosted, fixture.auth.service());
        active = &fixture;
        observed = 0;
        retireAt = n;
        fixture.actor();
        fixture.clock.before = hook;
        host_sdk_source::drainOne();
        fixture.clock.before = nullptr;
        TEST_ASSERT_EQUAL_MESSAGE(0, host_sdk_source::readSent.size(), std::to_string(n).c_str());
    }
}
struct AuthSetupModel : IssueSetupModel, IPeerAuthIngress {
    bool copyProof(const PeerProofWriteRequest &) noexcept override {
        return false;
    }
    bool copyResultRead(const PeerChallengeReadRequest &) noexcept override {
        return false;
    }
    bool workerNow() const noexcept override {
        return false;
    }
};
void test_auth_service_thirteen_handles_and_complete_source_ack_before_start() {
    AuthSetupModel ports;
    PeerChallengeReadSetup setup;
    setup.boot = f->boot;
    setup.context = &ports;
    setup.ingress = &ports;
    setup.issue = &ports;
    setup.auth = &ports;
    GattsServiceFixture sdk(setup);
    sdk.pending();
    const std::array<std::array<std::uint8_t, 16>, 4> uuids = {
        {GattsServiceDefinition::challenge(), GattsServiceDefinition::issue(),
         GattsServiceDefinition::proof(), GattsServiceDefinition::result()}};
    for (unsigned index = 0; index < 4; ++index) {
        auto p = sdk.attributeParameter(true);
        p.add_char.attr_handle = 0x26 + index * 2;
        std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuids[index].data(), 16);
        sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
        sdk.worker();
    }
    sdk.phase(GattsServicePhase::Installing);
    TEST_ASSERT_EQUAL(13, host_service_sdk::calls.front().handles);
    const auto &proof = host_service_sdk::calls[5];
    const auto &result = host_service_sdk::calls[6];
    TEST_ASSERT_EQUAL(4, proof.property);
    TEST_ASSERT_EQUAL(64, proof.permission);
    TEST_ASSERT_EQUAL(128, proof.maximum);
    TEST_ASSERT_EQUAL(2, result.property);
    TEST_ASSERT_EQUAL(4, result.permission);
    TEST_ASSERT_EQUAL(32, result.maximum);
    sdk.install();
    sdk.worker();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(0x2c, sdk.owner.snapshot().setup.result);
}
void test_auth_setup_wrong_handles_incomplete_bundle_and_late_configuration_fail_closed() {
    AuthFixture fixture;
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.source.configurePeerAuthNow(0x48, 0x49, fixture.auth));
    for (unsigned mode = 0; mode < 5; ++mode) {
        AuthSetupModel ports;
        GattsServiceFixture sdk;
        auto job = sdk.job();
        job.challenge = 0x26;
        job.issue = 0x28;
        job.proof = 0x2a;
        job.result = 0x2c;
        PeerChallengeReadSetup setup;
        setup.boot = f->boot;
        setup.context = &ports;
        setup.ingress = &ports;
        setup.issue = &ports;
        setup.auth = &ports;
        if (mode == 0)
            setup.auth = nullptr;
        if (mode == 1)
            job.proof = 0x28;
        if (mode == 2)
            job.result = 0x2a;
        if (mode == 3)
            job.result = 0x2d;
        if (mode == 4)
            job.issue = 0;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(GattsEndpointSetupPost::Rejected),
            static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses, setup)));
    }
}
#if defined(LARP_REAL_PEER_CRYPTO)
void test_auth_actual_crypto_sdk_issue_reads_proof_writes_ack_and_result_blobs() {
    Esp32PeerProofCrypto crypto;
    AuthFixture fixture(&crypto, 0x01020304);
    fixture.issueNow();
    fixture.read();
    const auto challenge = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, challenge.response.value.value, 108);
    std::string frames;
    const char *hex = "0123456789abcdef";
    auto hexString = [&](const std::uint8_t *bytes, std::size_t count) {
        std::string v;
        for (std::size_t n = 0; n < count; ++n) {
            v += hex[bytes[n] >> 4];
            v += hex[bytes[n] & 15];
        }
        return v;
    };
    for (unsigned offset = 0; offset < 72; offset += 12) {
        fixture.proofWrite(offset);
        const auto captured = fixture.authRecording.copied;
        TEST_ASSERT_TRUE(captured.scope.original == fixture.original);
        if (offset)
            frames += ", ";
        frames += "\"" +
                  hexString(reinterpret_cast<const std::uint8_t *>(captured.bytes.data()),
                            captured.count) +
                  "\"";
        authDispatch(PeerAuthDispatch::ProofProcessed, fixture.auth.service());
    }
    fixture.confirm();
    fixture.resultRead();
    fixture.sendResult();
    const auto result = host_sdk_source::readSent.back();
    fixture.resultRead(22, true);
    fixture.sendResult();
    const auto blob = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(0, result.status);
    TEST_ASSERT_EQUAL(0, blob.status);
    TEST_ASSERT_EQUAL(32, result.response.value.length);
    TEST_ASSERT_EQUAL(10, blob.response.value.length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(result.response.value.value + 22, blob.response.value.value, 10);
    PeerAuthResult decoded;
    TEST_ASSERT_TRUE(decodePeerAuthResult(result.response.value.value, 32, decoded));
    TEST_ASSERT_EQUAL(5, static_cast<unsigned>(decoded.state));
    TEST_ASSERT_EQUAL(7, decoded.principal);
    const std::string observation =
        "{\n  \"scope\": \"original_sdk_peer_auth_model_v1\",\n  \"issueHex\": \"" +
        hexString(fixture.recording.copied.bytes.data(), 18) + "\",\n  \"challengeHex\": \"" +
        hexString(challenge.response.value.value, 108) + "\",\n  \"proofFrames\": [" + frames +
        "],\n  \"resultHex\": \"" + hexString(result.response.value.value, 32) +
        "\",\n  \"blobHex\": \"" + hexString(blob.response.value.value, 10) +
        "\",\n  \"readConnection\": " + std::to_string(result.connection) +
        ",\n  \"resultTransaction\": " + std::to_string(result.transaction) +
        ",\n  \"blobTransaction\": " + std::to_string(blob.transaction) +
        ",\n  \"resultAttribute\": " + std::to_string(result.response.value.handle) +
        ",\n  \"blobOffset\": " + std::to_string(blob.response.value.offset) +
        ",\n  \"nonceCalls\": " + std::to_string(fixture.nonce.calls) +
        ",\n  \"rfMeasured\": false\n}\n";
    const char *path = "docs/companion/fixtures/peer-auth/observation.json";
    if (std::getenv("LARP_EXPORT_PEER_AUTH_FIXTURE")) {
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
