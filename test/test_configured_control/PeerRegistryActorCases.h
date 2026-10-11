#pragma once
#include "PeerRegistryCases.h"
struct RegistryActorFixture {
    IdentityAccess access{f->channel.actual};
    SourceClock clock;
    FreeRtosCommandFrameQueue commands{f->channel.actual};
    FreeRtosProfileFrameQueue profiles{f->channel.actual};
    Esp32GattsSource source{access, clock, f->channel.actual, commands, profiles};
    FreeRtosGattsResponseQueue responses{source};
    FreeRtosPeerRegistryRuntime publication{source, f->channel.actual, clock};
    Esp32PeerRegistryStore store;
    PeerCryptoModel crypto;
    PeerRegistryOwner owner;
    std::uint8_t address[6] = {1, 2, 3, 4, 5, 6};
    explicit RegistryActorFixture(IPeerProofCrypto *actual = nullptr,
                                  std::uint32_t fixtureRegistration = 0, bool installSmp = false)
        : owner(store, actual ? *actual : crypto, publication) {
        host_sdk_source::reset();
        registryBootModel();
        // TEST ONLY fresh SDK boot; never a production stop/restart operation.
        larp::companion::selected = nullptr;
        larp::companion::application = 0;
        larp::companion::registrationAttempted = false;
        larp::companion::originalCallbacks = {};
        larp::companion::smpRegistrationAttempted = larp::companion::smpRegistered = false;
        larp::companion::originalSmpCallback = nullptr;
        host_sdk_security::reset();
        btu_thread = nullptr;
        if (fixtureRegistration)
            larp::companion::registrationCounter = fixtureRegistration - 1; // TEST ONLY boot.
        accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
        TEST_ASSERT_TRUE(commands.begin() && profiles.begin() && responses.begin());
        TEST_ASSERT_TRUE(armGattsSourceBeforeSdkStart(0x4c45, source));
        if (installSmp)
            TEST_ASSERT_TRUE(__wrap_SMP_Register(host_sdk_security::original));
        sdk446::Uuid uuid = {};
        uuid.len = 2;
        uuid.uu.uuid16 = 0x4c45;
        sdk446::Callbacks callbacks = {};
        callbacks.connection = host_sdk_source::passConnection;
        callbacks.request = host_sdk_source::passRequest;
        btu_thread = reinterpret_cast<void *>(2);
        TEST_ASSERT_EQUAL(1, __wrap_GATT_Register(&uuid, &callbacks));
        TEST_ASSERT_TRUE(source.configureEndpointsNow(0x42, 0x43, responses));
        io();
        TEST_ASSERT_TRUE(publication.begin(testIdentity()));
        TEST_ASSERT_TRUE(store.begin());
        registryOutcome(PeerRegistryOutcome::Pending, owner.begin(testIdentity()));
    }
    ~RegistryActorFixture() {
        actor();
        host_sdk_source::drain();
        larp::companion::selected = nullptr;
        larp::companion::registrationAttempted = false;
        larp::companion::originalCallbacks = {};
        larp::companion::smpRegistrationAttempted = larp::companion::smpRegistered = false;
        larp::companion::originalSmpCallback = nullptr;
        host_sdk_security::reset();
    }
    void io() {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(2);
    }
    void actor() {
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(1);
    }
    PeerRegistryOutcome pump() {
        actor();
        host_sdk_source::drainOne();
        io();
        return owner.service();
    }
    void ready() {
        registryOutcome(PeerRegistryOutcome::Pending, pump());
        registryOutcome(PeerRegistryOutcome::Empty, pump());
    }
    void save(const PeerRegistrySnapshot &value) {
        io();
        registryOutcome(PeerRegistryOutcome::Pending,
                        owner.replaceTrusted(value.generation - 1, value));
        registryOutcome(PeerRegistryOutcome::Pending, pump());
        registryOutcome(PeerRegistryOutcome::Saved, pump());
    }
    BoundConnection open() {
        actor();
        host_sdk_source::registeredCallbacks.connection(1, address, 257, true, 0, 2);
        BoundConnection connection;
        TEST_ASSERT_TRUE(source.originalConnectionNow(connection));
        io();
        return connection;
    }
    void close() {
        actor();
        host_sdk_source::registeredCallbacks.connection(1, address, 257, false, 0, 2);
        io();
    }
};
void test_registry_actor_pending_ack_precedes_io_and_snapshot_publication() {
    RegistryActorFixture fixture;
    TEST_ASSERT_EQUAL(0, host_peer_nvs::opens);
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    for (unsigned i = 0; i < 8; ++i)
        registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    TEST_ASSERT_EQUAL(0, host_peer_nvs::opens);
    fixture.ready();
    TEST_ASSERT_EQUAL(2, host_sdk_source::posts);
    PeerRegistrySnapshot snapshot;
    TEST_ASSERT_TRUE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_EQUAL(0, snapshot.generation);
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(0, registryValue(1, true)));
    TEST_ASSERT_FALSE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
    TEST_ASSERT_EQUAL(1, host_peer_nvs::commits);
    TEST_ASSERT_FALSE(fixture.publication.snapshot(snapshot));
    for (unsigned i = 0; i < 8; ++i)
        registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
    TEST_ASSERT_EQUAL(4, host_sdk_source::posts);
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    TEST_ASSERT_TRUE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_TRUE(snapshot == registryValue(1, true));
}
void test_registry_actor_stored_snapshot_ack_keeps_product_authority_closed() {
    RegistryActorFixture fixture;
    PeerRegistrySnapshot loaded, installed;
    TrustedAuthority authority;
    TEST_ASSERT_FALSE(fixture.owner.snapshot(loaded));
    TEST_ASSERT_FALSE(fixture.publication.snapshot(installed));
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(authority));
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    TEST_ASSERT_FALSE(fixture.owner.snapshot(loaded));
    TEST_ASSERT_FALSE(fixture.publication.snapshot(installed));
    registryOutcome(PeerRegistryOutcome::Empty, fixture.pump());
    TEST_ASSERT_TRUE(fixture.owner.snapshot(loaded));
    TEST_ASSERT_TRUE(fixture.publication.snapshot(installed));
    TEST_ASSERT_TRUE(loaded == installed);
    TEST_ASSERT_EQUAL(0, loaded.count);
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(authority));

    fixture.save(registryValue(1, true));
    TEST_ASSERT_TRUE(fixture.owner.snapshot(loaded));
    TEST_ASSERT_TRUE(fixture.publication.snapshot(installed));
    TEST_ASSERT_TRUE(loaded == installed);
    TEST_ASSERT_EQUAL(1, installed.count);
    TEST_ASSERT_TRUE(fixture.publication.credentialCurrent(installed.peers[0].credential));
    // Stored key and original role-install ACK are not a connected proof/GM grant.
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(authority));
}
void test_registry_actor_snapshot_is_readable_on_crypto_worker_and_survives_fresh_links() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto link = fixture.open();
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    PeerCredential out;
    TEST_ASSERT_TRUE(fixture.publication.credential(testIdentity(2), out));
    TEST_ASSERT_TRUE(fixture.publication.credentialCurrent(out));
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(link, testVerifiedPeer()));
    TrustedAuthority route;
    TEST_ASSERT_TRUE(fixture.access.route(link, route));
    fixture.close();
    const auto next = fixture.open();
    TEST_ASSERT_TRUE(next.token != link.token);
    TEST_ASSERT_TRUE(fixture.publication.credentialCurrent(out));
    fixture.close();
}
void test_registry_actor_role_change_closes_link_before_nvs_and_retains_game() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto link = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(link, testVerifiedPeer()));
    TrustedAuthority route;
    TEST_ASSERT_TRUE(fixture.access.route(link, route));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    TEST_ASSERT_FALSE(fixture.publication.credentialCurrent(registryValue().peers[0].credential));
    static RegistryActorFixture *observed;
    observed = &fixture;
    static BoundConnection original;
    original = link;
    host_peer_nvs::beforeSet = []() {
        TEST_ASSERT_FALSE(observed->access.proofConnectionCurrent(original));
    };
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    TEST_ASSERT_FALSE(fixture.access.proofConnectionCurrent(link));
    TEST_ASSERT_FALSE(fixture.access.route(link, route));
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    fixture.close();
}

void test_registry_actor_lost_close_ack_expires_without_any_nvs_or_retry() {
    RegistryActorFixture fixture;
    for (unsigned i = 0; i < 6; ++i) {
        ++f->now;
        registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    }
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    f->now += 5000;
    registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.owner.service());
    TEST_ASSERT_EQUAL(0, host_peer_nvs::opens);
    TEST_ASSERT_FALSE(fixture.store.begin());
    registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.owner.service());
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    fixture.actor();
    host_sdk_source::drain();
    fixture.io();
    PeerRegistrySnapshot out = registryValue(9);
    TEST_ASSERT_FALSE(fixture.publication.snapshot(out));
    TEST_ASSERT_TRUE(out == registryValue(9));
}
void test_registry_actor_new_connection_and_foreign_authority_preserved_by_old_job() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto old = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(old, testVerifiedPeer()));
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    fixture.close();
    const auto fresh = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(fresh, testVerifiedPeer()));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(fixture.access.route(fresh, current));
    fixture.io();
    registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.pump());
    TrustedAuthority observed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(observed));
    TEST_ASSERT_TRUE(observed == current);
    TEST_ASSERT_TRUE(fixture.access.route(fresh, observed));
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
    fixture.close();
}
void test_registry_actor_install_wrong_sdk_task_never_publishes_stored_role() {
    RegistryActorFixture fixture;
    fixture.ready();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(0, registryValue(1, true)));
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    TEST_ASSERT_EQUAL(1, host_peer_nvs::commits);
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.owner.service());
    PeerCredential out;
    TEST_ASSERT_FALSE(fixture.publication.credential(testIdentity(2), out));
    TEST_ASSERT_FALSE(fixture.store.begin());
}
void test_registry_actor_nvs_uncertain_after_close_has_no_install_or_key_publication() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto link = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(link, testVerifiedPeer()));
    fixture.io();
    const auto posts = host_sdk_source::posts;
    host_peer_nvs::setError = 1;
    host_peer_nvs::persistSetError = true;
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    registryOutcome(PeerRegistryOutcome::Uncertain, fixture.pump());
    TEST_ASSERT_EQUAL(posts + 1, host_sdk_source::posts);
    TEST_ASSERT_FALSE(fixture.access.proofConnectionCurrent(link));
    PeerCredential out;
    TEST_ASSERT_FALSE(fixture.publication.credential(testIdentity(2), out));
    TEST_ASSERT_FALSE(fixture.store.begin());
    fixture.close();
}
void test_registry_actor_sdk_token_replay_cannot_consume_reused_slot() {
    RegistryActorFixture fixture;
    const auto old = host_sdk_source::work.front();
    fixture.ready();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(0, registryValue(1, true)));
    fixture.actor();
    old.function(old.context);
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
}
void test_registry_actor_pending_inputs_and_conflicting_retries_keep_frozen_snapshot() {
    RegistryActorFixture fixture;
    fixture.ready();
    auto input = registryValue(1, true);
    const auto original = input;
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.replaceTrusted(0, input));
    input = registryValue(9, false);
    registryOutcome(PeerRegistryOutcome::Busy, fixture.owner.replaceTrusted(0, input));
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.replaceTrusted(0, original));
    registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    PeerRegistrySnapshot out;
    TEST_ASSERT_TRUE(fixture.publication.snapshot(out));
    TEST_ASSERT_TRUE(out == original);
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
}
void test_registry_actor_role_slot_duplicate_full_and_wrong_worker_preserve_pending() {
    RegistryActorFixture fixture;
    PeerRegistryRoleJob first;
    TEST_ASSERT_TRUE(fixture.source.readPeerRoleOrigin(testIdentity(), first.original));
    first.sequence = 1;
    first.receivedAt = f->now;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRolePost::Duplicate),
                      static_cast<unsigned>(fixture.source.postPeerRoleJob(first)));
    auto other = first;
    other.sequence = 2;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRolePost::Busy),
                      static_cast<unsigned>(fixture.source.postPeerRoleJob(other)));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRolePost::Rejected),
                      static_cast<unsigned>(fixture.source.postPeerRoleJob(first)));
    fixture.io();
    fixture.ready();
    TEST_ASSERT_EQUAL(2, host_sdk_source::posts);
}
void test_registry_actor_public_snapshot_freezes_peer_before_mutex_observation() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue());
    static Identity peer;
    peer = testIdentity(2);
    host_rtos::beforeEnter = []() { peer = testIdentity(9); };
    PeerCredential out;
    TEST_ASSERT_TRUE(fixture.publication.credential(peer, out));
    TEST_ASSERT_TRUE(out.peer == testIdentity(2));
}
void test_registry_actor_expired_install_and_overall_budget_never_refresh() {
    for (unsigned variant = 0; variant < 2; ++variant) {
        RegistryActorFixture fixture;
        fixture.ready();
        registryOutcome(PeerRegistryOutcome::Pending,
                        fixture.owner.replaceTrusted(0, registryValue(1, true)));
        if (!variant) {
            registryOutcome(PeerRegistryOutcome::Pending, fixture.pump());
            f->now += 5000;
            registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.pump());
            TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
        } else {
            host_peer_nvs::afterCommit = []() { f->now += 30000; };
            registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.pump());
            TEST_ASSERT_EQUAL(3, host_sdk_source::posts);
        }
        TEST_ASSERT_FALSE(fixture.store.begin());
        PeerRegistrySnapshot out;
        TEST_ASSERT_FALSE(fixture.publication.snapshot(out));
    }
}
void test_registry_actor_wrong_sdk_instance_and_token_exhaustion_fail_without_repost() {
    {
        RegistryActorFixture fixture;
        fixture.ready();
        registryOutcome(PeerRegistryOutcome::Pending,
                        fixture.owner.replaceTrusted(0, registryValue()));
        btu_thread = reinterpret_cast<void *>(9);
        registryOutcome(PeerRegistryOutcome::RuntimeRejected, fixture.pump());
        TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
    }
    {
        RegistryActorFixture fixture;
        fixture.ready();
        const auto prior = larp::companion::peerRolePostCounter;
        larp::companion::peerRolePostCounter = UINT32_MAX;
        registryOutcome(PeerRegistryOutcome::RuntimeRejected,
                        fixture.owner.replaceTrusted(0, registryValue()));
        TEST_ASSERT_EQUAL(2, host_sdk_source::posts);
        TEST_ASSERT_EQUAL(0, host_peer_nvs::sets);
        larp::companion::peerRolePostCounter = prior; // TEST ONLY, queue quiescent.
    }
}
class ActorPeerContext : public IPeerProofContext {
  public:
    ActorPeerContext(RegistryActorFixture &fixture) : fixture_(fixture) {}
    PeerProofBinding original;
    bool originalLive(const PeerProofBinding &value) const noexcept override {
        return value == original && fixture_.source.closedPeerConnectionCurrent(value);
    }
    bool credentialCurrent(const PeerCredential &value) const noexcept override {
        return fixture_.publication.credentialCurrent(value);
    }

  private:
    RegistryActorFixture &fixture_;
};
void test_registry_actor_closed_proof_copy_is_original_and_closes_before_role_ports() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    ActorPeerContext context(fixture);
    context.original.device = testIdentity();
    context.original.boot = f->boot;
    GattsSourceRegistration reg;
    TEST_ASSERT_TRUE(fixture.source.readRegistration(reg));
    context.original.registration = reg.token;
    context.original.interface = reg.interface;
    context.original.connection = fixture.open();
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_TRUE(context.originalLive(context.original));
    auto wrong = context.original;
    wrong.connection.token++;
    TEST_ASSERT_FALSE(context.originalLive(wrong));
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    fixture.owner.replaceTrusted(1, registryValue(2, false)));
    fixture.actor();
    host_sdk_source::drainOne();
    TEST_ASSERT_FALSE(context.originalLive(context.original));
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, fixture.owner.service());
    registryOutcome(PeerRegistryOutcome::Saved, fixture.pump());
    fixture.close();
}

void test_registry_actor_foreign_authority_before_request_is_never_retired() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    const auto link = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(link, testVerifiedPeer()));
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.advanceAuthority(99, true, foreign)));
    fixture.io();
    auto result = fixture.owner.replaceTrusted(1, registryValue(2, false));
    if (result == PeerRegistryOutcome::Pending)
        result = fixture.pump();
    registryOutcome(PeerRegistryOutcome::RuntimeRejected, result);
    TrustedAuthority observed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(observed));
    TEST_ASSERT_TRUE(observed == foreign);
    TEST_ASSERT_EQUAL(1, host_peer_nvs::sets);
}

void test_registry_actor_copied_policy_remains_original_after_verification_and_revoke() {
    RegistryActorFixture fixture;
    fixture.ready();
    fixture.save(registryValue(1, true));
    PeerProofBinding original;
    original.device = testIdentity();
    original.boot = f->boot;
    GattsSourceRegistration reg;
    TEST_ASSERT_TRUE(fixture.source.readRegistration(reg));
    original.registration = reg.token;
    original.interface = reg.interface;
    original.connection = fixture.open();
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Verified,
                  fixture.access.authenticate(original.connection, testVerifiedPeer()));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_TRUE(fixture.source.closedPeerConnectionCurrent(original));
    fixture.actor();
    accessOutcome(IdentityAccessOutcome::Revoked, fixture.access.revokeGameMaster(testIdentity(2)));
    TrustedAuthority route;
    TEST_ASSERT_FALSE(fixture.access.route(original.connection, route));
    // Identity proof survives a direct role-only revoke; there is no GM route.
    TEST_ASSERT_TRUE(fixture.source.closedPeerConnectionCurrent(original));
    std::array<GameMasterAssignment, 4> empty = {};
    accessOutcome(IdentityAccessOutcome::AssignmentsReplaced,
                  fixture.access.replaceGameMastersClosed(empty));
    TEST_ASSERT_FALSE(fixture.source.closedPeerConnectionCurrent(original));
    fixture.close();
}
class IdentityCopyModel : public IIdentityAccessObserver {
  public:
    IdentityAccessSnapshot snapshot;
    unsigned calls = 0;
    void copyIdentitySnapshot(const IdentityAccessSnapshot &value) noexcept override {
        snapshot = value;
        ++calls;
    }
};
void test_registry_actor_owner_observer_is_single_and_failed_ack_preserves_output() {
    IdentityAccess owner(f->channel.actual);
    IdentityCopyModel observed, other;
    TEST_ASSERT_TRUE(owner.attachObserver(observed));
    TEST_ASSERT_FALSE(observed.snapshot.current);
    TEST_ASSERT_FALSE(owner.attachObserver(other));
    accessOutcome(IdentityAccessOutcome::Initialized, owner.begin(testIdentity()));
    TEST_ASSERT_TRUE(observed.snapshot.current);
    BoundConnection link;
    accessOutcome(IdentityAccessOutcome::Connected, owner.connect(257, link));
    TEST_ASSERT_TRUE(observed.snapshot.proofAllowed);
    TEST_ASSERT_TRUE(observed.snapshot.connection == link);
    std::array<GameMasterAssignment, 4> invalid = {};
    invalid[0].principal = 1;
    TrustedAuthority out = authority(123);
    accessOutcome(IdentityAccessOutcome::InvalidAssignment,
                  owner.replaceGameMastersClosed(invalid, &out));
    TEST_ASSERT_TRUE(out == authority(123));
    TEST_ASSERT_TRUE(observed.snapshot.proofAllowed);
    accessOutcome(IdentityAccessOutcome::Disconnected, owner.disconnect(link));
    TEST_ASSERT_FALSE(observed.snapshot.connected);
    TEST_ASSERT_FALSE(observed.snapshot.proofAllowed);
    TEST_ASSERT_TRUE(observed.snapshot.authority.epoch > link.token);
    TEST_ASSERT_EQUAL(0, other.calls);
}

#if defined(LARP_REAL_PEER_CRYPTO)
void registryActorCryptoVertical(bool sdkSecurity, bool asyncGrant = false) {
    Esp32PeerProofCrypto crypto;
    RegistryActorFixture fixture(&crypto, 0x01020304, sdkSecurity);
    fixture.ready();
    fixture.save(registryValue());
    fixture.save(registryValue(2, true));
    Esp32PeerRegistryStore restoredStore;
    TEST_ASSERT_TRUE(restoredStore.begin());
    PeerRegistryOwner restored(restoredStore, crypto, fixture.publication);
    registryOutcome(PeerRegistryOutcome::Pending, restored.begin(testIdentity()));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, restored.service());
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Loaded, restored.service());
    TEST_ASSERT_EQUAL(8, host_sdk_source::posts);
    ActorPeerContext context(fixture);
    FreeRtosPeerProofGrantRuntime grant(fixture.source, fixture.publication, fixture.clock,
                                        f->boot);
    if (asyncGrant) {
        fixture.actor();
        TEST_ASSERT_TRUE(fixture.source.configurePeerProofGrantNow(f->boot, grant));
    }
    IPeerProofContext &proofContext =
        asyncGrant ? static_cast<IPeerProofContext &>(grant) : context;
    context.original.device = testIdentity();
    context.original.boot = f->boot;
    GattsSourceRegistration reg;
    TEST_ASSERT_TRUE(fixture.source.readRegistration(reg));
    context.original.registration = reg.token;
    context.original.interface = reg.interface;
    context.original.connection = fixture.open();
    if (sdkSecurity) {
        fixture.actor();
        sdk446::SmpData data = {};
        TEST_ASSERT_EQUAL(23, host_sdk_security::callback(2, fixture.address, &data));
        TEST_ASSERT_EQUAL(23, host_sdk_security::callback(1, fixture.address, &data));
        const std::uint8_t resolved[5] = {0, 4, 0, 0, 0x0d};
        std::memcpy(data.complete, resolved, 5);
        TEST_ASSERT_EQUAL(23, host_sdk_security::callback(7, fixture.address, &data));
    }
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3); // Crypto model worker.
    PeerClock clock;
    PeerNonceModel nonce;
    PeerProofOwner proof(proofContext, crypto, nonce, clock);
    if (asyncGrant)
        TEST_ASSERT_TRUE(grant.beginWorker());
    PeerCredential credential;
    TEST_ASSERT_TRUE(fixture.publication.credential(testIdentity(2), credential));
    peerOutcome(PeerProofOutcome::Issued, proof.issue(context.original, credential));
    PeerChallenge challenge;
    TEST_ASSERT_TRUE(proof.challenge(context.original, challenge));
    PeerTranscript transcript;
    TEST_ASSERT_TRUE(encodePeerChallenge(challenge, transcript));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(peerTestTranscript, transcript.data(), 108);
    PeerProofReceiver receiver(proof);
    FreeRtosPeerProofQueue authFrames(proofContext, clock);
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
        const auto dispatch = dispatchPeerProofFrame(authFrames, receiver);
        TEST_ASSERT_TRUE(dispatch.received);
        result = dispatch.result;
    }
    peerOutcome(PeerProofOutcome::Verified, result.proof);
    VerifiedPeerProof verified;
    TEST_ASSERT_TRUE(proof.readVerified(context.original, verified));
    PeerProofGrantJob grantJob;
    TrustedAuthority grantAck;
    if (asyncGrant) {
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofGrantState::Pending),
                          static_cast<unsigned>(grant.submitVerified(proof, context.original)));
        TEST_ASSERT_TRUE(grant.job(grantJob));
        TEST_ASSERT_EQUAL(188, sizeof(grantJob));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofGrantState::Pending),
                          static_cast<unsigned>(grant.service(grantAck)));
        TEST_ASSERT_EQUAL(0, grantAck.epoch);
        fixture.actor();
        TrustedAuthority closed;
        TEST_ASSERT_FALSE(fixture.access.route(context.original.connection, closed));
        host_sdk_source::drainOne(); // Actual Source callback consumes the copied slot.
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerProofGrantState::Confirmed),
                          static_cast<unsigned>(grant.service(grantAck)));
        TEST_ASSERT_TRUE(grantAck.gameMaster && grantAck.principal == 7);
        fixture.actor();
    } else {
        OriginalPeerSecurity MODELED;
        MODELED.original = context.original;
        MODELED.encrypted = MODELED.mitm = MODELED.secureConnections = MODELED.bonded = true;
        fixture.actor();
        if (sdkSecurity) {
            // Verified copy handed from crypto model worker; no actor read of the
            // mutable proof owner. Recheck protected credentials/original at grant.
            TEST_ASSERT_TRUE(fixture.publication.credentialCurrent(verified.credential));
            TEST_ASSERT_TRUE(context.originalLive(verified.original));
            TEST_ASSERT_TRUE(f->now - verified.issuedAt < PeerProofLimits::MaxAgeMilliseconds);
            TEST_ASSERT_TRUE(
                fixture.source.readOriginalPeerSecurityNow(verified.original, MODELED));
            TEST_ASSERT_TRUE(fixture.publication.credentialCurrent(verified.credential));
            TEST_ASSERT_TRUE(context.originalLive(verified.original));
        }
        VerifiedPeerEvidence evidence;
        TEST_ASSERT_TRUE(bindPeerProofEvidence(verified, MODELED, evidence));
        accessOutcome(IdentityAccessOutcome::Verified,
                      fixture.access.authenticate(context.original.connection, evidence));
    }
    TrustedAuthority route;
    TEST_ASSERT_TRUE(fixture.access.route(context.original.connection, route));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_TRUE(proof.readVerified(context.original, verified));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(5); // Application model worker.
    f->control.service(++f->now);
    CommandStream stream(f->channel.actual);
    GattsResponseDispatcher responseWorker(fixture.responses, fixture.source);
    const auto document = json(f->command());
    unsigned frames = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        const auto bytes = frameBytes(document, offset, count);
        sdk446::Request request = {};
        request.write.handle = 0x42;
        request.write.needResponse = true;
        request.write.length = static_cast<std::uint16_t>(bytes.size());
        std::memcpy(request.write.value, bytes.data(), bytes.size());
        fixture.actor();
        host_sdk_source::registeredCallbacks.request(257, frames + 1, 2, &request);
        std::memset(&request, 0, sizeof(request)); // Original borrowed SDK value has expired.
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4); // Response worker.
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Submitted),
                          static_cast<unsigned>(responseWorker.service()));
        fixture.actor();
        host_sdk_source::drainOne();
        TEST_ASSERT_EQUAL(frames + 1, host_sdk_source::sent.size());
        TEST_ASSERT_EQUAL(257, host_sdk_source::sent.back().original.connection.handle);
        TEST_ASSERT_EQUAL(frames + 1, host_sdk_source::sent.back().transaction);
        TEST_ASSERT_EQUAL(0, static_cast<unsigned>(host_sdk_source::sent.back().status));
        host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(5);
        TEST_ASSERT_TRUE(dispatchCommandFrame(fixture.commands, stream, f->now).received);
        offset += count;
        ++frames;
    }
    TEST_ASSERT_EQUAL(13, frames);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending,
                    restored.replaceTrusted(2, registryValue(3, false)));
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Pending, restored.service());
    fixture.actor();
    host_sdk_source::drainOne();
    fixture.io();
    registryOutcome(PeerRegistryOutcome::Saved, restored.service());
    PeerRegistrySnapshot snapshot;
    TEST_ASSERT_TRUE(fixture.publication.snapshot(snapshot));
    TEST_ASSERT_TRUE(snapshot == registryValue(3, false));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(3);
    TEST_ASSERT_FALSE(proof.readVerified(context.original, verified));
    fixture.actor();
    TEST_ASSERT_FALSE(fixture.access.route(context.original.connection, route));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(5);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL(3, host_peer_nvs::sets);
    TEST_ASSERT_EQUAL(3, host_peer_nvs::commits);
    TEST_ASSERT_EQUAL(asyncGrant ? 24 : 23, host_sdk_source::posts);
    TEST_ASSERT_EQUAL(0, host_sdk_source::passedRequests);
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
    TEST_ASSERT_TRUE(host_peer_nvs::handles.empty());
    std::string observation =
        "{\"scope\":\"host_actual_crypto_original_sdk_actor_nvs_model\",\"recordBytes\":404,"
        "\"roleSlots\":1,\"rolesMaximum\":4,\"roleSdkPosts\":10,\"attSdkPosts\":13,"
        "\"sdkPosts\":23,\"setCalls\":3,\"commitCalls\":3,\"proofFrames\":6,\"commandFrames\":13,"
        "\"registryWorker\":2,\"sourceActor\":1,\"cryptoWorker\":3,\"responseWorker\":4,"
        "\"applicationWorker\":5,\"jobDeadlineMilliseconds\":5000,"
        "\"operationMilliseconds\":30000,\"restoredExplicitRole\":true,"
        "\"closeAckBeforeNvs\":true,\"installAckBeforePublication\":true,"
        "\"cryptoReadsProtectedCopy\":true,\"proofCurrentAfterVerification\":true,"
        "\"runningAfterRevocation\":true,\"oldProofRejected\":true,\"transportSecurityModeled\":"
        "true,"
        "\"physicalDurabilityMeasured\":false,\"smpMeasured\":false,\"rfInitialized\":false}\n";
    if (sdkSecurity) {
        TEST_ASSERT_EQUAL(1, host_sdk_security::registrations);
        TEST_ASSERT_EQUAL(3, host_sdk_security::forwarded);
        TEST_ASSERT_EQUAL(1, host_sdk_security::connections);
        TEST_ASSERT_EQUAL(1, host_sdk_security::flagReads);
        TEST_ASSERT_EQUAL(1, host_sdk_security::stateReads);
        TEST_ASSERT_EQUAL(2, host_sdk_security::runningReads);
        observation =
            "{\"scope\":\"host_actual_crypto_original_smp_sdk_metadata_model\","
            "\"smpRegistrations\":1,\"originalSmpCallbacks\":3,\"gattReads\":1,"
            "\"flagReads\":1,\"stateReads\":1,\"procedureReads\":2,"
            "\"resolvedPairingMode\":13,\"securityLevel\":4,\"flags\":22,"
            "\"linkState\":5,\"keyBytes\":16,\"proofFrames\":6,\"commandFrames\":13,"
            "\"sourceActor\":1,\"cryptoWorker\":3,\"pairMilliseconds\":30000,"
            "\"storedExplicitRole\":true,\"oldProofRejected\":true,\"runningAfterRevocation\":true,"
            "\"bondRestoreImplemented\":false,\"smpMeasured\":false,\"rfInitialized\":false}\n";
    }
    if (asyncGrant) {
        TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
        TEST_ASSERT_TRUE(grantJob.proof.original == context.original);
        TEST_ASSERT_TRUE(grantJob.original.authority.epoch == context.original.connection.token);
        TEST_ASSERT_TRUE(grantJob.proof.credential == registryValue().peers[0].credential);
        observation =
            "{\"scope\":\"host_actual_crypto_original_sdk_proof_grant_model\","
            "\"grantSlots\":1,\"grantBytes\":188,\"grantSdkPosts\":1,\"sdkPosts\":24,"
            "\"queueMilliseconds\":5000,\"proofMilliseconds\":30000,"
            "\"sourceActor\":1,\"cryptoWorker\":3,\"registryWorker\":2,"
            "\"proofFrames\":6,\"commandFrames\":13,\"setCalls\":3,\"commitCalls\":3,"
            "\"originalSmpCallbacks\":3,\"gattReads\":1,\"flagReads\":1,"
            "\"stateReads\":1,\"procedureReads\":2,\"bootHigh\":" +
            std::to_string(grantJob.proof.original.boot.high) +
            ",\"bootLow\":" + std::to_string(grantJob.proof.original.boot.low) +
            ",\"registration\":" + std::to_string(grantJob.proof.original.registration) +
            ",\"lifecycle\":" + std::to_string(grantJob.original.lifecycle) +
            ",\"connectionToken\":" + std::to_string(grantJob.proof.original.connection.token) +
            ",\"packedConnection\":" + std::to_string(grantJob.proof.original.connection.handle) +
            ",\"interface\":" + std::to_string(grantJob.proof.original.interface) +
            ",\"proofSequence\":" + std::to_string(grantJob.proof.sequence) +
            ",\"grantSequence\":" + std::to_string(grantJob.sequence) +
            ",\"ackEpoch\":" + std::to_string(grantAck.epoch) +
            ",\"ackPrincipal\":" + std::to_string(grantAck.principal) +
            ",\"storedExplicitRole\":true,\"immutableBootContext\":true,"
            "\"ackBeforeWorkerConfirmation\":true,\"actorReadsMutableProofOwner\":false,"
            "\"oldProofRejected\":true,\"runningAfterRevocation\":true,"
            "\"rfInitialized\":false,\"smpMeasured\":false,\"physicalDurabilityMeasured\":false}\n";
    }
    const char *path = asyncGrant ? "docs/companion/fixtures/peer-grant/observation.json"
                       : sdkSecurity
                           ? "docs/companion/fixtures/peer-security/observation.json"
                           : "docs/companion/fixtures/peer-registry/actor-observation.json";
    if (std::getenv(asyncGrant    ? "LARP_EXPORT_PEER_GRANT_FIXTURE"
                    : sdkSecurity ? "LARP_EXPORT_PEER_SECURITY_FIXTURE"
                                  : "LARP_EXPORT_REGISTRY_ACTOR_FIXTURE")) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(observation.size(),
                          std::fwrite(observation.data(), 1, observation.size(), file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char read[2048];
    const auto count = std::fread(read, 1, sizeof(read), file);
    TEST_ASSERT_EQUAL(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(observation.c_str(), std::string(read, count).c_str());
    fixture.close();
}
void test_registry_actor_actual_crypto_original_sdk_roles_frames_game_and_revoke() {
    registryActorCryptoVertical(false);
}
void test_security_actual_crypto_original_smp_metadata_frames_game_and_revoke() {
    registryActorCryptoVertical(true);
}
void test_grant_actual_crypto_worker_original_sdk_ack_frames_game_and_revoke() {
    registryActorCryptoVertical(true, true);
}

#endif

void test_registry_actor_fixed_job_rejects_invalid_origin_roles_and_order() {
    RegistryActorFixture fixture;
    PeerRegistryRoleJob original;
    TEST_ASSERT_TRUE(fixture.source.readPeerRoleOrigin(testIdentity(), original.original));
    original.sequence = 2;
    original.receivedAt = f->now;
    original.kind = PeerRegistryRoleKind::Install;
    original.roles[0].peer = testIdentity(2);
    original.roles[0].principal = 7;
    TEST_ASSERT_TRUE(validPeerRegistryRoleJob(original));
    const auto posts = host_sdk_source::posts;
    for (unsigned variant = 0; variant < 16; ++variant) {
        auto wrong = original;
        if (variant == 0)
            wrong.original.device = {};
        if (variant == 1)
            wrong.original.registration = 0;
        if (variant == 2)
            wrong.original.lifecycle = 0;
        if (variant == 3)
            wrong.original.interface = 0;
        if (variant == 4)
            wrong.original.interface = 0xff;
        if (variant == 5)
            wrong.original.authority.principal = 7;
        if (variant == 6)
            wrong.original.authority = authority(1, true, 0);
        if (variant == 7)
            wrong.original.authority = authority(1, false, 7);
        if (variant == 8)
            wrong.sequence = 0;
        if (variant == 9)
            wrong.kind = PeerRegistryRoleKind::Close;
        if (variant == 10)
            wrong.kind = static_cast<PeerRegistryRoleKind>(2);
        if (variant == 11)
            wrong.roles[0].peer = {};
        if (variant == 12)
            wrong.roles[0].principal = 0;
        if (variant == 13) {
            wrong.roles[2].peer = testIdentity(3);
            wrong.roles[2].principal = 8;
        }
        if (variant == 14) {
            wrong.roles[1].peer = testIdentity(3);
            wrong.roles[1].principal = 7;
        }
        if (variant == 15) {
            wrong.roles[1].peer = testIdentity();
            wrong.roles[1].principal = 8;
        }
        TEST_ASSERT_FALSE(validPeerRegistryRoleJob(wrong));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(PeerRegistryRolePost::Rejected),
                          static_cast<unsigned>(fixture.source.postPeerRoleJob(wrong)));
    }
    TEST_ASSERT_EQUAL(posts, host_sdk_source::posts);
    fixture.ready();
}
