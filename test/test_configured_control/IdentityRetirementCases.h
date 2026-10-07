#pragma once
// Fault/lifecycle replacement model: the channel has a newer trusted producer
// while an old serialized identity owner still holds a queued disconnect.
void test_identity_old_disconnect_preserves_replacement_and_actual_game_control() {
    IdentityAccess oldOwner(f->channel.actual);
    BoundConnection connection;
    const auto oldBinding = approvedGattBinding(oldOwner, connection);
    const auto replacement = authority(oldBinding.authority.epoch + 1, true, 99);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(replacement)));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, oldOwner.disconnect(connection));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
    f->control.service(++f->now);
    auto command = f->command();
    TEST_ASSERT_TRUE(f->submit(command, replacement.epoch, replacement.principal));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(99, reply.principal);
    TEST_ASSERT_EQUAL_UINT32(replacement.epoch, reply.connectionEpoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    BoundConnection denied;
    denied.token = 99;
    const auto reopening = oldOwner.connect(connection.handle, denied);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, reopening);
    TEST_ASSERT_EQUAL_UINT32(99, denied.token);
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    TEST_ASSERT_EQUAL_UINT32(replacement.epoch, status.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(replacement.principal, status.principal);
    char wire[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeConfiguredStatus(status.status, wire, sizeof(wire), written)));
    sdkFixture("retirement-status-v2.json", std::string(wire, written) + char(10));
    char observation[768];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_identity_retirement\",\"oldConnectionToken\":%lu,"
        "\"oldAuthorityEpoch\":%lu,\"replacementEpoch\":%lu,\"currentEpoch\":%lu,"
        "\"closeOutcome\":\"authority_unavailable\",\"oldOwnerRetired\":true,"
        "\"newOwnerExecuted\":%s,\"replyPrincipal\":%lu,\"replyEpoch\":%lu,"
        "\"statusEpoch\":%lu,\"lease\":%lu,\"rfInitialized\":false,\"sdkOriginProven\":false}\n",
        static_cast<unsigned long>(connection.token),
        static_cast<unsigned long>(oldBinding.authority.epoch),
        static_cast<unsigned long>(replacement.epoch), static_cast<unsigned long>(current.epoch),
        f->application.gameRunning() ? "true" : "false",
        static_cast<unsigned long>(reply.principal),
        static_cast<unsigned long>(reply.connectionEpoch),
        static_cast<unsigned long>(status.connectionEpoch),
        static_cast<unsigned long>(status.status.controlLease));
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observation));
    sdkFixture("retirement-observation.json", std::string(observation, count));
}
void test_identity_retired_owner_cannot_regrant_reconnect_or_reset() {
    IdentityAccess oldOwner(f->channel.actual);
    BoundConnection connection;
    const auto oldBinding = approvedGattBinding(oldOwner, connection);
    const auto replacement = authority(oldBinding.authority.epoch + 1, true, 99);
    f->channel.actual.updateAuthority(replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, oldOwner.disconnect(connection));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, oldOwner.begin(testIdentity(1)));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  oldOwner.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  oldOwner.revokeGameMaster(testIdentity(2)));
    accessOutcome(IdentityAccessOutcome::StaleConnection,
                  oldOwner.authenticate(connection, testVerifiedPeer()));
    TrustedAuthority destination = authority(99);
    TEST_ASSERT_FALSE(oldOwner.route(connection, destination));
    TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
    Identity publicIdentity;
    TEST_ASSERT_TRUE(oldOwner.deviceIdentity(publicIdentity));
    TEST_ASSERT_TRUE(publicIdentity == testIdentity(1));
    BoundConnection next;
    next.token = 99;
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, oldOwner.connect(0, next));
    TEST_ASSERT_EQUAL_UINT32(99, next.token);
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
}
void test_identity_disconnect_uses_role_authority_not_original_connection_token() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    TEST_ASSERT_TRUE(binding.authority.epoch > connection.token);
    accessOutcome(IdentityAccessOutcome::Disconnected, access.disconnect(connection));
    TrustedAuthority closed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(closed));
    TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch + 1, closed.epoch);
    TEST_ASSERT_FALSE(closed.gameMaster);
    TEST_ASSERT_EQUAL_UINT32(0, closed.principal);
    BoundConnection next;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(connection.handle, next));
    TEST_ASSERT_TRUE(next.token > closed.epoch);
    accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(next, testVerifiedPeer()));
    TrustedAuthority route;
    TEST_ASSERT_TRUE(access.route(next, route));
}
void test_authority_retirement_current_allocates_closed_epoch_without_wrap() {
    AuthorityState state;
    const auto original = authority(8);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(original)));
    TrustedAuthority closed;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.closeIfCurrent(original, closed)));
    TEST_ASSERT_EQUAL_UINT32(9, closed.epoch);
    TEST_ASSERT_FALSE(closed.gameMaster);
    TEST_ASSERT_EQUAL_UINT32(0, closed.principal);
    TEST_ASSERT_FALSE(state.admits(original.principal, original.epoch));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(state.read(current));
    TEST_ASSERT_TRUE(current == closed);
}
void test_authority_retirement_full_observation_mismatch_preserves_current_and_destination() {
    AuthorityState state;
    const auto current = authority(8);
    state.update(current);
    for (unsigned i = 0; i < 4; ++i) {
        auto wrong = current;
        if (i == 0)
            wrong.epoch = 7;
        if (i == 1)
            ++wrong.principal;
        if (i == 2)
            wrong.gameMaster = false;
        if (i == 3)
            wrong.epoch = UINT32_MAX;
        TrustedAuthority destination = authority(99);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Conflict),
                          static_cast<unsigned>(state.closeIfCurrent(wrong, destination)));
        TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
        TrustedAuthority observed;
        TEST_ASSERT_TRUE(state.read(observed));
        TEST_ASSERT_TRUE(observed == current);
        TEST_ASSERT_TRUE(state.admits(current.principal, current.epoch));
    }
    TrustedAuthority closed;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.closeIfCurrent(current, closed)));
    TEST_ASSERT_EQUAL_UINT32(9, closed.epoch); // Bad future expectation did not fence highest.
}
void test_authority_retirement_unavailable_maximum_and_grant_policy_are_separate() {
    AuthorityState state;
    TrustedAuthority destination = authority(99);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.closeIfCurrent(authority(1), destination)));
    TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
    const auto last = authority(UINT32_MAX);
    state.update(last);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.closeIfCurrent(last, destination)));
    TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
    TEST_ASSERT_FALSE(state.read(destination));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.advance(7, true, destination)));
    AuthorityState grant;
    grant.update(authority(2));
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Conflict),
        static_cast<unsigned>(grant.advanceIfCurrent(authority(1), 7, true, destination)));
    TEST_ASSERT_TRUE(grant.read(destination)); // Preserve the newer producer.
    TEST_ASSERT_TRUE(destination == authority(2));
}
void test_authority_retirement_adapter_freezes_expected_before_lock_observation() {
    auto expected = authority(1);
    static TrustedAuthority *caller;
    caller = &expected;
    host_rtos::beforeEnter = [] {
        *caller = authority(2, true, 99);
        f->channel.actual.updateAuthority(*caller);
    };
    TrustedAuthority destination = authority(99);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Conflict),
        static_cast<unsigned>(f->channel.actual.closeAuthorityIfCurrent(expected, destination)));
    caller = nullptr;
    TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == authority(2, true, 99));
}
void test_identity_disconnect_rechecks_replacement_at_atomic_adapter_boundary() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    static TrustedAuthority replacement;
    replacement = authority(binding.authority.epoch + 1, true, 99);
    host_rtos::beforeEnter = [] { f->channel.actual.updateAuthority(replacement); };
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.disconnect(connection));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.begin(testIdentity(1)));
}
void test_identity_suspended_disconnect_can_reconnect_with_fresh_original_token() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    f->channel.actual.suspendAuthority();
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.disconnect(connection));
    BoundConnection next;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(connection.handle, next));
    TEST_ASSERT_TRUE(next.token > binding.authority.epoch);
    accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(next, testVerifiedPeer()));
    TrustedAuthority route;
    TEST_ASSERT_TRUE(access.route(next, route));
    accessOutcome(IdentityAccessOutcome::StaleConnection, access.disconnect(connection));
    TrustedAuthority observed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(observed));
    TEST_ASSERT_TRUE(observed == route);
}
void test_authority_retirement_uninitialized_adapter_preserves_destination() {
    FreeRtosGameControlChannel unopened;
    TrustedAuthority destination = authority(99);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Suspended),
        static_cast<unsigned>(unopened.closeAuthorityIfCurrent(authority(1), destination)));
    TEST_ASSERT_EQUAL_UINT32(99, destination.epoch);
}
void test_identity_retirement_stale_copied_frame_cannot_execute_or_close_new_gm() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto accepted = admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual,
                                          input, responses, f->now);
    TEST_ASSERT_TRUE(accepted.responseCopied);
    const auto replacement = authority(binding.authority.epoch + 1, true, 99);
    f->channel.actual.updateAuthority(replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable, access.disconnect(connection));
    CommandStream stream(f->channel.actual);
    const auto dispatched = dispatchCommandFrame(input, stream, ++f->now);
    TEST_ASSERT_TRUE(dispatched.received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Unauthorized),
                      static_cast<unsigned>(dispatched.stream.outcome));
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
}
void test_identity_old_security_failure_preserves_replacement_authority() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    const auto replacement = authority(binding.authority.epoch + 1, true, 99);
    f->channel.actual.updateAuthority(replacement);
    auto evidence = testVerifiedPeer();
    evidence.encrypted = false;
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.authenticate(connection, evidence));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.revokeGameMaster(testIdentity(2)));
}
void test_identity_old_role_revocation_preserves_replacement_authority() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    const auto replacement = authority(binding.authority.epoch + 1, true, 99);
    f->channel.actual.updateAuthority(replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.revokeGameMaster(testIdentity(2)));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
    accessOutcome(IdentityAccessOutcome::AuthorityUnavailable,
                  access.assignGameMaster(testIdentity(2), 7));
}
