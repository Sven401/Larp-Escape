#pragma once
Identity testIdentity(std::uint8_t id = 1) {
    Identity result;
    result.bytes[0] = id;
    result.bytes[15] = static_cast<std::uint8_t>(id + 1);
    return result;
}
VerifiedPeerEvidence testVerifiedPeer(std::uint8_t id = 2) {
    VerifiedPeerEvidence evidence;
    evidence.peer = testIdentity(id);
    evidence.encrypted = evidence.mitm = evidence.secureConnections = evidence.bonded = true;
    return evidence;
}
void accessOutcome(IdentityAccessOutcome expected, IdentityAccessOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void openIdentity(IdentityAccess &access, BoundConnection &connection) {
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(0, connection));
}
TrustedAuthority assignedRoute(IdentityAccess &access, const BoundConnection &connection) {
    TrustedAuthority route;
    TEST_ASSERT_TRUE(access.route(connection, route));
    return route;
}
void test_device_identity_codec_bounds_and_public_fixture_only() {
    Identity device;
    const char *hex = "00112233445566778899AABBCCDDEEFF";
    TEST_ASSERT_TRUE(decodeIdentityHex(hex, 32, device));
    auto preserved = device;
    for (const auto text : {"00000000000000000000000000000000", "00112233445566778899AABBCCDDEEFf",
                            "GG112233445566778899AABBCCDDEEFF"}) {
        TEST_ASSERT_FALSE(decodeIdentityHex(text, std::strlen(text), preserved));
        TEST_ASSERT_TRUE(preserved == device);
    }
    TEST_ASSERT_FALSE(decodeIdentityHex(hex, 31, preserved));
    TEST_ASSERT_FALSE(decodeIdentityHex(hex, 33, preserved));
    TEST_ASSERT_FALSE(decodeIdentityHex(nullptr, 32, preserved));
    char bytes[maxDeviceIdentityJsonBytes + 2];
    std::memset(bytes, '#', sizeof(bytes));
    std::size_t written = 99;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeDeviceIdentity(
                          device, bytes + 1, maxDeviceIdentityJsonBytes, written)));
    TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[maxDeviceIdentityJsonBytes + 1]);
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, hex));
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, "\"scope\":\"display_identity\""));
    const auto count = written;
    const auto document = std::string(bytes + 1, count);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::BufferTooSmall),
        static_cast<unsigned>(encodeDeviceIdentity(device, bytes + 1, count, written)));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[1]);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeDeviceIdentity(device, bytes + 1, count + 1, written)));
    TEST_ASSERT_EQUAL_UINT32(count, written);
    TEST_ASSERT_EQUAL_STRING(document.c_str(), bytes + 1);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::InvalidValue),
        static_cast<unsigned>(encodeDeviceIdentity({}, bytes + 1, sizeof(bytes) - 1, written)));
    const char *path = "docs/companion/fixtures/device-identity-v1.json";
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *out = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT_EQUAL_UINT32(document.size(),
                                 std::fwrite(document.data(), 1, document.size(), out));
        TEST_ASSERT_EQUAL_INT('\n', std::fputc('\n', out));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(out));
    }
    auto *in = std::fopen(path, "rt");
    TEST_ASSERT_NOT_NULL(in);
    std::string expected;
    for (int byte = std::fgetc(in); byte != EOF; byte = std::fgetc(in))
        expected += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(in));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(in));
    TEST_ASSERT_EQUAL_STRING((document + "\n").c_str(), expected.c_str());
}
void test_authority_owned_epoch_advance_above_fault_and_maximum_no_wrap() {
    AuthorityState state;
    TrustedAuthority output = authority(99);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(10))));
    auto conflict = authority(10, false);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Conflict),
                      static_cast<unsigned>(state.update(conflict)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.advance(7, true, output)));
    TEST_ASSERT_EQUAL_UINT32(11, output.epoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Invalid),
                      static_cast<unsigned>(state.advance(0, true, output)));
    TEST_ASSERT_EQUAL_UINT32(11, output.epoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.advance(0, false, output)));
    TEST_ASSERT_EQUAL_UINT32(13, output.epoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(UINT32_MAX))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.advance(7, true, output)));
    TEST_ASSERT_EQUAL_UINT32(13, output.epoch);
    TEST_ASSERT_FALSE(state.read(output));
    AuthorityState conditional;
    auto expected = authority(30, false);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(conditional.update(expected)));
    TrustedAuthority conditionOutput = authority(999);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Published),
        static_cast<unsigned>(conditional.advanceIfCurrent(expected, 7, true, conditionOutput)));
    TEST_ASSERT_EQUAL_UINT32(31, conditionOutput.epoch);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Conflict),
        static_cast<unsigned>(conditional.advanceIfCurrent(expected, 7, true, conditionOutput)));
    TEST_ASSERT_EQUAL_UINT32(31, conditionOutput.epoch);
    TEST_ASSERT_TRUE(conditional.read(conditionOutput));
    TEST_ASSERT_EQUAL_UINT32(31, conditionOutput.epoch);
    FreeRtosGameControlChannel empty;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(empty.advanceAuthority(7, true, output)));
    TEST_ASSERT_EQUAL_UINT32(13, output.epoch);
}
void test_verified_connection_needs_explicit_gm_assignment_before_actual_start() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    TrustedAuthority route = authority(99);
    TEST_ASSERT_FALSE(access.route(connection, route));
    TEST_ASSERT_EQUAL_UINT32(99, route.epoch);
    const auto formerlyValidCommand = f->command();
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
    TEST_ASSERT_FALSE(f->submit(formerlyValidCommand, connection.token, 7));
    TEST_ASSERT_FALSE(f->application.gameRunning());
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    route = assignedRoute(access, connection);
    TEST_ASSERT_TRUE(route.epoch > connection.token);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_EQUAL_UINT32(route.epoch, reply.connectionEpoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    const auto status = currentStatus();
    TEST_ASSERT_EQUAL_UINT32(route.epoch, status.connectionEpoch);
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, f->now));
    completeReply(sender, f->now);
    TEST_ASSERT_EQUAL_UINT32(route.epoch, bearer.last.epoch);
}
void test_assigned_peer_without_each_verification_requirement_never_admits() {
    for (unsigned missing = 0; missing < 5; ++missing) {
        IdentityAccess access(f->channel.actual);
        BoundConnection connection;
        openIdentity(access, connection);
        accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
        auto evidence = testVerifiedPeer();
        if (missing == 0)
            evidence.encrypted = false;
        if (missing == 1)
            evidence.mitm = false;
        if (missing == 2)
            evidence.secureConnections = false;
        if (missing == 3)
            evidence.bonded = false;
        if (missing == 4)
            evidence.peer = {};
        accessOutcome(IdentityAccessOutcome::SecurityRejected,
                      access.authenticate(connection, evidence));
        TrustedAuthority route;
        TEST_ASSERT_FALSE(access.route(connection, route));
        accessOutcome(IdentityAccessOutcome::SecurityRejected,
                      access.authenticate(connection, testVerifiedPeer()));
        TEST_ASSERT_FALSE(
            access.route(connection, route)); // Failure seals this link until reconnect.
        accessOutcome(IdentityAccessOutcome::Disconnected, access.disconnect(connection));
        BoundConnection next;
        accessOutcome(IdentityAccessOutcome::Connected, access.connect(0, next));
        TEST_ASSERT_TRUE(next.token > connection.token);
        accessOutcome(IdentityAccessOutcome::Verified,
                      access.authenticate(next, testVerifiedPeer()));
        TEST_ASSERT_TRUE(access.route(next, route));
        accessOutcome(IdentityAccessOutcome::Disconnected, access.disconnect(next));
    }
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_duplicate_auth_and_assignment_preserve_epoch_and_game_receipt() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    const auto route = assignedRoute(access, connection);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    const auto lease = f->control.snapshot().lease;
    accessOutcome(IdentityAccessOutcome::Duplicate,
                  access.authenticate(connection, testVerifiedPeer()));
    accessOutcome(IdentityAccessOutcome::Duplicate, access.assignGameMaster(testIdentity(2), 7));
    const auto current = assignedRoute(access, connection);
    TEST_ASSERT_TRUE(current == route);
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(lease, f->control.snapshot().lease);
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().nextRequest);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_recycled_handle_stale_auth_and_disconnect_cannot_change_new_binding() {
    IdentityAccess access(f->channel.actual);
    BoundConnection old;
    openIdentity(access, old);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(old, testVerifiedPeer()));
    const auto oldRoute = assignedRoute(access, old);
    BoundConnection unchanged;
    unchanged.token = 999;
    accessOutcome(IdentityAccessOutcome::Busy, access.connect(0, unchanged));
    TEST_ASSERT_EQUAL_UINT32(999, unchanged.token);
    TEST_ASSERT_TRUE(assignedRoute(access, old) == oldRoute);
    accessOutcome(IdentityAccessOutcome::Disconnected, access.disconnect(old));
    BoundConnection next;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(old.handle, next));
    TEST_ASSERT_TRUE(next.token > old.token);
    accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(next, testVerifiedPeer()));
    const auto current = assignedRoute(access, next);
    accessOutcome(IdentityAccessOutcome::StaleConnection,
                  access.authenticate(old, testVerifiedPeer(3)));
    accessOutcome(IdentityAccessOutcome::StaleConnection, access.disconnect(old));
    auto wrongHandle = next;
    ++wrongHandle.handle;
    accessOutcome(IdentityAccessOutcome::StaleConnection,
                  access.authenticate(wrongHandle, testVerifiedPeer()));
    TEST_ASSERT_TRUE(assignedRoute(access, next) == current);
    TrustedAuthority unchangedAuthority = authority(999);
    TEST_ASSERT_FALSE(access.route(old, unchangedAuthority));
    TEST_ASSERT_EQUAL_UINT32(999, unchangedAuthority.epoch);
    f->control.service(++f->now);
    TEST_ASSERT_FALSE(f->submit(f->command(), oldRoute.epoch, oldRoute.principal));
}

void test_gm_revocation_fences_copied_stop_but_preserves_running_game() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    const auto route = assignedRoute(access, connection);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_TRUE(f->submit(f->command(GameCommandKind::Stop), route.epoch, route.principal));
    accessOutcome(IdentityAccessOutcome::Revoked, access.revokeGameMaster(testIdentity(2)));
    TrustedAuthority target = authority(999);
    TEST_ASSERT_FALSE(access.route(connection, target));
    TEST_ASSERT_EQUAL_UINT32(999, target.epoch);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_FALSE(f->channel.actual.takeCurrentReply(reply));
    ConfiguredStatus raw;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(raw));
    TEST_ASSERT_EQUAL_UINT32(0, raw.controlLease);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 8));
    const auto reassigned = assignedRoute(access, connection);
    TEST_ASSERT_TRUE(reassigned.epoch > route.epoch);
    TEST_ASSERT_EQUAL_UINT32(8, reassigned.principal);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_conflicting_identity_closes_and_same_link_cannot_reopen() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    const auto route = assignedRoute(access, connection);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->submit(f->command(), route.epoch, route.principal));
    f->control.service(++f->now);
    accessOutcome(IdentityAccessOutcome::IdentityConflict,
                  access.authenticate(connection, testVerifiedPeer(3)));
    TrustedAuthority target;
    TEST_ASSERT_FALSE(access.route(connection, target));
    accessOutcome(IdentityAccessOutcome::SecurityRejected,
                  access.authenticate(connection, testVerifiedPeer()));
    TEST_ASSERT_FALSE(access.route(connection, target));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_identity_registry_bounds_unique_principals_and_explicit_slot_reuse() {
    IdentityAccess access(f->channel.actual);
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    accessOutcome(IdentityAccessOutcome::InvalidAssignment, access.assignGameMaster({}, 1));
    accessOutcome(IdentityAccessOutcome::InvalidAssignment,
                  access.assignGameMaster(testIdentity(2), 0));
    for (unsigned i = 0; i < IdentityAccessLimits::Assignments; ++i)
        accessOutcome(IdentityAccessOutcome::Assigned,
                      access.assignGameMaster(testIdentity(i + 2), i + 7));
    accessOutcome(IdentityAccessOutcome::Duplicate, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::InvalidAssignment,
                  access.assignGameMaster(testIdentity(2), 99));
    accessOutcome(IdentityAccessOutcome::InvalidAssignment,
                  access.assignGameMaster(testIdentity(99), 7));
    accessOutcome(IdentityAccessOutcome::Capacity, access.assignGameMaster(testIdentity(99), 99));
    accessOutcome(IdentityAccessOutcome::Revoked, access.revokeGameMaster(testIdentity(2)));
    accessOutcome(IdentityAccessOutcome::Duplicate, access.revokeGameMaster(testIdentity(2)));
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(99), 7));
    BoundConnection connection;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(0, connection));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer(2)));
    TrustedAuthority target;
    TEST_ASSERT_FALSE(
        access.route(connection, target)); // Revoked identity did not inherit reused principal.
}
void test_identity_device_must_be_valid_and_remains_immutable_until_new_boot() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    connection.token = 999;
    accessOutcome(IdentityAccessOutcome::InvalidIdentity, access.connect(0, connection));
    TEST_ASSERT_EQUAL_UINT32(999, connection.token);
    accessOutcome(IdentityAccessOutcome::InvalidIdentity,
                  access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    Identity device;
    TEST_ASSERT_TRUE(access.deviceIdentity(device));
    TEST_ASSERT_TRUE(device == testIdentity());
    accessOutcome(IdentityAccessOutcome::Duplicate, access.begin(testIdentity()));
    accessOutcome(IdentityAccessOutcome::InvalidIdentity, access.begin(testIdentity(99)));
    accessOutcome(IdentityAccessOutcome::InvalidIdentity, access.begin(testIdentity()));
    TEST_ASSERT_FALSE(access.deviceIdentity(device));
    TEST_ASSERT_TRUE(device == testIdentity());
    TrustedAuthority unchangedAuthority;
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(unchangedAuthority));
}
void test_identity_maximum_epoch_and_channel_suspend_cannot_be_reopened_by_duplicates() {
    IdentityAccess access(f->channel.actual);
    accessOutcome(IdentityAccessOutcome::Initialized, access.begin(testIdentity()));
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    f->channel.actual.updateAuthority(authority(UINT32_MAX - 1, false));
    BoundConnection connection;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(0, connection));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, connection.token);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.authenticate(connection, testVerifiedPeer()));
    TrustedAuthority target = authority(999);
    TEST_ASSERT_FALSE(access.route(connection, target));
    TEST_ASSERT_EQUAL_UINT32(999, target.epoch);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.disconnect(connection));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.connect(0, connection));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, connection.token);
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(target));
}
void test_identity_route_rechecks_actual_channel_and_preserves_false_target() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    const auto route = assignedRoute(access, connection);
    host_rtos::beforeEnter = [] { f->channel.actual.suspendAuthority(); };
    TrustedAuthority target = authority(999);
    TEST_ASSERT_FALSE(access.route(connection, target));
    TEST_ASSERT_EQUAL_UINT32(999, target.epoch);
    accessOutcome(IdentityAccessOutcome::Duplicate,
                  access.authenticate(connection, testVerifiedPeer()));
    accessOutcome(IdentityAccessOutcome::Duplicate, access.assignGameMaster(testIdentity(2), 7));
    TEST_ASSERT_FALSE(access.route(connection, target));
    TEST_ASSERT_FALSE(f->submit(f->command(), route.epoch, route.principal));
}
void test_delayed_verification_cannot_reopen_revoked_channel_at_publish_point() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    host_rtos::beforeEnter = [] { f->channel.actual.suspendAuthority(); };
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.authenticate(connection, testVerifiedPeer()));
    TrustedAuthority route = authority(999);
    TEST_ASSERT_FALSE(access.route(connection, route));
    TEST_ASSERT_EQUAL_UINT32(999, route.epoch);
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(route));
    accessOutcome(IdentityAccessOutcome::SecurityRejected,
                  access.authenticate(connection, testVerifiedPeer()));
}
void test_delayed_local_assignment_cannot_reopen_revoked_verified_link() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    host_rtos::beforeEnter = [] { f->channel.actual.suspendAuthority(); };
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.assignGameMaster(testIdentity(2), 7));
    TrustedAuthority route = authority(999);
    TEST_ASSERT_FALSE(access.route(connection, route));
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(route));
}
