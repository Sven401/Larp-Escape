#pragma once
#include "PeerProofVectors.h"
struct PeerContext : IPeerProofContext {
    PeerProofBinding binding;
    PeerCredential credential;
    bool live = true, ready = true, key = true;
    bool originalLive(const PeerProofBinding &value) const noexcept override {
        return live && ready && value == binding;
    }
    bool credentialCurrent(const PeerCredential &value) const noexcept override {
        return key && value == credential;
    }
};
struct PeerClock : IProfileImportClock {
    mutable void (*before)() = nullptr;
    std::uint32_t nowMilliseconds() const noexcept override {
        if (before) {
            auto hook = before;
            before = nullptr;
            hook();
        }
        return f->now;
    }
};
struct PeerNonceModel : IPeerProofNonce {
    unsigned calls = 0;
    bool fail = false, zero = false;
    void (*before)() = nullptr;
    bool make(PeerNonce &out) noexcept override {
        ++calls;
        if (before) {
            auto hook = before;
            before = nullptr;
            hook();
        }
        if (fail)
            return false;
        PeerNonce value;
        for (unsigned i = 0; i < value.size(); ++i)
            value[i] = zero ? 0 : i + 1;
        out = value;
        return true;
    }
};
struct PeerCryptoModel : IPeerProofCrypto {
    unsigned keys = 0, proofs = 0;
    bool key = true, accepted = true;
    void (*before)() = nullptr;
    PeerTranscript seen;
    bool validKey(const PeerPublicKey &) noexcept override {
        ++keys;
        return key;
    }
    bool verify(const PeerPublicKey &, const PeerTranscript &value,
                const PeerSignature &) noexcept override {
        ++proofs;
        seen = value;
        if (before) {
            auto hook = before;
            before = nullptr;
            hook();
        }
        return accepted;
    }
};
void peerOutcome(PeerProofOutcome expected, PeerProofOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
struct PeerFixture {
    IdentityAccess access{f->channel.actual};
    PeerContext context;
    PeerClock clock;
    PeerNonceModel nonce;
    PeerCryptoModel crypto;
    PeerProofOwner owner;
    PeerProofReceiver receiver{owner};
    PeerFixture(IPeerProofCrypto *actual = nullptr, std::uint32_t seed = 0)
        : owner(context, actual ? *actual : crypto, nonce, clock, seed) {
        context.binding.device = testIdentity();
        context.binding.boot = f->boot;
        context.binding.registration = 0x01020304;
        context.binding.interface = 1;
        accessOutcome(IdentityAccessOutcome::Initialized, access.begin(context.binding.device));
        accessOutcome(IdentityAccessOutcome::Connected,
                      access.connect(257, context.binding.connection));
        context.credential.peer = testIdentity(2);
        context.credential.generation = 1;
        std::copy(peerTestPublicKey, peerTestPublicKey + 65, context.credential.key.begin());
    }
    PeerProof proof() {
        PeerProof p;
        TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, sizeof(peerTestProofBytes), p));
        return p;
    }
    PeerChallenge issue() {
        peerOutcome(PeerProofOutcome::Issued, owner.issue(context.binding, context.credential));
        PeerChallenge value;
        TEST_ASSERT_TRUE(owner.challenge(context.binding, value));
        return value;
    }
    void verified() {
        issue();
        peerOutcome(PeerProofOutcome::Verified, owner.verify(context.binding, proof()));
    }
    void reconnect() {
        accessOutcome(IdentityAccessOutcome::Disconnected,
                      access.disconnect(context.binding.connection));
        accessOutcome(IdentityAccessOutcome::Connected,
                      access.connect(257, context.binding.connection));
    }
    std::string frame(unsigned offset, unsigned count = 12, std::uint16_t message = 1) {
        char out[128];
        std::size_t written = 0;
        TEST_ASSERT_TRUE(encodePeerProofFrame(
            message, 72, offset, reinterpret_cast<const char *>(peerTestProofBytes) + offset, count,
            out, sizeof(out), written));
        return std::string(out, written);
    }
    PeerProofReceive receive(const std::string &v) {
        return receiver.receive(context.binding, v.data(), v.size());
    }
};
void test_peer_challenge_and_proof_canonical_bytes_and_decoder_destinations() {
    PeerFixture peer;
    const auto challenge = peer.issue();
    PeerTranscript encoded;
    TEST_ASSERT_TRUE(encodePeerChallenge(challenge, encoded));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, encoded.data(), 108);
    PeerTranscript framed = {};
    unsigned frameCount = 0;
    for (std::size_t offset = 0; offset < encoded.size(); offset += 12) {
        char outer[20];
        std::size_t written = 0;
        CommandFrame part;
        TEST_ASSERT_TRUE(encodePeerChallengeFrame(
            1, 108, offset, reinterpret_cast<const char *>(encoded.data() + offset), 12, outer,
            sizeof(outer), written));
        TEST_ASSERT_EQUAL(20, written);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                          static_cast<unsigned>(decodePeerChallengeFrame(outer, written, part)));
        std::memcpy(framed.data() + part.offset, part.payload, part.count);
        ++frameCount;
    }
    TEST_ASSERT_EQUAL(9, frameCount);
    TEST_ASSERT_TRUE(framed == encoded);
    PeerChallenge decoded;
    TEST_ASSERT_TRUE(decodePeerChallenge(encoded.data(), encoded.size(), decoded));
    TEST_ASSERT_TRUE(decoded.original == challenge.original && decoded.peer == challenge.peer &&
                     decoded.nonce == challenge.nonce);
    std::array<std::uint8_t, 72> response;
    TEST_ASSERT_TRUE(encodePeerProof(peer.proof(), response));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestProofBytes, response.data(), 72);
}
void test_peer_fixed_frame_total_must_match_and_preserve_decoder_target() {
    for (unsigned proof = 0; proof < 2; ++proof) {
        const std::uint16_t exact = proof ? 72 : 108;
        for (const std::uint16_t wrong :
             {std::uint16_t(20), std::uint16_t(exact - 1), std::uint16_t(exact + 1)}) {
            char value[20];
            std::size_t written = 0;
            const char *payload =
                reinterpret_cast<const char *>(proof ? peerTestProofBytes : peerTestTranscript);
            const auto encode = proof ? encodePeerProofFrame : encodePeerChallengeFrame;
            const auto decode = proof ? decodePeerProofFrame : decodePeerChallengeFrame;
            TEST_ASSERT_TRUE(encode(1, exact, 0, payload, 12, value, sizeof(value), written));
            value[4] = static_cast<char>(wrong);
            value[5] = static_cast<char>(wrong >> 8);
            CommandFrame unchanged;
            unchanged.message = 90;
            unchanged.total = 91;
            unchanged.offset = 92;
            unchanged.payload = payload;
            unchanged.count = 93;
            TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                              static_cast<unsigned>(decode(value, sizeof(value), unchanged)));
            TEST_ASSERT_TRUE(unchanged.message == 90 && unchanged.total == 91 &&
                             unchanged.offset == 92 && unchanged.payload == payload &&
                             unchanged.count == 93);
            written = 99;
            value[0] = 99;
            TEST_ASSERT_FALSE(encode(1, wrong, 0, payload, 12, value, sizeof(value), written));
            TEST_ASSERT_EQUAL(0, written);
            TEST_ASSERT_EQUAL(0, value[0]);
        }
    }
}
void test_peer_challenge_malformed_fields_preserve_decoder_target() {
    PeerFixture peer;
    const auto original = peer.issue();
    for (unsigned variant = 0; variant < 15; ++variant) {
        PeerTranscript value;
        std::copy(peerTestTranscript, peerTestTranscript + 108, value.begin());
        if (variant < 6)
            value[variant == 0   ? 0
                  : variant == 1 ? 16
                  : variant == 2 ? 17
                  : variant == 3 ? 18
                  : variant == 4 ? 19
                                 : 55] ^= 1;
        if (variant == 6)
            std::fill(value.begin() + 20, value.begin() + 36, 0);
        if (variant == 7)
            std::fill(value.begin() + 36, value.begin() + 44, 0);
        if (variant == 8)
            std::fill(value.begin() + 44, value.begin() + 48, 0);
        if (variant == 9)
            std::fill(value.begin() + 48, value.begin() + 52, 0);
        if (variant == 10)
            value[52] = 2;
        if (variant == 11)
            value[54] = 255;
        if (variant == 12)
            std::fill(value.begin() + 56, value.begin() + 60, 0);
        if (variant == 13)
            std::fill(value.begin() + 60, value.begin() + 76, 0);
        if (variant == 14)
            std::fill(value.begin() + 76, value.end(), 0);
        auto out = original;
        TEST_ASSERT_FALSE(decodePeerChallenge(value.data(), value.size(), out));
        TEST_ASSERT_TRUE(out.original == original.original && out.nonce == original.nonce &&
                         out.sequence == original.sequence);
    }
    auto out = original;
    TEST_ASSERT_FALSE(decodePeerChallenge(nullptr, 108, out));
    TEST_ASSERT_FALSE(decodePeerChallenge(peerTestTranscript, 107, out));
    PeerTranscript preserved;
    preserved.fill(99);
    auto invalid = original;
    invalid.sequence = 0;
    TEST_ASSERT_FALSE(encodePeerChallenge(invalid, preserved));
    TEST_ASSERT_EQUAL(99, preserved[0]);
}
void test_peer_proof_malformed_header_sequence_signature_and_extent_preserve() {
    PeerFixture peer;
    const auto original = peer.proof();
    for (unsigned variant = 0; variant < 6; ++variant) {
        std::array<std::uint8_t, 72> value;
        std::copy(peerTestProofBytes, peerTestProofBytes + 72, value.begin());
        if (variant < 4)
            value[variant] ^= 1;
        if (variant == 4)
            std::fill(value.begin() + 4, value.begin() + 8, 0);
        if (variant == 5)
            std::fill(value.begin() + 8, value.end(), 0);
        auto out = original;
        TEST_ASSERT_FALSE(decodePeerProof(value.data(), value.size(), out));
        TEST_ASSERT_TRUE(out.signature == original.signature && out.sequence == original.sequence);
    }
    auto out = original;
    TEST_ASSERT_FALSE(decodePeerProof(nullptr, 72, out));
    TEST_ASSERT_FALSE(decodePeerProof(peerTestProofBytes, 71, out));
}
void test_peer_one_challenge_per_original_connection_and_unchanged_retry() {
    PeerFixture peer;
    const auto first = peer.issue();
    peerOutcome(PeerProofOutcome::Consumed,
                peer.owner.issue(peer.context.binding, peer.context.credential));
    PeerChallenge repeat;
    ++f->now;
    TEST_ASSERT_TRUE(peer.owner.challenge(peer.context.binding, repeat));
    TEST_ASSERT_TRUE(repeat.nonce == first.nonce && repeat.sequence == first.sequence);
    TEST_ASSERT_EQUAL(1, peer.nonce.calls);
    TEST_ASSERT_EQUAL(1, peer.crypto.keys);
    peerOutcome(PeerProofOutcome::Verified, peer.owner.verify(peer.context.binding, peer.proof()));
    TEST_ASSERT_TRUE(peer.owner.challenge(peer.context.binding, repeat));
    peerOutcome(PeerProofOutcome::Duplicate, peer.owner.verify(peer.context.binding, peer.proof()));
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
}
void test_peer_absolute_challenge_deadline_including_verification_and_wrap() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        if (variant == 2)
            f->now = UINT32_MAX - 10;
        PeerFixture peer;
        const auto issuedAt = f->now;
        peer.issue();
        f->now = issuedAt + 29999;
        PeerChallenge out;
        TEST_ASSERT_TRUE(peer.owner.challenge(peer.context.binding, out));
        f->now = issuedAt + 30000;
        peerOutcome(PeerProofOutcome::Expired,
                    peer.owner.verify(peer.context.binding, peer.proof()));
        TEST_ASSERT_EQUAL(0, peer.crypto.proofs);
        peerOutcome(PeerProofOutcome::Consumed,
                    peer.owner.issue(peer.context.binding, peer.context.credential));
    }
}
void test_peer_deadline_crossed_inside_actual_crypto_port_stays_consumed() {
    PeerFixture peer;
    peer.issue();
    peer.crypto.before = []() { f->now += 30000; };
    peerOutcome(PeerProofOutcome::Expired, peer.owner.verify(peer.context.binding, peer.proof()));
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
    VerifiedPeerProof result;
    result.sequence = 99;
    TEST_ASSERT_FALSE(peer.owner.readVerified(peer.context.binding, result));
    TEST_ASSERT_EQUAL(99, result.sequence);
}
void test_peer_conflicting_duplicate_closes_proof_without_second_crypto() {
    PeerFixture peer;
    peer.verified();
    auto changed = peer.proof();
    changed.signature[0] ^= 1;
    peerOutcome(PeerProofOutcome::SignatureRejected,
                peer.owner.verify(peer.context.binding, changed));
    VerifiedPeerProof result;
    TEST_ASSERT_FALSE(peer.owner.readVerified(peer.context.binding, result));
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
}
void test_peer_old_and_wrong_origins_do_not_consume_current_attempt() {
    PeerFixture peer;
    peer.issue();
    auto wrong = peer.context.binding;
    ++wrong.connection.token;
    peerOutcome(PeerProofOutcome::Stale, peer.owner.verify(wrong, peer.proof()));
    ++wrong.registration;
    VerifiedPeerProof result;
    result.sequence = 99;
    TEST_ASSERT_FALSE(peer.owner.readVerified(wrong, result));
    TEST_ASSERT_EQUAL(99, result.sequence);
    peerOutcome(PeerProofOutcome::Verified, peer.owner.verify(peer.context.binding, peer.proof()));
}
void test_peer_reconnect_same_handle_requires_new_sequence_and_original() {
    PeerFixture peer;
    const auto first = peer.issue();
    peer.reconnect();
    const auto next = peer.issue();
    TEST_ASSERT_TRUE(first.original.connection.token != next.original.connection.token);
    TEST_ASSERT_EQUAL(first.original.connection.handle, next.original.connection.handle);
    TEST_ASSERT_EQUAL_UINT32(first.sequence + 1, next.sequence);
    peerOutcome(PeerProofOutcome::Stale, peer.owner.verify(first.original, peer.proof()));
    peerOutcome(PeerProofOutcome::Mismatch, peer.owner.verify(next.original, peer.proof()));
    auto fresh = peer.proof();
    fresh.sequence = next.sequence;
    peerOutcome(PeerProofOutcome::Verified, peer.owner.verify(next.original, fresh));
}
void test_peer_invalid_context_credentials_and_entropy_fail_closed() {
    for (unsigned variant = 0; variant < 6; ++variant) {
        PeerFixture peer;
        if (variant == 0)
            peer.context.ready = false;
        if (variant == 1)
            peer.context.credential.generation = 0;
        if (variant == 2)
            peer.context.key = false;
        if (variant == 3)
            peer.crypto.key = false;
        if (variant == 4)
            peer.nonce.fail = true;
        if (variant == 5)
            peer.nonce.zero = true;
        const auto expected = variant == 0  ? PeerProofOutcome::InvalidContext
                              : variant < 4 ? PeerProofOutcome::InvalidCredential
                                            : PeerProofOutcome::EntropyFailure;
        peerOutcome(expected, peer.owner.issue(peer.context.binding, peer.context.credential));
        PeerChallenge out;
        TEST_ASSERT_FALSE(peer.owner.challenge(peer.context.binding, out));
    }
}
void test_peer_key_revocation_before_and_after_crypto_never_publishes() {
    static PeerContext *context = nullptr;
    for (unsigned variant = 0; variant < 2; ++variant) {
        PeerFixture peer;
        peer.issue();
        context = &peer.context;
        if (!variant)
            peer.context.key = false;
        else
            peer.crypto.before = []() { context->credential.generation++; };
        peerOutcome(PeerProofOutcome::Stale, peer.owner.verify(peer.context.binding, peer.proof()));
        TEST_ASSERT_EQUAL(variant, peer.crypto.proofs);
        VerifiedPeerProof out;
        out.sequence = 99;
        TEST_ASSERT_FALSE(peer.owner.readVerified(peer.context.binding, out));
        TEST_ASSERT_EQUAL(99, out.sequence);
    }
}
void test_peer_source_closure_before_and_after_crypto_never_publishes() {
    static PeerContext *context = nullptr;
    for (unsigned variant = 0; variant < 2; ++variant) {
        PeerFixture peer;
        peer.issue();
        context = &peer.context;
        if (!variant)
            peer.context.ready = false;
        else
            peer.crypto.before = []() { context->ready = false; };
        peerOutcome(PeerProofOutcome::Stale, peer.owner.verify(peer.context.binding, peer.proof()));
        TEST_ASSERT_EQUAL(variant, peer.crypto.proofs);
    }
}
void test_peer_signature_attempt_consumed_before_reentry_and_without_retry() {
    PeerFixture peer;
    peer.issue();
    static PeerFixture *current = nullptr;
    current = &peer;
    peer.crypto.before = []() {
        peerOutcome(PeerProofOutcome::Busy,
                    current->owner.verify(current->context.binding, current->proof()));
    };
    peer.crypto.accepted = false;
    peerOutcome(PeerProofOutcome::SignatureRejected,
                peer.owner.verify(peer.context.binding, peer.proof()));
    peer.crypto.accepted = true;
    peerOutcome(PeerProofOutcome::Consumed, peer.owner.verify(peer.context.binding, peer.proof()));
    peerOutcome(PeerProofOutcome::Consumed,
                peer.owner.issue(peer.context.binding, peer.context.credential));
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
}
void test_peer_issue_reentry_and_frozen_inputs_before_clock_observation() {
    PeerFixture peer;
    static PeerFixture *current = nullptr;
    current = &peer;
    peer.nonce.before = []() {
        peerOutcome(PeerProofOutcome::Busy,
                    current->owner.issue(current->context.binding, current->context.credential));
    };
    peer.issue();
    static PeerProof proof;
    proof = peer.proof();
    peer.clock.before = []() {
        proof.signature.fill(0);
        proof.sequence = 0;
    };
    peerOutcome(PeerProofOutcome::Verified, peer.owner.verify(peer.context.binding, proof));
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
}
void test_peer_duplicate_does_not_refresh_age_or_renew_key_proof() {
    PeerFixture peer;
    const auto issuedAt = f->now;
    peer.verified();
    VerifiedPeerProof first;
    TEST_ASSERT_TRUE(peer.owner.readVerified(peer.context.binding, first));
    AcceptedPeerProof original;
    TEST_ASSERT_TRUE(peer.owner.readVerifiedEvidence(peer.context.binding, original));
    TEST_ASSERT_TRUE(original.verified.original == first.original);
    TEST_ASSERT_TRUE(original.verified.credential == first.credential);
    TEST_ASSERT_EQUAL(first.issuedAt, original.verified.issuedAt);
    TEST_ASSERT_EQUAL(first.verifiedAt, original.verified.verifiedAt);
    TEST_ASSERT_TRUE(original.transcript == peer.crypto.seen);
    TEST_ASSERT_TRUE(original.signature == peer.proof().signature);
    original.signature.fill(7); // Caller modification cannot change owner evidence.
    AcceptedPeerProof ownedAgain;
    TEST_ASSERT_TRUE(peer.owner.readVerifiedEvidence(peer.context.binding, ownedAgain));
    TEST_ASSERT_TRUE(ownedAgain.signature == peer.proof().signature);
    f->now += 100;
    peerOutcome(PeerProofOutcome::Duplicate, peer.owner.verify(peer.context.binding, peer.proof()));
    VerifiedPeerProof repeat;
    TEST_ASSERT_TRUE(peer.owner.readVerified(peer.context.binding, repeat));
    TEST_ASSERT_EQUAL(first.issuedAt, repeat.issuedAt);
    TEST_ASSERT_EQUAL(first.verifiedAt, repeat.verifiedAt);
    f->now = issuedAt + 30000;
    TEST_ASSERT_FALSE(peer.owner.readVerified(peer.context.binding, repeat));
    TEST_ASSERT_FALSE(peer.owner.readVerifiedEvidence(peer.context.binding, original));
    TEST_ASSERT_EQUAL(7, original.signature[0]); // Failure preserves previous caller output.
}
void test_peer_counter_exhaustion_never_wraps_and_wrong_sequence_preserves() {
    PeerFixture peer(nullptr, UINT32_MAX - 1);
    const auto first = peer.issue();
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, first.sequence);
    peerOutcome(PeerProofOutcome::Mismatch, peer.owner.verify(peer.context.binding, peer.proof()));
    auto p = peer.proof();
    p.sequence = UINT32_MAX;
    peerOutcome(PeerProofOutcome::Verified, peer.owner.verify(peer.context.binding, p));
    peer.reconnect();
    peerOutcome(PeerProofOutcome::Exhausted,
                peer.owner.issue(peer.context.binding, peer.context.credential));
    TEST_ASSERT_EQUAL(1, peer.nonce.calls);
}
void test_peer_proof_cannot_fabricate_transport_security_or_game_master() {
    PeerFixture peer;
    peer.verified();
    VerifiedPeerProof proof;
    TEST_ASSERT_TRUE(peer.owner.readVerified(peer.context.binding, proof));
    for (unsigned variant = 0; variant < 5; ++variant) {
        OriginalPeerSecurity security;
        security.original = peer.context.binding;
        security.encrypted = variant != 0;
        security.mitm = variant != 1;
        security.secureConnections = variant != 2;
        security.bonded = variant != 3;
        if (variant == 4)
            ++security.original.connection.token;
        auto evidence = testVerifiedPeer(99);
        TEST_ASSERT_FALSE(bindPeerProofEvidence(proof, security, evidence));
        TEST_ASSERT_TRUE(evidence.peer == testIdentity(99));
    }
    OriginalPeerSecurity MODELED;
    MODELED.original = peer.context.binding;
    MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
    VerifiedPeerEvidence evidence;
    TEST_ASSERT_TRUE(bindPeerProofEvidence(proof, MODELED, evidence));
    accessOutcome(IdentityAccessOutcome::Verified,
                  peer.access.authenticate(peer.context.binding.connection, evidence));
    TrustedAuthority route;
    TEST_ASSERT_FALSE(peer.access.route(peer.context.binding.connection, route));
    accessOutcome(IdentityAccessOutcome::Assigned,
                  peer.access.assignGameMaster(proof.credential.peer, 7));
    TEST_ASSERT_TRUE(peer.access.route(peer.context.binding.connection, route));
}
void test_peer_receiver_six_actual_twenty_byte_frames_and_full_duplicate() {
    PeerFixture peer;
    peer.issue();
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        PeerProofReceive result;
        for (unsigned i = 0; i < 6; ++i) {
            const auto value = peer.frame(i * 12);
            result = peer.receive(value);
            if (i < 5)
                TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Receiving),
                                  static_cast<unsigned>(result.kind));
            if (!i)
                TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::DuplicateFragment),
                                  static_cast<unsigned>(peer.receive(value).kind));
        }
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Completed),
                          static_cast<unsigned>(result.kind));
        peerOutcome(repeat ? PeerProofOutcome::Duplicate : PeerProofOutcome::Verified,
                    result.proof);
        TEST_ASSERT_EQUAL(72, result.received);
    }
    TEST_ASSERT_EQUAL(1, peer.crypto.proofs);
}
void test_peer_receiver_bad_order_message_overlap_conflict_and_header() {
    PeerFixture peer;
    peer.issue();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::OutOfOrder),
                      static_cast<unsigned>(peer.receive(peer.frame(12)).kind));
    peer.receive(peer.frame(0));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Busy),
                      static_cast<unsigned>(peer.receive(peer.frame(12, 12, 2)).kind));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::OutOfOrder),
                      static_cast<unsigned>(peer.receive(peer.frame(6)).kind));
    auto malformed = peer.frame(0);
    malformed[8] ^= 1;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Conflict),
                      static_cast<unsigned>(peer.receive(malformed).kind));
    malformed = peer.frame(0);
    malformed[1] = 8;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::UnsupportedFrame),
                      static_cast<unsigned>(peer.receive(malformed).kind));
    TEST_ASSERT_EQUAL(0, peer.crypto.proofs);
}
void test_peer_receiver_original_age_and_snapshot_freeze_before_observation() {
    PeerFixture peer;
    peer.issue();
    static std::string value;
    value = peer.frame(0);
    peer.clock.before = []() { std::fill(value.begin(), value.end(), 0); };
    const auto copy = peer.receiver.receive(peer.context.binding, value.data(), value.size());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Receiving),
                      static_cast<unsigned>(copy.kind));
    for (unsigned i = 1; i < 5; ++i)
        peer.receive(peer.frame(i * 12));
    f->now += 30000;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Stale),
                      static_cast<unsigned>(peer.receive(peer.frame(60)).kind));
    TEST_ASSERT_EQUAL(0, peer.crypto.proofs);
}
void test_peer_queue_four_fixed_copies_zero_wait_and_no_callback_crypto() {
    PeerFixture peer;
    peer.issue();
    FreeRtosPeerProofQueue queue(peer.context, peer.clock);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = peer.frame(0);
    for (unsigned i = 0; i < 4; ++i)
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Copied),
                          static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                                value.size(), f->now)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Full),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now)));
    TEST_ASSERT_EQUAL(0, peer.crypto.proofs);
    const auto dispatched = dispatchPeerProofFrame(queue, peer.receiver);
    TEST_ASSERT_TRUE(dispatched.received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofReceiveKind::Receiving),
                      static_cast<unsigned>(dispatched.result.kind));
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_peer_queue_freezes_bytes_origin_and_first_timestamp() {
    PeerFixture peer;
    peer.issue();
    FreeRtosPeerProofQueue queue(peer.context, peer.clock);
    TEST_ASSERT_TRUE(queue.begin());
    static PeerProofBinding original;
    original = peer.context.binding;
    static std::string value;
    value = peer.frame(0);
    const auto expected = value;
    peer.clock.before = []() {
        original.registration = 0;
        std::fill(value.begin(), value.end(), 0);
    };
    const auto received = f->now;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(PeerProofCopy::Copied),
        static_cast<unsigned>(queue.copyWrite(original, value.data(), value.size(), received)));
    ++f->now;
    CopiedPeerProofFrame out;
    TEST_ASSERT_TRUE(queue.takeCurrent(out));
    TEST_ASSERT_TRUE(out.original == peer.context.binding);
    TEST_ASSERT_EQUAL(received, out.receivedAt);
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), out.bytes.data(), expected.size());
}
void test_peer_queue_old_connection_ready_loss_and_expiry_never_rebind() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        PeerFixture peer;
        peer.issue();
        FreeRtosPeerProofQueue queue(peer.context, peer.clock);
        TEST_ASSERT_TRUE(queue.begin());
        const auto value = peer.frame(0);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Copied),
                          static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                                value.size(), f->now)));
        if (!variant)
            peer.reconnect();
        else if (variant == 1)
            peer.context.ready = false;
        else
            f->now += 1000;
        CopiedPeerProofFrame out;
        out.count = 99;
        TEST_ASSERT_FALSE(queue.takeCurrent(out));
        TEST_ASSERT_EQUAL(99, out.count);
        TEST_ASSERT_EQUAL(0, peer.crypto.proofs);
    }
}
void test_peer_queue_bad_write_and_closed_port_preserve_destination() {
    PeerFixture peer;
    FreeRtosPeerProofQueue queue(peer.context, peer.clock);
    const auto value = peer.frame(0);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Unavailable),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now)));
    TEST_ASSERT_TRUE(queue.begin());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Invalid),
                      static_cast<unsigned>(
                          queue.copyWrite(peer.context.binding, nullptr, value.size(), f->now)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Invalid),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now, true)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Invalid),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now, false, 1)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Expired),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now - 1000)));
    peer.context.ready = false;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Stale),
                      static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                            value.size(), f->now)));
}
void test_peer_queue_receiver_reentry_cannot_take_another_item() {
    PeerFixture peer;
    FreeRtosPeerProofQueue queue(peer.context, peer.clock);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = peer.frame(0);
    for (unsigned i = 0; i < 2; ++i)
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Copied),
                          static_cast<unsigned>(queue.copyWrite(peer.context.binding, value.data(),
                                                                value.size(), f->now)));
    static FreeRtosPeerProofQueue *reentrant = nullptr;
    reentrant = &queue;
    peer.clock.before = []() {
        CopiedPeerProofFrame out;
        out.count = 99;
        TEST_ASSERT_FALSE(reentrant->takeCurrent(out));
        TEST_ASSERT_EQUAL(99, out.count);
    };
    CopiedPeerProofFrame out;
    TEST_ASSERT_TRUE(queue.takeCurrent(out));
    TEST_ASSERT_TRUE(queue.takeCurrent(out));
    TEST_ASSERT_FALSE(queue.takeCurrent(out));
}
#if defined(LARP_REAL_PEER_CRYPTO)
void test_peer_actual_mbedtls_p256_independent_signature_and_bad_public_inputs() {
    Esp32PeerProofCrypto actual;
    PeerPublicKey key;
    PeerTranscript transcript;
    PeerProof proof;
    std::copy(peerTestPublicKey, peerTestPublicKey + 65, key.begin());
    std::copy(peerTestTranscript, peerTestTranscript + 108, transcript.begin());
    TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, 72, proof));
    TEST_ASSERT_TRUE(actual.validKey(key));
    TEST_ASSERT_TRUE(actual.verify(key, transcript, proof.signature));
    for (unsigned i = 0; i < 108; ++i) {
        auto changed = transcript;
        changed[i] ^= 1;
        TEST_ASSERT_FALSE(actual.verify(key, changed, proof.signature));
    }
    for (unsigned i = 0; i < 64; ++i) {
        auto changed = proof.signature;
        changed[i] ^= 1;
        TEST_ASSERT_FALSE(actual.verify(key, transcript, changed));
    }
    for (unsigned variant = 0; variant < 3; ++variant) {
        auto changed = key;
        if (!variant)
            changed[0] = 2;
        else if (variant == 1)
            std::fill(changed.begin() + 1, changed.end(), 0);
        else
            changed[1] ^= 1;
        TEST_ASSERT_FALSE(actual.validKey(changed));
        TEST_ASSERT_FALSE(actual.verify(changed, transcript, proof.signature));
    }
    auto zero = proof.signature;
    std::fill(zero.begin(), zero.begin() + 32, 0);
    TEST_ASSERT_FALSE(actual.verify(key, transcript, zero));
    // A mathematically valid high-S twin is deliberately noncanonical.
    auto high = proof.signature;
    const std::uint8_t n[32] = {0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
                                0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};
    auto boundary = proof.signature;
    std::fill(boundary.begin() + 32, boundary.end(), 0);
    TEST_ASSERT_FALSE(actual.verify(key, transcript, boundary)); // s == 0
    boundary = proof.signature;
    std::copy(n, n + 32, boundary.begin());
    TEST_ASSERT_FALSE(actual.verify(key, transcript, boundary)); // r == order
    boundary = proof.signature;
    std::copy(n, n + 32, boundary.begin() + 32);
    TEST_ASSERT_FALSE(actual.verify(key, transcript, boundary)); // s == order
    int borrow = 0;
    for (int i = 31; i >= 0; --i) {
        int value = int(n[i]) - int(high[32 + i]) - borrow;
        high[32 + i] = std::uint8_t(value);
        borrow = value < 0;
    }
    TEST_ASSERT_FALSE(actual.verify(key, transcript, high));
}
void test_peer_actual_crypto_owner_frames_explicit_role_and_application() {
    Esp32PeerProofCrypto actual;
    PeerFixture peer(&actual);
    const auto challenge = peer.issue();
    PeerTranscript bytes;
    TEST_ASSERT_TRUE(encodePeerChallenge(challenge, bytes));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, bytes.data(), 108);
    PeerProofReceive result;
    FreeRtosPeerProofQueue authFrames(peer.context, peer.clock);
    TEST_ASSERT_TRUE(authFrames.begin());
    for (unsigned offset = 0; offset < 72; offset += 12) {
        const auto frame = peer.frame(offset);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofCopy::Copied),
                          static_cast<unsigned>(authFrames.copyWrite(
                              peer.context.binding, frame.data(), frame.size(), f->now)));
        const auto dispatched = dispatchPeerProofFrame(authFrames, peer.receiver);
        TEST_ASSERT_TRUE(dispatched.received);
        result = dispatched.result;
    }
    peerOutcome(PeerProofOutcome::Verified, result.proof);
    VerifiedPeerProof verified;
    TEST_ASSERT_TRUE(peer.owner.readVerified(peer.context.binding, verified));
    TrustedAuthority closed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(closed));
    TEST_ASSERT_FALSE(closed.gameMaster);
    OriginalPeerSecurity MODELED;
    MODELED.original = peer.context.binding;
    MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
    VerifiedPeerEvidence evidence;
    TEST_ASSERT_TRUE(bindPeerProofEvidence(verified, MODELED, evidence));
    accessOutcome(IdentityAccessOutcome::Verified,
                  peer.access.authenticate(peer.context.binding.connection, evidence));
    TrustedAuthority route;
    TEST_ASSERT_FALSE(peer.access.route(peer.context.binding.connection, route));
    accessOutcome(IdentityAccessOutcome::Assigned,
                  peer.access.assignGameMaster(verified.credential.peer, 7));
    TEST_ASSERT_TRUE(peer.access.route(peer.context.binding.connection, route));
    f->control.service(++f->now);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin());
    CommandStream stream(f->channel.actual);
    const auto command = json(f->command());
    unsigned frames = 0;
    for (std::size_t offset = 0; offset < command.size();) {
        const auto count = std::min<std::size_t>(12, command.size() - offset);
        const auto v = frameBytes(command, offset, count);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameCopyOutcome::Copied),
                          static_cast<unsigned>(input.copyWrite(v.data(), v.size(), route.principal,
                                                                route.epoch, f->now)));
        TEST_ASSERT_TRUE(dispatchCommandFrame(input, stream, f->now).received);
        offset += count;
        ++frames;
    }
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    const std::string observation =
        "{\"scope\":\"host_actual_peer_crypto\",\"challengeBytes\":108,\"proofBytes\":72,"
        "\"challengeFrames20\":9,\"proofFrames20\":6,\"queueCopies\":6,\"keyProofVerified\":true,"
        "\"keyProofAloneGrantsGM\":false,\"explicitRoleAssigned\":true,\"commandFrames\":" +
        std::to_string(frames) +
        ",\"running\":true,\"transportSecurityModeled\":true,\"rfInitialized\":false,"
        "\"deviceMeasured\":false}\n";
    if (std::getenv("LARP_EXPORT_PEER_PROOF_FIXTURE")) {
        auto *file =
            std::fopen("docs/companion/fixtures/peer-proof/verification-observation.json", "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(observation.size(),
                          std::fwrite(observation.data(), 1, observation.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file =
        std::fopen("docs/companion/fixtures/peer-proof/verification-observation.json", "rb");
    TEST_ASSERT_NOT_NULL(file);
    char read[1024];
    const auto size = std::fread(read, 1, sizeof(read), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(observation.c_str(), std::string(read, size).c_str());
}
#endif
