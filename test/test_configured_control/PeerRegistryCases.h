#pragma once
#include "PeerRegistryVectors.h"
void registryOutcome(PeerRegistryOutcome expected, PeerRegistryOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void registryBootModel() {
    // TEST ONLY simulated fresh boot, not a production fence-reset API.
    host_peer_nvs::reset();
    host_identity_nvs::reset();
    peerNamespaceBlocked = false;
    peerNamespaceMutex = nullptr;
    peerNamespaceMutexStorage = {};
}
PeerRegistrySnapshot registryValue(std::uint32_t generation = 1, bool gm = false) {
    PeerRegistrySnapshot value;
    value.device = testIdentity();
    value.generation = generation;
    value.count = 1;
    auto &entry = value.peers[0];
    entry.credential.peer = testIdentity(2);
    entry.credential.generation = 1;
    std::copy(peerTestPublicKey, peerTestPublicKey + 65, entry.credential.key.begin());
    entry.gameMaster = gm;
    entry.principal = gm ? 7 : 0;
    return value;
}
PeerRegistryRecord registryRecord(const PeerRegistrySnapshot &value) {
    PeerRegistryRecord record;
    TEST_ASSERT_TRUE(encodePeerRegistry(value, record));
    return record;
}
void registryRecrc(PeerRegistryRecord &record) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 400; ++i)
        crc = profileSourceCrcByte(crc, record.bytes[i]);
    crc ^= 0xffffffffu;
    for (unsigned i = 0; i < 4; ++i)
        record.bytes[400 + i] = std::uint8_t(crc >> (8 * i));
}
struct RegistryFixture {
    IdentityAccess access{f->channel.actual};
    PeerRegistryRuntime runtime{access};
    Esp32PeerRegistryStore store;
    PeerCryptoModel model;
    PeerRegistryOwner owner;
    explicit RegistryFixture(IPeerProofCrypto *crypto = nullptr)
        : owner(store, crypto ? *crypto : model, runtime) {
        registryBootModel();
        accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
        TEST_ASSERT_TRUE(store.begin());
        registryOutcome(PeerRegistryOutcome::Empty, owner.begin(testIdentity()));
    }
};
class RegistryProofContext : public IPeerProofContext {
  public:
    RegistryProofContext(PeerRegistryOwner &registry, IdentityAccess &access)
        : registry_(registry), access_(access) {}
    PeerProofBinding original;
    bool ready = true;
    bool originalLive(const PeerProofBinding &value) const noexcept override {
        return ready && value == original && access_.proofConnectionCurrent(value.connection);
    }
    bool credentialCurrent(const PeerCredential &value) const noexcept override {
        return registry_.credentialCurrent(value);
    }

  private:
    PeerRegistryOwner &registry_;
    IdentityAccess &access_;
};
void test_peer_registry_canonical_record_crc_sorted_unique_and_target_preservation() {
    const auto selected = registryValue();
    const auto record = registryRecord(selected);
    TEST_ASSERT_EQUAL(404, sizeof(record));
    PeerRegistrySnapshot output;
    TEST_ASSERT_TRUE(decodePeerRegistry(record, output));
    TEST_ASSERT_TRUE(output == selected);
    for (unsigned i = 0; i < 404; ++i) {
        auto wrong = record;
        wrong.bytes[i] ^= 1;
        output = selected;
        TEST_ASSERT_FALSE(decodePeerRegistry(wrong, output));
        TEST_ASSERT_TRUE(output == selected);
    }
    for (unsigned at : {6u, 7u, 28u, 29u, 30u, 31u, 121u, 122u, 123u, 124u, 399u}) {
        auto wrong = record;
        wrong.bytes[at] = 3;
        registryRecrc(wrong);
        output = selected;
        TEST_ASSERT_FALSE(decodePeerRegistry(wrong, output));
        TEST_ASSERT_TRUE(output == selected);
    }
    for (unsigned variant = 0; variant < 9; ++variant) {
        auto wrong = selected;
        if (variant == 0)
            wrong.device = {};
        if (variant == 1)
            wrong.generation = 0;
        if (variant == 2)
            wrong.count = 5;
        if (variant == 3)
            wrong.peers[0].credential.peer = {};
        if (variant == 4)
            wrong.peers[0].credential.generation = 0;
        if (variant == 5)
            wrong.peers[0].credential.key[0] = 2;
        if (variant == 6)
            wrong.peers[0].principal = 7;
        if (variant == 7)
            wrong.peers[0].gameMaster = true;
        if (variant == 8)
            wrong.peers[1] = wrong.peers[0];
        auto target = record;
        TEST_ASSERT_FALSE(encodePeerRegistry(wrong, target));
        TEST_ASSERT_TRUE(target == record);
    }
}
void test_peer_registry_two_peers_duplicate_keys_principals_labels_and_order() {
    auto value = registryValue(1, true);
    value.count = 2;
    value.peers[1] = value.peers[0];
    value.peers[1].credential.peer = testIdentity(3);
    std::copy(peerRegistrySecondKey, peerRegistrySecondKey + 65,
              value.peers[1].credential.key.begin());
    value.peers[1].principal = 8;
    TEST_ASSERT_TRUE(validPeerRegistry(value));
    for (unsigned variant = 0; variant < 4; ++variant) {
        auto wrong = value;
        if (!variant)
            wrong.peers[1].credential.key = wrong.peers[0].credential.key;
        if (variant == 1)
            wrong.peers[1].principal = 7;
        if (variant == 2)
            wrong.peers[1].credential.peer = wrong.peers[0].credential.peer;
        if (variant == 3)
            std::swap(wrong.peers[0], wrong.peers[1]);
        TEST_ASSERT_FALSE(validPeerRegistry(wrong));
    }
}
void test_peer_registry_missing_then_key_only_and_explicit_role_persist_without_opening() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(0, registryValue()));
    PeerCredential credential;
    TEST_ASSERT_TRUE(registry.owner.credential(testIdentity(2), credential));
    auto gm = registryValue(2, true);
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(1, gm));
    TrustedAuthority route;
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(route));
    Esp32PeerRegistryStore second;
    TEST_ASSERT_TRUE(second.begin());
    PeerRegistryOwner restored(second, registry.model, registry.runtime);
    registryOutcome(PeerRegistryOutcome::Loaded, restored.begin(testIdentity()));
    PeerRegistrySnapshot snapshot;
    TEST_ASSERT_TRUE(restored.snapshot(snapshot));
    TEST_ASSERT_TRUE(snapshot == gm);
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
    TEST_ASSERT_FALSE(registry.access.route(link, route));
    TEST_ASSERT_TRUE(registry.access.proofConnectionCurrent(link));
    TEST_ASSERT_EQUAL(2, host_peer_nvs::sets);
    TEST_ASSERT_EQUAL(2, host_peer_nvs::commits);
    TEST_ASSERT_EQUAL(0, host_peer_nvs::unknown);
    TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
    TEST_ASSERT_EQUAL(0, host_identity_nvs::sets);
}
void test_peer_registry_role_change_closes_original_proof_and_keeps_running_game() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(0, registryValue(1, true)));
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
    VerifiedPeerEvidence MODELED;
    MODELED.peer = testIdentity(2);
    MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
    accessOutcome(IdentityAccessOutcome::Verified, registry.access.authenticate(link, MODELED));
    TrustedAuthority route;
    TEST_ASSERT_TRUE(registry.access.route(link, route));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(1, registryValue(2, false)));
    TEST_ASSERT_FALSE(registry.access.proofConnectionCurrent(link));
    TEST_ASSERT_FALSE(registry.access.route(link, route));
    accessOutcome(IdentityAccessOutcome::SecurityRejected,
                  registry.access.authenticate(link, MODELED));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_peer_registry_rotation_generation_and_old_credential_revocation() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(0, registryValue()));
    auto rotated = registryValue(2);
    rotated.peers[0].credential.generation = 2;
    std::copy(peerRegistrySecondKey, peerRegistrySecondKey + 65,
              rotated.peers[0].credential.key.begin());
    auto invalid = rotated;
    invalid.peers[0].credential.generation = 1;
    registryOutcome(PeerRegistryOutcome::Invalid, registry.owner.replaceTrusted(1, invalid));
    PeerCredential old;
    TEST_ASSERT_TRUE(registry.owner.credential(testIdentity(2), old));
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(1, rotated));
    TEST_ASSERT_FALSE(registry.owner.credentialCurrent(old));
    auto inconsistent = rotated;
    inconsistent.generation = 3;
    inconsistent.peers[0].credential.generation = 3;
    registryOutcome(PeerRegistryOutcome::Invalid, registry.owner.replaceTrusted(2, inconsistent));
    TEST_ASSERT_EQUAL(2, host_peer_nvs::sets);
}
void test_peer_registry_stale_duplicate_invalid_edits_do_not_write_or_close_live_link() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(0, registryValue()));
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
    registryOutcome(PeerRegistryOutcome::Stale, registry.owner.replaceTrusted(0, registryValue(2)));
    registryOutcome(PeerRegistryOutcome::Duplicate,
                    registry.owner.replaceTrusted(1, registryValue(2)));
    auto wrong = registryValue(2, true);
    wrong.device = testIdentity(8);
    registryOutcome(PeerRegistryOutcome::Invalid, registry.owner.replaceTrusted(1, wrong));
    registry.model.key = false;
    registryOutcome(PeerRegistryOutcome::CryptoRejected,
                    registry.owner.replaceTrusted(1, registryValue(2, true)));
    TEST_ASSERT_TRUE(registry.access.proofConnectionCurrent(link));
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
}

void registrySeed(const PeerRegistryRecord &record) {
    host_peer_nvs::namespacePresent = host_peer_nvs::present = true;
    host_peer_nvs::value.assign(record.bytes.begin(), record.bytes.end());
}
void test_peer_registry_restore_corruption_device_and_read_failure_never_restore_roles() {
    for (unsigned variant = 0; variant < 5; ++variant) {
        registryBootModel();
        IdentityAccess access(f->channel.actual);
        PeerRegistryRuntime runtime(access);
        accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
        Esp32PeerRegistryStore store;
        TEST_ASSERT_TRUE(store.begin());
        auto record = registryRecord(registryValue(1, true));
        if (!variant)
            record.bytes[50] ^= 1;
        if (variant == 1) {
            record.bytes[12] = 8;
            registryRecrc(record);
        }
        registrySeed(record);
        if (variant == 2)
            host_peer_nvs::value.resize(403);
        if (variant == 3)
            host_peer_nvs::value.resize(405);
        if (variant == 4)
            host_peer_nvs::getError = 1;
        PeerCryptoModel crypto;
        PeerRegistryOwner owner(store, crypto, runtime);
        registryOutcome(variant < 2 ? PeerRegistryOutcome::Corrupt
                                    : PeerRegistryOutcome::ReadFailure,
                        owner.begin(testIdentity()));
        PeerRegistrySnapshot snapshot = registryValue(9);
        TEST_ASSERT_FALSE(owner.snapshot(snapshot));
        TEST_ASSERT_TRUE(snapshot == registryValue(9));
        TEST_ASSERT_FALSE(store.begin());
        Esp32PeerRegistryStore replacement;
        TEST_ASSERT_FALSE(replacement.begin());
        TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
        TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
        BoundConnection link;
        accessOutcome(IdentityAccessOutcome::Connected, access.connect(257, link));
        VerifiedPeerEvidence MODELED;
        MODELED.peer = testIdentity(2);
        MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
        accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(link, MODELED));
        TrustedAuthority route;
        TEST_ASSERT_FALSE(access.route(link, route));
    }
}
void test_peer_registry_uncertain_set_commit_readback_and_shared_same_boot_fence() {
    for (unsigned variant = 0; variant < 6; ++variant) {
        RegistryFixture registry;
        registryOutcome(PeerRegistryOutcome::Saved,
                        registry.owner.replaceTrusted(0, registryValue(1, true)));
        BoundConnection link;
        accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
        if (variant < 2) {
            host_peer_nvs::setError = 1;
            host_peer_nvs::persistSetError = variant == 1;
        }
        if (variant == 2 || variant == 3) {
            host_peer_nvs::commitError = 1;
            host_peer_nvs::persistCommitError = variant == 3;
        }
        if (variant == 4)
            host_peer_nvs::afterCommit = []() { host_peer_nvs::value[50] ^= 1; };
        if (variant == 5)
            host_peer_nvs::afterCommit = []() { host_peer_nvs::getError = 1; };
        registryOutcome(PeerRegistryOutcome::Uncertain,
                        registry.owner.replaceTrusted(1, registryValue(2, false)));
        TEST_ASSERT_FALSE(registry.access.proofConnectionCurrent(link));
        PeerCredential out = registryValue().peers[0].credential;
        TEST_ASSERT_FALSE(registry.owner.credential(testIdentity(2), out));
        registryOutcome(PeerRegistryOutcome::NotReady,
                        registry.owner.replaceTrusted(1, registryValue(2)));
        Esp32PeerRegistryStore replacement;
        TEST_ASSERT_FALSE(replacement.begin());
        PeerRegistryOwner fresh(replacement, registry.model, registry.runtime);
        registryOutcome(PeerRegistryOutcome::ReadFailure, fresh.begin(testIdentity()));
        TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
    }
}
void test_peer_registry_compare_entire_record_conflict_never_overwrites_other_owner() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(0, registryValue()));
    auto foreign = registryRecord(registryValue(2, true));
    registrySeed(foreign);
    const auto saved = host_peer_nvs::value;
    registryOutcome(PeerRegistryOutcome::WriteConflict,
                    registry.owner.replaceTrusted(1, registryValue(2, true)));
    TEST_ASSERT_TRUE(saved == host_peer_nvs::value);
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
    TEST_ASSERT_FALSE(registry.store.begin());
}
void test_peer_registry_record_freeze_busy_reentry_and_closed_roles_before_nvs_write() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(0, registryValue(1, true)));
    static PeerRegistrySnapshot input;
    input = registryValue(2, false);
    const auto frozen = input;
    static RegistryFixture *observed;
    observed = &registry;
    static BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
    host_peer_nvs::beforeSet = []() {
        input = registryValue(10, true);
        TEST_ASSERT_FALSE(observed->access.proofConnectionCurrent(link));
        PeerRegistrySnapshot destination = registryValue(9);
        TEST_ASSERT_FALSE(observed->owner.snapshot(destination));
        TEST_ASSERT_TRUE(destination == registryValue(9));
        registryOutcome(PeerRegistryOutcome::Busy, observed->owner.begin(testIdentity()));
        registryOutcome(PeerRegistryOutcome::Busy, observed->owner.replaceTrusted(1, input));
        Esp32PeerRegistryStore other;
        TEST_ASSERT_FALSE(other.begin());
    };
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(1, input));
    PeerRegistrySnapshot actual;
    TEST_ASSERT_TRUE(registry.owner.snapshot(actual));
    TEST_ASSERT_TRUE(actual == frozen);
    TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
}
void test_peer_registry_init_busy_invalid_direct_write_and_fenced_read_preserve_targets() {
    registryBootModel();
    Esp32PeerRegistryStore store;
    auto out = registryRecord(registryValue(9));
    const auto original = out;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRead::Failure),
                      static_cast<unsigned>(store.read(out)));
    TEST_ASSERT_TRUE(out == original);
    host_identity_nvs::initError = 1;
    TEST_ASSERT_FALSE(store.begin());
    host_identity_nvs::initError = 0;
    TEST_ASSERT_TRUE(store.begin());
    auto invalid = original;
    invalid.bytes[0] = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryWrite::Failure),
                      static_cast<unsigned>(store.replace(nullptr, invalid)));
    TEST_ASSERT_EQUAL(0, host_peer_nvs::opens);
    host_mutex::forceBusy = true;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRead::Failure),
                      static_cast<unsigned>(store.read(out)));
    host_mutex::forceBusy = false;
    registrySeed(registryRecord(registryValue()));
    static Esp32PeerRegistryStore *observed;
    observed = &store;
    host_peer_nvs::beforeGet = []() { observed->fence(); };
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRead::Failure),
                      static_cast<unsigned>(store.read(out)));
    TEST_ASSERT_TRUE(out == original);
    TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
}

void test_peer_registry_counter_exhaustion_and_repeat_begin_cannot_reset_fence() {
    RegistryFixture registry;
    auto last = registryValue(UINT32_MAX, true);
    registrySeed(registryRecord(last));
    Esp32PeerRegistryStore second;
    TEST_ASSERT_TRUE(second.begin());
    PeerRegistryOwner owner(second, registry.model, registry.runtime);
    registryOutcome(PeerRegistryOutcome::Loaded, owner.begin(testIdentity()));
    registryOutcome(PeerRegistryOutcome::CounterExhausted,
                    owner.replaceTrusted(UINT32_MAX, registryValue()));
    registryOutcome(PeerRegistryOutcome::Invalid, owner.begin(testIdentity(9)));
    registryOutcome(PeerRegistryOutcome::Invalid, owner.begin(testIdentity()));
    TEST_ASSERT_FALSE(second.begin());
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
}
void test_peer_registry_closed_assignment_replacement_preserves_new_authority() {
    RegistryFixture registry;
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, registry.access.connect(257, link));
    std::array<GameMasterAssignment, 4> invalid = {};
    invalid[0].principal = 7;
    accessOutcome(IdentityAccessOutcome::InvalidAssignment,
                  registry.access.replaceGameMastersClosed(invalid));
    TEST_ASSERT_TRUE(registry.access.proofConnectionCurrent(link));
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.advanceAuthority(99, true, foreign)));
    registryOutcome(PeerRegistryOutcome::RuntimeRejected,
                    registry.owner.replaceTrusted(0, registryValue(1, true)));
    TrustedAuthority after;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(after));
    TEST_ASSERT_TRUE(after == foreign);
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
    TEST_ASSERT_FALSE(registry.access.proofConnectionCurrent(link));
}
void test_peer_registry_revoke_during_proof_crypto_cannot_publish_old_verified_peer() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(0, registryValue(1, true)));
    RegistryProofContext context(registry.owner, registry.access);
    context.original.device = testIdentity();
    context.original.boot = f->boot;
    context.original.registration = 0x01020304;
    context.original.interface = 1;
    accessOutcome(IdentityAccessOutcome::Connected,
                  registry.access.connect(257, context.original.connection));
    PeerClock clock;
    PeerNonceModel nonce;
    PeerProofOwner proof(context, registry.model, nonce, clock);
    PeerCredential credential;
    TEST_ASSERT_TRUE(registry.owner.credential(testIdentity(2), credential));
    peerOutcome(PeerProofOutcome::Issued, proof.issue(context.original, credential));
    static RegistryFixture *observed;
    observed = &registry;
    registry.model.before = []() {
        registryOutcome(PeerRegistryOutcome::Saved,
                        observed->owner.replaceTrusted(1, registryValue(2, false)));
    };
    PeerProof signature;
    TEST_ASSERT_TRUE(decodePeerProof(peerTestProofBytes, 72, signature));
    peerOutcome(PeerProofOutcome::Stale, proof.verify(context.original, signature));
    VerifiedPeerProof out;
    TEST_ASSERT_FALSE(proof.readVerified(context.original, out));
    TEST_ASSERT_EQUAL(1, registry.model.proofs);
}

unsigned registryVerifyGets = 0;
void registryFailOwnerRead() {
    if (++registryVerifyGets == 3)
        host_peer_nvs::getError = 1;
    else
        host_peer_nvs::beforeGet = registryFailOwnerRead;
}
void test_peer_registry_owner_separate_readback_failure_and_confirmed_revoke_corruption() {
    RegistryFixture registry;
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(0, registryValue(1, true)));
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(1, registryValue(2, false)));
    auto changed = registryValue(3, false);
    changed.peers[0].credential.generation = 2;
    std::copy(peerRegistrySecondKey, peerRegistrySecondKey + 65,
              changed.peers[0].credential.key.begin());
    host_peer_nvs::afterCommit = []() {
        registryVerifyGets = 0;
        host_peer_nvs::beforeGet = registryFailOwnerRead;
    };
    registryOutcome(PeerRegistryOutcome::VerifyFailure, registry.owner.replaceTrusted(2, changed));
    TEST_ASSERT_FALSE(registry.store.begin());
    host_peer_nvs::getError = 0;
    PeerRegistrySnapshot persisted;
    PeerRegistryRecord raw;
    std::copy(host_peer_nvs::value.begin(), host_peer_nvs::value.end(), raw.bytes.begin());
    TEST_ASSERT_TRUE(decodePeerRegistry(raw, persisted));
    TEST_ASSERT_TRUE(persisted == changed);
    // Test-only new boot: the confirmed current revoked blob is then corrupt.
    registryBootModel();
    raw = registryRecord(registryValue(2, false));
    raw.bytes[50] ^= 1;
    registrySeed(raw);
    IdentityAccess access(f->channel.actual);
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    PeerRegistryRuntime runtime(access);
    Esp32PeerRegistryStore store;
    TEST_ASSERT_TRUE(store.begin());
    PeerCryptoModel crypto;
    PeerRegistryOwner restored(store, crypto, runtime);
    registryOutcome(PeerRegistryOutcome::Corrupt, restored.begin(testIdentity()));
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
    TEST_ASSERT_FALSE(store.begin());
}
#if defined(LARP_REAL_PEER_CRYPTO)
void test_peer_registry_actual_curve_restore_and_invalid_point_rejection() {
    registryBootModel();
    IdentityAccess access(f->channel.actual);
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    PeerRegistryRuntime runtime(access);
    Esp32PeerRegistryStore store;
    TEST_ASSERT_TRUE(store.begin());
    auto invalid = registryValue(1, true);
    invalid.peers[0].credential.key[1] ^= 1;
    registrySeed(registryRecord(invalid));
    Esp32PeerProofCrypto crypto;
    PeerPublicKey second;
    std::copy(peerRegistrySecondKey, peerRegistrySecondKey + 65, second.begin());
    TEST_ASSERT_TRUE(crypto.validKey(second));
    PeerRegistryOwner owner(store, crypto, runtime);
    registryOutcome(PeerRegistryOutcome::CryptoRejected, owner.begin(testIdentity()));
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
}
void registryExport(const char *name, const PeerRegistryRecord &record) {
    char hex[809];
    static const char digits[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < 404; ++i) {
        hex[2 * i] = digits[record.bytes[i] >> 4];
        hex[2 * i + 1] = digits[record.bytes[i] & 15];
    }
    hex[808] = 0;
    const std::string path = std::string("docs/companion/fixtures/peer-registry/") + name + ".hex";
    const std::string contents = std::string(hex) + "\n";
    if (std::getenv("LARP_EXPORT_PEER_REGISTRY_FIXTURES")) {
        auto *file = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(contents.size(), std::fwrite(contents.data(), 1, contents.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file = std::fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL(file);
    char buffer[810];
    const auto count = std::fread(buffer, 1, sizeof(buffer), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(contents.c_str(), std::string(buffer, count).c_str());
}
void test_peer_registry_actual_crypto_restored_explicit_role_frames_game_and_revocation() {
    Esp32PeerProofCrypto crypto;
    RegistryFixture registry(&crypto);
    registryOutcome(PeerRegistryOutcome::Saved, registry.owner.replaceTrusted(0, registryValue()));
    registryExport("key-only", registryRecord(registryValue()));
    registryOutcome(PeerRegistryOutcome::Saved,
                    registry.owner.replaceTrusted(1, registryValue(2, true)));
    registryExport("game-master", registryRecord(registryValue(2, true)));
    Esp32PeerRegistryStore restoredStore;
    TEST_ASSERT_TRUE(restoredStore.begin());
    PeerRegistryOwner restored(restoredStore, crypto, registry.runtime);
    registryOutcome(PeerRegistryOutcome::Loaded, restored.begin(testIdentity()));
    RegistryProofContext context(restored, registry.access);
    context.original.device = testIdentity();
    context.original.boot = f->boot;
    context.original.registration = 0x01020304;
    context.original.interface = 1;
    accessOutcome(IdentityAccessOutcome::Connected,
                  registry.access.connect(257, context.original.connection));
    PeerClock clock;
    PeerNonceModel nonce;
    PeerProofOwner proof(context, crypto, nonce, clock);
    PeerCredential credential;
    TEST_ASSERT_TRUE(restored.credential(testIdentity(2), credential));
    peerOutcome(PeerProofOutcome::Issued, proof.issue(context.original, credential));
    PeerChallenge challenge;
    TEST_ASSERT_TRUE(proof.challenge(context.original, challenge));
    PeerTranscript bytes;
    TEST_ASSERT_TRUE(encodePeerChallenge(challenge, bytes));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, bytes.data(), 108);
    PeerProofReceiver receiver(proof);
    FreeRtosPeerProofQueue authFrames(context, clock);
    TEST_ASSERT_TRUE(authFrames.begin());
    PeerProofReceive result;
    for (unsigned offset = 0; offset < 72; offset += 12) {
        char frame[20];
        std::size_t count = 0;
        TEST_ASSERT_TRUE(encodePeerProofFrame(
            1, 72, offset, reinterpret_cast<const char *>(peerTestProofBytes) + offset, 12, frame,
            sizeof(frame), count));
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(PeerProofCopy::Copied),
            static_cast<unsigned>(authFrames.copyWrite(context.original, frame, count, f->now)));
        const auto dispatched = dispatchPeerProofFrame(authFrames, receiver);
        TEST_ASSERT_TRUE(dispatched.received);
        result = dispatched.result;
    }
    peerOutcome(PeerProofOutcome::Verified, result.proof);
    VerifiedPeerProof verified;
    TEST_ASSERT_TRUE(proof.readVerified(context.original, verified));
    TrustedAuthority route;
    TEST_ASSERT_FALSE(registry.access.route(context.original.connection, route));
    OriginalPeerSecurity MODELED;
    MODELED.original = context.original;
    MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
    VerifiedPeerEvidence evidence;
    TEST_ASSERT_TRUE(bindPeerProofEvidence(verified, MODELED, evidence));
    accessOutcome(IdentityAccessOutcome::Verified,
                  registry.access.authenticate(context.original.connection, evidence));
    TEST_ASSERT_TRUE(registry.access.route(context.original.connection, route));
    f->control.service(++f->now);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(commands.begin());
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    unsigned frames = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        const auto frame = frameBytes(document, offset, count);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameCopyOutcome::Copied),
                          static_cast<unsigned>(commands.copyWrite(
                              frame.data(), frame.size(), route.principal, route.epoch, f->now)));
        TEST_ASSERT_TRUE(dispatchCommandFrame(commands, stream, f->now).received);
        offset += count;
        ++frames;
    }
    TEST_ASSERT_EQUAL(13, frames);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    registryOutcome(PeerRegistryOutcome::Saved,
                    restored.replaceTrusted(2, registryValue(3, false)));
    registryExport("revoked", registryRecord(registryValue(3, false)));
    TEST_ASSERT_FALSE(proof.readVerified(context.original, verified));
    TEST_ASSERT_FALSE(registry.access.route(context.original.connection, route));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL(3, host_peer_nvs::sets);
    TEST_ASSERT_EQUAL(3, host_peer_nvs::commits);
    const std::string observation =
        "{\"scope\":\"host_actual_crypto_nvs_model\",\"recordBytes\":404,\"peers\":1,\"setCalls\":"
        "3,\"commitCalls\":3,\"restoredExplicitRole\":true,\"proofFrames\":6,\"commandFrames\":13,"
        "\"runningAfterRevocation\":true,\"oldProofRejected\":true,\"transportSecurityModeled\":"
        "true,\"physicalDurabilityMeasured\":false,\"rfInitialized\":false}\n";
    const char *path = "docs/companion/fixtures/peer-registry/observation.json";
    if (std::getenv("LARP_EXPORT_PEER_REGISTRY_FIXTURES")) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(observation.size(),
                          std::fwrite(observation.data(), 1, observation.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char read[1024];
    const auto count = std::fread(read, 1, sizeof(read), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(observation.c_str(), std::string(read, count).c_str());
}
#endif
