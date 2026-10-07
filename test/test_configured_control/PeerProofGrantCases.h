#pragma once
#include "PeerLinkSecurityCases.h"
void grantPost(PeerProofGrantPost expected, PeerProofGrantPost actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void grantState(PeerProofGrantState expected, PeerProofGrantState actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
struct GrantFixture : SecurityFixture {
    FreeRtosPeerProofGrantRuntime grant{source, publication, clock, f->boot};
    PeerNonceModel nonce;
    PeerProofOwner proof{grant, crypto, nonce, clock};
    explicit GrantFixture(bool gm = true) : SecurityFixture(false, gm) {
        TEST_ASSERT_TRUE(source.configurePeerProofGrantNow(f->boot, grant));
        TEST_ASSERT_FALSE(source.configurePeerProofGrantNow(f->boot, grant));
        original.connection = open();
        actor();
        pair();
        cryptoWorker();
        TEST_ASSERT_TRUE(grant.beginWorker());
    }
    void cryptoWorker() {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    }
    void verify() {
        cryptoWorker();
        PeerCredential credential;
        TEST_ASSERT_TRUE(publication.credential(testIdentity(2), credential));
        peerOutcome(PeerProofOutcome::Issued, proof.issue(original, credential));
        PeerProof value;
        TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, 72, value));
        PeerChallenge challenge;
        TEST_ASSERT_TRUE(proof.challenge(original, challenge));
        value.sequence = challenge.sequence; // Modeled crypto; each reconnect has a fresh sequence.
        peerOutcome(PeerProofOutcome::Verified, proof.verify(original, value));
    }
    PeerProofGrantJob posted() {
        verify();
        grantState(PeerProofGrantState::Pending, grant.submitVerified(proof, original));
        PeerProofGrantJob out;
        TEST_ASSERT_TRUE(grant.job(out));
        return out;
    }
    TrustedAuthority apply() {
        actor();
        host_sdk_source::drainOne();
        cryptoWorker();
        TrustedAuthority ack;
        grantState(PeerProofGrantState::Confirmed, grant.service(ack));
        return ack;
    }
};
void test_grant_actor_apply_precedes_worker_confirmation_and_completed_age_does_not_revoke() {
    GrantFixture fixture;
    auto job = fixture.posted();
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Pending, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    fixture.actor();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    fixture.cryptoWorker();
    const auto posts = host_sdk_source::posts;
    grantPost(PeerProofGrantPost::Duplicate, fixture.source.postPeerProofGrant(job));
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    const auto ack = fixture.apply();
    TEST_ASSERT_TRUE(ack.gameMaster && ack.principal == 7);
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, route));
    TEST_ASSERT_TRUE(route == ack);
    f->now += 30000;
    fixture.cryptoWorker();
    TrustedAuthority again;
    grantState(PeerProofGrantState::Confirmed, fixture.grant.service(again));
    TEST_ASSERT_TRUE(again == ack);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_grant_wrong_boot_and_unverified_owner_cannot_post_or_change_target() {
    GrantFixture fixture;
    grantState(PeerProofGrantState::Rejected,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    TEST_ASSERT_EQUAL(4, host_sdk_source::posts);
    fixture.verify();
    auto wrong = fixture.original;
    wrong.boot.low++;
    grantState(PeerProofGrantState::Rejected, fixture.grant.submitVerified(fixture.proof, wrong));
    PeerRegistryActorOrigin sentinel;
    sentinel.lifecycle = 99;
    TEST_ASSERT_FALSE(fixture.source.readPeerProofGrantOrigin(wrong, sentinel));
    TEST_ASSERT_EQUAL(99, sentinel.lifecycle);
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.source.configurePeerProofGrantNow(f->boot, fixture.grant));
}
void test_grant_cancel_before_actor_closes_exact_original_and_reconnect_recovers() {
    GrantFixture fixture;
    auto old = fixture.posted();
    const auto replay = host_sdk_source::work.front();
    fixture.grant.cancel();
    TrustedAuthority sentinel = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(sentinel));
    TEST_ASSERT_EQUAL(99, sentinel.epoch);
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_FALSE(fixture.access.route(old.proof.original.connection, sentinel));
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    fixture.cryptoWorker();
    TEST_ASSERT_TRUE(fixture.original.connection.token != old.proof.original.connection.token);
    fixture.verify();
    grantState(PeerProofGrantState::Pending,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    fixture.actor();
    replay.function(replay.context);
    fixture.cryptoWorker();
    grantState(PeerProofGrantState::Pending, fixture.grant.service(sentinel));
    TEST_ASSERT_TRUE(fixture.apply().gameMaster);
}
void test_grant_lost_ack_timeout_retires_applied_rights_and_preserves_running_game() {
    GrantFixture fixture;
    fixture.posted();
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority route;
    TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, route));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    f->now += 5000;
    fixture.cryptoWorker();
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    fixture.cryptoWorker();
    fixture.verify();
    grantState(PeerProofGrantState::Pending,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    TEST_ASSERT_TRUE(fixture.apply().gameMaster);
}
void test_grant_queue_and_proof_deadlines_expire_without_refresh_across_wrap() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        GrantFixture fixture;
        f->now = UINT32_MAX - 100;
        fixture.verify();
        if (variant == 1)
            f->now += 29999;
        grantState(PeerProofGrantState::Pending,
                   fixture.grant.submitVerified(fixture.proof, fixture.original));
        PeerProofGrantJob job;
        TEST_ASSERT_TRUE(fixture.grant.job(job));
        if (variant != 1)
            f->now += 4999;
        grantPost(PeerProofGrantPost::Duplicate, fixture.source.postPeerProofGrant(job));
        ++f->now;
        fixture.actor();
        host_sdk_source::drainOne();
        fixture.cryptoWorker();
        TrustedAuthority out = authority(99);
        grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
        TEST_ASSERT_EQUAL(99, out.epoch);
        fixture.actor();
        TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
    }
}
void test_grant_new_connection_and_foreign_publisher_survive_old_cancel() {
    GrantFixture fixture;
    auto old = fixture.posted();
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(fixture.original.connection, testVerifiedPeer()));
    TrustedAuthority fresh;
    TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, fresh));
    fixture.cryptoWorker();
    fixture.source.cancelPeerProofGrant(old);
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority current;
    TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, current));
    TEST_ASSERT_TRUE(current == fresh);
    fixture.cryptoWorker();
    TEST_ASSERT_TRUE(fixture.grant.originalLive(fixture.original));
}
void test_grant_worker_binding_wrong_actor_sdk_and_full_slot_are_closed() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        GrantFixture fixture;
        const auto job = fixture.posted();
        auto different = job;
        different.sequence++;
        grantPost(PeerProofGrantPost::Busy, fixture.source.postPeerProofGrant(different));
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
        grantPost(PeerProofGrantPost::Rejected, fixture.source.postPeerProofGrant(job));
        TrustedAuthority sentinel = authority(99);
        grantState(PeerProofGrantState::Unavailable, fixture.grant.service(sentinel));
        TEST_ASSERT_EQUAL(99, sentinel.epoch);
        if (variant == 0)
            host_sdk_source::drainOne();
        else {
            fixture.actor();
            btu_thread = reinterpret_cast<void *>(9);
            host_sdk_source::drainOne();
            btu_thread = reinterpret_cast<void *>(2);
        }
        fixture.cryptoWorker();
        grantState(PeerProofGrantState::Rejected, fixture.grant.service(sentinel));
        fixture.actor();
        TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, sentinel));
    }
}
void test_grant_expiry_during_sdk_read_is_rechecked_before_authentication() {
    GrantFixture fixture;
    fixture.posted();
    host_sdk_security::beforeState = [](std::uint8_t *) { f->now += 5000; };
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    fixture.cryptoWorker();
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(route));
}
void test_grant_credential_revocation_during_metadata_never_authenticates() {
    GrantFixture fixture;
    fixture.posted();
    static GrantFixture *observed;
    observed = &fixture;
    host_sdk_security::beforeState = [](std::uint8_t *) {
        observed->io();
        registryOutcome(PeerRegistryOutcome::Pending,
                        observed->owner.replaceTrusted(1, registryValue(2, false)));
        observed->actor();
    };
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    fixture.cryptoWorker();
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(route));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
}
void test_grant_immutable_context_wrong_boot_scope_and_record_fail_closed() {
    GrantFixture fixture;
    fixture.verify();
    auto wrong = fixture.original;
    wrong.boot.low++;
    TEST_ASSERT_FALSE(fixture.grant.originalLive(wrong));
    wrong = fixture.original;
    wrong.connection.token++;
    TEST_ASSERT_FALSE(fixture.grant.originalLive(wrong));
    PeerCredential wrongCredential = registryValue(1, true).peers[0].credential;
    wrongCredential.generation++;
    TEST_ASSERT_FALSE(fixture.grant.credentialCurrent(wrongCredential));
    PeerProofGrantJob valid;
    TEST_ASSERT_TRUE(fixture.source.readPeerProofGrantOrigin(fixture.original, valid.original));
    TEST_ASSERT_TRUE(fixture.proof.readVerified(fixture.original, valid.proof));
    valid.sequence = 1;
    valid.receivedAt = f->now;
    TEST_ASSERT_TRUE(validPeerProofGrantJob(valid));
    for (unsigned variant = 0; variant < 8; ++variant) {
        auto v = valid;
        if (variant == 0)
            v.sequence = 0;
        if (variant == 1)
            v.proof.sequence = 0;
        if (variant == 2)
            v.original.lifecycle = 0;
        if (variant == 3)
            v.proof.credential.key[0] = 0;
        if (variant == 4)
            v.original.authority.gameMaster = true;
        if (variant == 5)
            v.original.device = testIdentity(9);
        if (variant == 6)
            v.proof.verifiedAt = v.proof.issuedAt + 30000;
        if (variant == 7)
            v.receivedAt = v.proof.verifiedAt - 1;
        TEST_ASSERT_FALSE(validPeerProofGrantJob(v));
        grantPost(PeerProofGrantPost::Rejected, fixture.source.postPeerProofGrant(v));
    }
}
void test_grant_input_frozen_before_external_clock_and_observation_ports() {
    GrantFixture fixture;
    auto value = fixture.posted();
    fixture.grant.cancel();
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    fixture.cryptoWorker();
    fixture.verify();
    TEST_ASSERT_TRUE(fixture.source.readPeerProofGrantOrigin(fixture.original, value.original));
    TEST_ASSERT_TRUE(fixture.proof.readVerified(fixture.original, value.proof));
    ++value.sequence;
    value.receivedAt = f->now;
    static PeerProofGrantJob borrowed;
    borrowed = value;
    fixture.clock.before = []() {
        borrowed.proof.original.boot.low++;
        borrowed.proof.credential.generation++;
    };
    grantPost(PeerProofGrantPost::Posted, fixture.source.postPeerProofGrant(borrowed));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.cryptoWorker();
    TrustedAuthority out;
    grantState(PeerProofGrantState::Confirmed, fixture.source.peerProofGrantState(value, out));
    TEST_ASSERT_TRUE(out.gameMaster);
}
void test_identity_original_auth_conflict_preserves_foreign_publisher() {
    IdentityAccess owner(f->channel.actual);
    accessOutcome(IdentityAccessOutcome::Initialized, owner.begin(testIdentity()));
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, owner.connect(257, link));
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.advanceAuthority(99, true, foreign)));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  owner.authenticate(link, testVerifiedPeer()));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == foreign);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  owner.assignGameMaster(testIdentity(2), 7));
}

void test_grant_deadline_crossed_after_clock_sample_never_confirms_lost_ack() {
    GrantFixture fixture;
    fixture.posted();
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.cryptoWorker();
    fixture.clock.after = []() { f->now += 5000; };
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
}
void test_grant_rejected_sdk_post_is_consumed_and_fresh_reconnect_can_recover() {
    GrantFixture fixture;
    fixture.verify();
    host_sdk_source::rejectPost = true;
    grantState(PeerProofGrantState::Rejected,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    const auto count = host_sdk_source::posts;
    host_sdk_source::rejectPost = false;
    grantState(PeerProofGrantState::Rejected,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    TEST_ASSERT_EQUAL(count, host_sdk_source::posts);
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.pair();
    fixture.verify();
    grantState(PeerProofGrantState::Pending,
               fixture.grant.submitVerified(fixture.proof, fixture.original));
    TEST_ASSERT_TRUE(fixture.apply().gameMaster);
}
void test_grant_opaque_counter_exhaustion_never_posts_or_opens_rights() {
    GrantFixture fixture;
    fixture.verify();
    const auto prior = larp::companion::proofGrantPostCounter;
    larp::companion::proofGrantPostCounter = UINT32_MAX;
    const auto posts = host_sdk_source::posts;
    const auto result = fixture.grant.submitVerified(fixture.proof, fixture.original);
    larp::companion::proofGrantPostCounter = prior; // TEST ONLY quiescent simulated boot.
    grantState(PeerProofGrantState::Rejected, result);
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    fixture.actor();
    TrustedAuthority out;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
}
void test_grant_key_possession_without_stored_gm_assignment_never_opens_game_control() {
    GrantFixture fixture(false);
    fixture.posted();
    const auto ack = fixture.apply();
    TEST_ASSERT_TRUE(ack.epoch && !ack.gameMaster && !ack.principal);
    fixture.actor();
    TrustedAuthority out;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
    TEST_ASSERT_FALSE(f->submit(f->command(), ack.epoch, 7));
}
void test_grant_cancel_during_metadata_never_authenticates_or_disturbs_closed_publisher() {
    GrantFixture fixture;
    fixture.posted();
    TrustedAuthority before;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(before));
    static GrantFixture *current;
    current = &fixture;
    host_sdk_security::beforeState = [](std::uint8_t *) {
        current->cryptoWorker();
        current->grant.cancel();
        current->actor();
    };
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority after;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(after));
    TEST_ASSERT_TRUE(after == before);
    fixture.cryptoWorker();
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(after));
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, after));
}
void test_grant_foreign_publisher_after_applied_ack_survives_worker_rejection() {
    GrantFixture fixture;
    fixture.posted();
    fixture.actor();
    host_sdk_source::drainOne();
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.advanceAuthority(99, true, foreign)));
    fixture.cryptoWorker();
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    TrustedAuthority actual;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(actual));
    TEST_ASSERT_TRUE(actual == foreign);
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
}
void test_grant_close_ack_requires_exact_owned_connection_authority_and_closed_publisher() {
    GrantFixture fixture;
    fixture.posted();
    const auto ack = fixture.apply();
    TrustedAuthority closed;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(fixture.access.closeCopiedAuthority(ack, closed)));
    fixture.actor();
    auto wrong = fixture.original.connection;
    ++wrong.token;
    TEST_ASSERT_FALSE(fixture.access.acknowledgeCopiedClose(wrong, ack, closed));
    auto wrongAck = closed;
    wrongAck.gameMaster = true;
    wrongAck.principal = 7;
    TEST_ASSERT_FALSE(
        fixture.access.acknowledgeCopiedClose(fixture.original.connection, ack, wrongAck));
    auto wrongExpected = ack;
    ++wrongExpected.epoch;
    TEST_ASSERT_FALSE(
        fixture.access.acknowledgeCopiedClose(fixture.original.connection, wrongExpected, closed));
    TEST_ASSERT_TRUE(
        fixture.access.acknowledgeCopiedClose(fixture.original.connection, ack, closed));
    TEST_ASSERT_FALSE(
        fixture.access.acknowledgeCopiedClose(fixture.original.connection, ack, closed));
    TrustedAuthority actual;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(actual));
    TEST_ASSERT_TRUE(actual == closed);
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, actual));
}

void test_grant_applied_cancel_and_queued_role_revoke_share_exact_own_close_ack() {
    GrantFixture fixture;
    fixture.posted();
    const auto ack = fixture.apply();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    fixture.cryptoWorker();
    TrustedAuthority out = authority(99);
    grantState(PeerProofGrantState::Rejected, fixture.grant.service(out));
    TEST_ASSERT_EQUAL(99, out.epoch);
    TrustedAuthority closed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(closed));
    TEST_ASSERT_TRUE(closed.epoch > ack.epoch && !closed.gameMaster && !closed.principal);
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    PeerRegistrySnapshot snapshot;
    TEST_ASSERT_TRUE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_TRUE(snapshot == registryValue(2, false));
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, out));
}

void test_grant_publisher_freezes_expected_before_lock_and_preserves_replacement() {
    TrustedAuthority expected;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(expected));
    static TrustedAuthority *borrowed;
    borrowed = &expected;
    static TrustedAuthority replacement;
    host_rtos::beforeEnter = [] {
        f->channel.actual.advanceAuthority(99, true, replacement);
        *borrowed = replacement;
    };
    TrustedAuthority out = authority(99);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Conflict),
        static_cast<unsigned>(f->channel.actual.advanceAuthorityIfCurrent(expected, 7, true, out)));
    TEST_ASSERT_EQUAL(99, out.epoch);
    TrustedAuthority actual;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(actual));
    TEST_ASSERT_TRUE(actual == replacement);
    TEST_ASSERT_EQUAL(99, actual.principal);
}

GrantFixture *grantPollingFixture = nullptr;
bool grantPollingObserved = false;
void grantPollAtClearedIdentityObservation() {
    auto &fixture = *grantPollingFixture;
    fixture.cryptoWorker();
    const bool current = fixture.grant.originalLive(fixture.original);
    if (!current) {
        grantPollingObserved = true;
        TrustedAuthority out = authority(99);
        const auto result = fixture.grant.service(out);
        fixture.actor();
        grantState(PeerProofGrantState::Pending, result);
        TEST_ASSERT_EQUAL(99, out.epoch);
    } else {
        fixture.actor();
        host_rtos::beforeEnter = grantPollAtClearedIdentityObservation;
    }
}
void test_grant_poll_during_actor_publication_remains_pending_without_mutable_owner_read() {
    GrantFixture fixture;
    fixture.posted();
    grantPollingFixture = &fixture;
    grantPollingObserved = false;
    host_sdk_security::beforeState = [](std::uint8_t *) {
        host_rtos::beforeEnter = grantPollAtClearedIdentityObservation;
    };
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_TRUE(grantPollingObserved);
    fixture.cryptoWorker();
    TrustedAuthority ack;
    grantState(PeerProofGrantState::Confirmed, fixture.grant.service(ack));
    TEST_ASSERT_TRUE(ack.gameMaster && ack.principal == 7);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
