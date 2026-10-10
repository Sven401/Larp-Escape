#pragma once
#include "PeerProofCases.h"
#include "PeerRegistryActorCases.h"
PeerTransportBinding securityTransport() {
    PeerTransportBinding value;
    value.device = testIdentity();
    value.connection.handle = 257;
    value.connection.token = 1;
    value.registration = 1;
    value.lifecycle = 2;
    value.interface = 1;
    value.address = {{1, 2, 3, 4, 5, 6}};
    return value;
}
PeerProofBinding securityProof(const PeerTransportBinding &transport) {
    PeerProofBinding proof;
    proof.device = transport.device;
    proof.boot = f->boot;
    proof.connection = transport.connection;
    proof.registration = transport.registration;
    proof.interface = transport.interface;
    return proof;
}
PeerPairingResult securityResult() {
    PeerPairingResult value;
    value.reason = 0;
    value.securityLevel = 4;
    value.mode = 0x0d;
    return value;
}
void pairingOutcome(PeerPairingOutcome expected, PeerPairingOutcome observed) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(observed));
}
void test_security_core_requires_original_start_and_all_resolved_mode_bits() {
    const auto original = securityTransport();
    const auto proof = securityProof(original);
    for (unsigned mode = 0; mode < 256; ++mode) {
        PeerLinkSecurity owner;
        auto result = securityResult();
        result.mode = static_cast<std::uint8_t>(mode);
        pairingOutcome(PeerPairingOutcome::Mismatch, owner.complete(original, result, 1));
        pairingOutcome(PeerPairingOutcome::Started, owner.start(original, 2));
        const bool valid = mode == 13 || mode == 29;
        pairingOutcome(valid ? PeerPairingOutcome::Completed : PeerPairingOutcome::Failed,
                       owner.complete(original, result, 3));
        OriginalPeerSecurity out;
        out.original = proof;
        TEST_ASSERT_EQUAL(valid, owner.select(original, proof, PeerLinkState{0x16, 16}, out));
        if (valid)
            TEST_ASSERT_TRUE(out.encrypted && out.mitm && out.secureConnections && out.bonded);
    }
}
void test_security_core_rejects_cancel_failure_br_and_weak_link_state() {
    const auto original = securityTransport();
    const auto proof = securityProof(original);
    for (unsigned variant = 0; variant < 5; ++variant) {
        PeerLinkSecurity owner;
        auto result = securityResult();
        if (variant == 0)
            result.reason = 1;
        if (variant == 1)
            result.securityLevel = 1;
        if (variant == 2)
            result.cancelled = 1;
        if (variant == 3)
            result.overBr = 1;
        if (variant == 4)
            result.cancelled = 255;
        pairingOutcome(PeerPairingOutcome::Started, owner.start(original, 0));
        pairingOutcome(PeerPairingOutcome::Failed, owner.complete(original, result, 1));
        OriginalPeerSecurity out;
        TEST_ASSERT_FALSE(owner.select(original, proof, PeerLinkState{0x16, 16}, out));
    }
    PeerLinkSecurity owner;
    owner.start(original, 0);
    owner.complete(original, securityResult(), 1);
    for (unsigned flags = 0; flags < 256; ++flags)
        for (unsigned key = 0; key <= 17; ++key) {
            OriginalPeerSecurity out;
            const bool valid = (flags & 0x16) == 0x16 && key == 16;
            TEST_ASSERT_EQUAL(valid, owner.select(original, proof,
                                                  PeerLinkState{static_cast<std::uint8_t>(flags),
                                                                static_cast<std::uint8_t>(key)},
                                                  out));
        }
}
void test_security_core_deadline_wrap_duplicate_start_does_not_refresh() {
    const auto original = securityTransport();
    PeerLinkSecurity owner;
    const auto start = UINT32_MAX - 100;
    pairingOutcome(PeerPairingOutcome::Started, owner.start(original, start));
    pairingOutcome(PeerPairingOutcome::Duplicate, owner.start(original, start + 29999));
    pairingOutcome(PeerPairingOutcome::Expired,
                   owner.complete(original, securityResult(), start + 30000));
    OriginalPeerSecurity out;
    TEST_ASSERT_FALSE(
        owner.select(original, securityProof(original), PeerLinkState{0x16, 16}, out));
}
void test_security_core_abandoned_attempt_cannot_attach_to_recycled_connection() {
    auto old = securityTransport();
    auto fresh = old;
    ++fresh.lifecycle;
    ++fresh.connection.token;
    PeerLinkSecurity owner;
    owner.start(old, 0);
    pairingOutcome(PeerPairingOutcome::Mismatch, owner.start(fresh, 1));
    owner.close();
    pairingOutcome(PeerPairingOutcome::Mismatch, owner.start(fresh, 1));
    auto wrong = old.address;
    wrong[0]++;
    TEST_ASSERT_FALSE(owner.discardAbandonedCompletion(wrong));
    pairingOutcome(PeerPairingOutcome::Mismatch, owner.start(fresh, 2));
    TEST_ASSERT_TRUE(owner.discardAbandonedCompletion(old.address));
    pairingOutcome(PeerPairingOutcome::Started, owner.start(fresh, 3));
    pairingOutcome(PeerPairingOutcome::Mismatch, owner.complete(old, securityResult(), 4));
    pairingOutcome(PeerPairingOutcome::Completed, owner.complete(fresh, securityResult(), 5));
    OriginalPeerSecurity out;
    TEST_ASSERT_TRUE(owner.select(fresh, securityProof(fresh), PeerLinkState{0x16, 16}, out));
    pairingOutcome(PeerPairingOutcome::Mismatch, owner.start(old, 6));
    TEST_ASSERT_TRUE(owner.select(fresh, securityProof(fresh), PeerLinkState{0x16, 16}, out));
}
void test_security_core_repair_invalidates_previous_mode_until_terminal_drain() {
    const auto original = securityTransport();
    PeerLinkSecurity owner;
    owner.start(original, 0);
    owner.complete(original, securityResult(), 1);
    pairingOutcome(PeerPairingOutcome::Duplicate, owner.complete(original, securityResult(), 2));
    pairingOutcome(PeerPairingOutcome::Consumed, owner.start(original, 3));
    OriginalPeerSecurity out;
    TEST_ASSERT_FALSE(
        owner.select(original, securityProof(original), PeerLinkState{0x16, 16}, out));
    owner.close();
    TEST_ASSERT_TRUE(owner.discardAbandonedCompletion(original.address));
    pairingOutcome(PeerPairingOutcome::Consumed, owner.start(original, 4));
}
struct SecurityFixture : RegistryActorFixture {
    PeerProofBinding original;
    explicit SecurityFixture(bool openLink = true, bool gm = true,
                             IPeerProofCrypto *actual = nullptr, std::uint32_t registration = 0)
        : RegistryActorFixture(actual, registration, true) {
        ready();
        save(registryValue(1, gm));
        GattsSourceRegistration reg;
        TEST_ASSERT_TRUE(source.readRegistration(reg));
        original.device = testIdentity();
        original.boot = f->boot;
        original.registration = reg.token;
        original.interface = reg.interface;
        if (openLink)
            original.connection = open();
        actor();
    }
    void event(std::uint8_t event, PeerPairingResult result = securityResult()) {
        sdk446::SmpData data = {};
        const std::uint8_t bytes[5] = {result.reason, result.securityLevel, result.cancelled,
                                       result.overBr, result.mode};
        std::memcpy(data.complete, bytes, 5);
        TEST_ASSERT_EQUAL(23, host_sdk_security::callback(event, address, &data));
        TEST_ASSERT_EQUAL_PTR(address, host_sdk_security::lastAddress);
        TEST_ASSERT_EQUAL_PTR(&data, host_sdk_security::lastData);
        TEST_ASSERT_EQUAL(event, host_sdk_security::lastEvent);
    }
    void pair() {
        event(2);
        event(1);
        event(7);
    }
    bool read() {
        OriginalPeerSecurity out;
        return source.readOriginalPeerSecurityNow(original, out);
    }
    void grantModeledProof() {
        OriginalPeerSecurity security;
        TEST_ASSERT_TRUE(source.readOriginalPeerSecurityNow(original, security));
        // Only key possession here is modeled; separate native case uses real P-256.
        VerifiedPeerProof proof;
        proof.original = original;
        proof.credential = registryValue(1, true).peers[0].credential;
        proof.sequence = 1;
        VerifiedPeerEvidence evidence;
        TEST_ASSERT_TRUE(bindPeerProofEvidence(proof, security, evidence));
        accessOutcome(IdentityAccessOutcome::Verified,
                      access.authenticate(original.connection, evidence));
    }
};
void test_security_source_hci_birth_is_current_and_recycled_handle_stays_closed() {
    SecurityFixture fixture;
    TEST_ASSERT_TRUE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x124, 0, 1));
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 1, 1));
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 0));
    fixture.close();
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x123, 0, 1));
    fixture.close();
    host_sdk_security::hci = 0x124;
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.hciEncryptionNow(0x124, 0, 1));
    host_sdk_security::hci = 0x125;
    TEST_ASSERT_FALSE(fixture.source.hciEncryptionNow(0x124, 0, 1));
}
void test_security_source_original_callbacks_forward_without_roles_and_reader_needs_start() {
    SecurityFixture fixture;
    TEST_ASSERT_FALSE(fixture.read());
    fixture.event(7);
    TEST_ASSERT_FALSE(fixture.read());
    fixture.pair();
    TEST_ASSERT_TRUE(fixture.read());
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
    TEST_ASSERT_EQUAL(1, host_sdk_security::registrations);
    TEST_ASSERT_FALSE(__wrap_SMP_Register(host_sdk_security::original));
    TEST_ASSERT_EQUAL(1, host_sdk_security::registrations);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_security_source_sdk_metadata_rejections_preserve_destination() {
    for (unsigned variant = 0; variant < 12; ++variant) {
        SecurityFixture fixture;
        fixture.pair();
        if (variant == 0)
            host_sdk_security::connectionResult = false;
        if (variant == 1)
            host_sdk_security::interface = 2;
        if (variant == 2)
            host_sdk_security::transport = 1;
        if (variant == 3)
            host_sdk_security::address[0]++;
        if (variant == 4)
            host_sdk_security::flagsResult = false;
        if (variant == 5)
            host_sdk_security::flags = 0x14;
        if (variant == 6)
            host_sdk_security::flags = 0x12;
        if (variant == 7)
            host_sdk_security::stateResult = false;
        if (variant == 8)
            host_sdk_security::state = 4; // Paired, but unencrypted.
        if (variant == 9)
            host_sdk_security::keyBytes = 15;
        if (variant == 10)
            host_sdk_security::running = true;
        if (variant == 11)
            host_sdk_security::flags = 6; // Authenticated/encrypted without key known.
        OriginalPeerSecurity out;
        out.original = fixture.original;
        out.mitm = true;
        TEST_ASSERT_FALSE(fixture.source.readOriginalPeerSecurityNow(fixture.original, out));
        TEST_ASSERT_TRUE(out.original == fixture.original && out.mitm && !out.encrypted &&
                         !out.secureConnections && !out.bonded);
        TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    }
}
void test_security_source_wrong_actor_sdk_instance_and_forwarding_cannot_read() {
    SecurityFixture fixture;
    fixture.pair();
    fixture.io();
    TEST_ASSERT_FALSE(fixture.read());
    fixture.actor();
    btu_thread = reinterpret_cast<void *>(9);
    TEST_ASSERT_FALSE(fixture.read());
    btu_thread = reinterpret_cast<void *>(2);
    static SecurityFixture *observed;
    observed = &fixture;
    const auto before = host_sdk_security::connections;
    host_sdk_security::beforeForward = []() { TEST_ASSERT_FALSE(observed->read()); };
    fixture.event(7);
    TEST_ASSERT_EQUAL(before, host_sdk_security::connections);
    TEST_ASSERT_TRUE(fixture.read());
    fixture.io();
    fixture.event(7);
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.read()); // Wrong task passes through only.
}
void test_security_source_sdk_port_input_mutation_stops_before_next_query() {
    SecurityFixture fixture;
    fixture.pair();
    host_sdk_security::beforeFlags = [](std::uint8_t *address) {
        address[0]++;
        host_sdk_security::address[0]++;
    };
    TEST_ASSERT_FALSE(fixture.read());
    TEST_ASSERT_EQUAL(0, host_sdk_security::stateReads);
}
void test_security_source_borrowed_completion_frozen_before_clock_port() {
    SecurityFixture fixture;
    fixture.event(2);
    static sdk446::SmpData data;
    static std::uint8_t address[6];
    std::memcpy(address, fixture.address, 6);
    const std::uint8_t bytes[5] = {0, 4, 0, 0, 13};
    std::memcpy(data.complete, bytes, 5);
    fixture.clock.before = []() {
        std::memset(data.complete, 0, 5);
        address[0] = 99;
    };
    TEST_ASSERT_EQUAL(23, host_sdk_security::callback(7, address, &data));
    TEST_ASSERT_TRUE(fixture.read());
    TEST_ASSERT_EQUAL(99, address[0]);
}
void test_security_source_reconnect_during_sdk_read_preserves_fresh_binding() {
    SecurityFixture fixture;
    fixture.pair();
    static SecurityFixture *observed;
    observed = &fixture;
    host_sdk_security::beforeConnection = []() {
        observed->close();
        observed->original.connection = observed->open();
        observed->actor();
    };
    const auto old = fixture.original;
    OriginalPeerSecurity out;
    TEST_ASSERT_FALSE(fixture.source.readOriginalPeerSecurityNow(old, out));
    TEST_ASSERT_FALSE(fixture.original.connection == old.connection);
    TEST_ASSERT_EQUAL(0, host_sdk_security::flagReads);
    fixture.pair();
    TEST_ASSERT_TRUE(fixture.read());
}
void test_security_source_disconnect_pending_drains_old_completion_before_new_pairing() {
    SecurityFixture fixture;
    fixture.event(2);
    const auto old = fixture.original;
    fixture.close();
    fixture.original.connection = fixture.open();
    fixture.actor();
    fixture.event(2);
    fixture.event(7);
    TEST_ASSERT_FALSE(fixture.read()); // Drain abandoned old completion.
    fixture.pair();
    TEST_ASSERT_TRUE(fixture.read());
    OriginalPeerSecurity out;
    TEST_ASSERT_FALSE(fixture.source.readOriginalPeerSecurityNow(old, out));
}
void test_security_source_failure_and_repair_revoke_exact_owned_gm_without_stopping_game() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        SecurityFixture fixture;
        fixture.pair();
        fixture.grantModeledProof();
        TrustedAuthority route;
        TEST_ASSERT_TRUE(fixture.access.route(fixture.original.connection, route));
        f->control.service(++f->now);
        TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
        f->control.service(++f->now);
        if (!variant) {
            auto result = securityResult();
            result.reason = 1;
            fixture.event(7, result);
        } else
            fixture.event(2);
        TEST_ASSERT_FALSE(fixture.access.route(fixture.original.connection, route));
        TEST_ASSERT_FALSE(fixture.read());
        f->control.service(++f->now);
        TEST_ASSERT_TRUE(f->application.gameRunning());
    }
}
void test_security_source_security_failure_preserves_foreign_publisher() {
    SecurityFixture fixture;
    fixture.pair();
    fixture.grantModeledProof();
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.advanceAuthority(99, true, foreign)));
    auto failure = securityResult();
    failure.reason = 1;
    fixture.event(7, failure);
    TrustedAuthority observed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(observed));
    TEST_ASSERT_TRUE(observed == foreign);
}
void test_security_source_legacy_cancel_timeout_and_restored_bond_cannot_prove_sc() {
    for (unsigned variant = 0; variant < 5; ++variant) {
        SecurityFixture fixture;
        if (variant != 4)
            fixture.event(2);
        auto result = securityResult();
        if (variant == 0)
            result.mode = 5;
        if (variant == 1)
            result.cancelled = 1;
        if (variant == 2) {
            f->now += 29999;
            fixture.event(1);
            ++f->now;
        }
        if (variant == 3)
            result.overBr = 1;
        fixture.event(7, result);
        TEST_ASSERT_FALSE(fixture.read());
    }
}

void test_security_source_stale_proof_binding_never_queries_sdk() {
    SecurityFixture fixture;
    fixture.pair();
    const auto connectionReads = host_sdk_security::connections;
    for (unsigned variant = 0; variant < 7; ++variant) {
        auto wrong = fixture.original;
        if (variant == 0)
            wrong.connection.token++;
        if (variant == 1)
            wrong.connection.handle = 513;
        if (variant == 2)
            wrong.registration++;
        if (variant == 3)
            wrong.interface++;
        if (variant == 4)
            wrong.device = testIdentity(9);
        if (variant == 5)
            wrong.boot = {};
        if (variant == 6)
            wrong.connection = {};
        OriginalPeerSecurity out;
        TEST_ASSERT_FALSE(fixture.source.readOriginalPeerSecurityNow(wrong, out));
    }
    TEST_ASSERT_EQUAL(connectionReads, host_sdk_security::connections);
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_FALSE(fixture.read());
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.read());
    TEST_ASSERT_EQUAL(connectionReads, host_sdk_security::connections);
}
void test_security_source_sdk_registration_failure_is_closed_without_retry() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto connection = fixture.open();
    fixture.actor();
    host_sdk_security::registerResult = false;
    TEST_ASSERT_FALSE(__wrap_SMP_Register(host_sdk_security::original));
    host_sdk_security::registerResult = true;
    TEST_ASSERT_FALSE(__wrap_SMP_Register(host_sdk_security::original));
    TEST_ASSERT_EQUAL(1, host_sdk_security::registrations);
    PeerProofBinding proof;
    proof.device = testIdentity();
    proof.boot = f->boot;
    proof.connection = connection;
    GattsSourceRegistration registration;
    TEST_ASSERT_TRUE(fixture.source.readRegistration(registration));
    proof.registration = registration.token;
    proof.interface = registration.interface;
    OriginalPeerSecurity out;
    TEST_ASSERT_FALSE(fixture.source.readOriginalPeerSecurityNow(proof, out));
    TEST_ASSERT_EQUAL(0, host_sdk_security::connections);
}
