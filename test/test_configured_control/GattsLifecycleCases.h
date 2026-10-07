#pragma once
class LifecycleClock : public IProfileImportClock {
  public:
    std::uint32_t nowMilliseconds() const noexcept override {
        return f->now;
    }
};
LifecycleClock lifecycleClock;
GattsConnectionEvent lifecycleEvent(std::uint16_t handle = 0) {
    GattsConnectionEvent value;
    value.interface = 1;
    value.connection = handle;
    value.peerAddress = {{1, 2, 3, 4, 5, 6}};
    return value;
}
GattsEndpoints lifecycleEndpoints() {
    GattsEndpoints endpoints;
    endpoints.interface = 1;
    endpoints.command = 0x42;
    endpoints.profile = 0x43;
    return endpoints;
}
void lifecycleOutcome(GattsLifecycleOutcome expected, GattsLifecycleOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
GattsWriteBinding openLifecycle(IdentityAccess &identity, FreeRtosGattsConnection &owner,
                                BoundConnection &connection, bool gm = true) {
    accessOutcome(IdentityAccessOutcome::Initialized, identity.begin(testIdentity(1)));
    if (gm)
        accessOutcome(IdentityAccessOutcome::Assigned,
                      identity.assignGameMaster(testIdentity(2), 7));
    lifecycleOutcome(GattsLifecycleOutcome::Opened, owner.connectNow(lifecycleEvent(), connection));
    if (gm)
        accessOutcome(IdentityAccessOutcome::Verified,
                      identity.authenticate(connection, testVerifiedPeer()));
    GattsWriteBinding expected;
    expected.interface = 1;
    expected.attribute = 0x42;
    expected.connection = connection;
    expected.peerAddress = lifecycleEvent().peerAddress;
    GattsWriteBinding actual;
    TEST_ASSERT_TRUE(
        owner.captureWriteNow(gattEvent(expected, "123456789"), GattsEndpoint::Command, actual));
    f->control.service(++f->now);
    return actual;
}
void test_lifecycle_closed_sdk_context_grants_no_authority_but_copies_att_error() {
    const auto document = json(f->command()); // Valid before the closed-link lease is zero.
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection, false);
    TEST_ASSERT_FALSE(binding.authority.gameMaster);
    TEST_ASSERT_EQUAL_UINT32(0, binding.authority.principal);
    FreeRtosGattsResponseQueue responses(owner);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(responses.begin() && commands.begin());
    const auto value = frameBytes(document, 0, 12);
    const auto result = admitOriginalGattsWrite(gattEvent(binding, value), owner, f->channel.actual,
                                                commands, responses, f->now);
    gattOutcome(GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization, result);
    TEST_ASSERT_TRUE(result.responseCopied);
    RoutedWriteResponse response;
    TEST_ASSERT_TRUE(responses.takeCurrent(response));
    TEST_ASSERT_TRUE(response.original.connection == connection);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_lifecycle_actual_frames_start_and_original_status_fixture() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    FreeRtosGattsResponseQueue responses(owner);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(responses.begin() && commands.begin());
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    unsigned copies = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto size = std::min<std::size_t>(12, document.size() - offset);
        auto bytes = frameBytes(document, offset, size);
        const auto result = admitOriginalGattsWrite(gattEvent(binding, bytes), owner,
                                                    f->channel.actual, commands, responses, f->now);
        TEST_ASSERT_TRUE(result.responseCopied);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        bytes.assign(bytes.size(), '\0');
        RoutedWriteResponse response;
        TEST_ASSERT_TRUE(responses.takeCurrent(response));
        TEST_ASSERT_TRUE(response.original.connection == connection);
        TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, response.original.authority.epoch);
        const auto dispatch = dispatchCommandFrame(commands, stream, f->now);
        TEST_ASSERT_TRUE(dispatch.received);
        offset += size;
        ++copies;
    }
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, status.connectionEpoch);
    const auto afterStartNextRequest = status.status.nextRequest;
    const auto oldBytes = frameBytes(document, 0, 12);
    TEST_ASSERT_TRUE(admitOriginalGattsWrite(gattEvent(binding, oldBytes), owner, f->channel.actual,
                                             commands, responses, f->now)
                         .responseCopied);
    lifecycleOutcome(GattsLifecycleOutcome::Closed, owner.disconnectNow(lifecycleEvent()));
    BoundConnection replacement;
    lifecycleOutcome(GattsLifecycleOutcome::Opened,
                     owner.connectNow(lifecycleEvent(), replacement));
    accessOutcome(IdentityAccessOutcome::Verified,
                  identity.authenticate(replacement, testVerifiedPeer()));
    lifecycleOutcome(GattsLifecycleOutcome::Ignored, owner.retireOriginal(binding));
    RoutedWriteResponse stale;
    stale.transaction = 99;
    TEST_ASSERT_FALSE(responses.takeCurrent(stale));
    TEST_ASSERT_EQUAL_UINT32(99, stale.transaction);
    const auto oldDispatch = dispatchCommandFrame(commands, stream, ++f->now);
    TEST_ASSERT_TRUE(oldDispatch.received);
    streamOutcome(CommandStreamOutcome::Unauthorized, oldDispatch.stream);
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    GattsWriteBinding current;
    TEST_ASSERT_TRUE(
        owner.captureWriteNow(gattEvent(binding, oldBytes), GattsEndpoint::Command, current));
    TEST_ASSERT_TRUE(current.connection == replacement);
    TEST_ASSERT_TRUE(replacement.token > binding.authority.epoch);
    TEST_ASSERT_EQUAL_UINT32(current.authority.epoch, status.connectionEpoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    char wire[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeConfiguredStatus(status.status, wire, sizeof(wire), written)));
    sdkFixture("lifecycle-status-v2.json", std::string(wire, written) + "\n");
    char observation[512];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_callback_lifecycle\",\"frameCopies\":%u,\"originalToken\":%lu,"
        "\"authorityEpoch\":%lu,\"replacementToken\":%lu,\"replacementEpoch\":%lu,"
        "\"statusEpoch\":%lu,\"afterStartNextRequest\":%lu,\"staleOutputDiscarded\":true,"
        "\"oldRetirementIgnored\":true,\"sameSdkHandle\":%s,\"running\":true,"
        "\"sdkOrderingProven\":false,\"rfInitialized\":false}\n",
        copies, static_cast<unsigned long>(connection.token),
        static_cast<unsigned long>(binding.authority.epoch),
        static_cast<unsigned long>(replacement.token),
        static_cast<unsigned long>(current.authority.epoch),
        static_cast<unsigned long>(status.connectionEpoch),
        static_cast<unsigned long>(afterStartNextRequest),
        replacement.handle == connection.handle ? "true" : "false");
    TEST_ASSERT_TRUE(count > 0 && std::size_t(count) < sizeof(observation));
    sdkFixture("lifecycle-observation.json", std::string(observation, count));
}
void test_lifecycle_reused_handle_rejects_original_queued_output_and_old_retirement() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection old;
    const auto binding = openLifecycle(identity, owner, old);
    FreeRtosGattsResponseQueue responses(owner);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(responses.begin() && commands.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    TEST_ASSERT_TRUE(admitOriginalGattsWrite(gattEvent(binding, bytes), owner, f->channel.actual,
                                             commands, responses, f->now)
                         .responseCopied);
    lifecycleOutcome(GattsLifecycleOutcome::Closed, owner.disconnectNow(lifecycleEvent()));
    BoundConnection current;
    lifecycleOutcome(GattsLifecycleOutcome::Opened, owner.connectNow(lifecycleEvent(), current));
    TEST_ASSERT_TRUE(current.token > old.token);
    TEST_ASSERT_EQUAL_UINT16(old.handle, current.handle);
    accessOutcome(IdentityAccessOutcome::Verified,
                  identity.authenticate(current, testVerifiedPeer()));
    TEST_ASSERT_FALSE(owner.originalLive(binding));
    lifecycleOutcome(GattsLifecycleOutcome::Ignored, owner.retireOriginal(binding));
    RoutedWriteResponse output;
    output.transaction = 99;
    TEST_ASSERT_FALSE(responses.takeCurrent(output));
    TEST_ASSERT_EQUAL_UINT32(99, output.transaction);
    CommandStream stream(f->channel.actual);
    const auto dispatch = dispatchCommandFrame(commands, stream, f->now);
    TEST_ASSERT_TRUE(dispatch.received);
    streamOutcome(CommandStreamOutcome::Unauthorized, dispatch.stream);
    TrustedAuthority currentRoute;
    TEST_ASSERT_TRUE(identity.route(current, currentRoute));
}
void test_lifecycle_wrong_context_and_bad_endpoints_preserve_outputs() {
    IdentityAccess identity(f->channel.actual);
    auto endpoints = lifecycleEndpoints();
    FreeRtosGattsConnection owner(identity, lifecycleClock, endpoints);
    endpoints.command = 0;
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    for (unsigned i = 0; i < 6; ++i) {
        auto event = gattEvent(binding, bytes);
        if (i == 0)
            ++event.interface;
        if (i == 1)
            ++event.connection;
        if (i == 2)
            ++event.attribute;
        if (i == 3)
            ++event.peerAddress[0];
        auto endpoint = i == 4   ? GattsEndpoint::Profile
                        : i == 5 ? static_cast<GattsEndpoint>(99)
                                 : GattsEndpoint::Command;
        GattsWriteBinding output;
        output.connection.token = 99;
        TEST_ASSERT_FALSE(owner.captureWriteNow(event, endpoint, output));
        TEST_ASSERT_EQUAL_UINT32(99, output.connection.token);
    }
    auto wrong = lifecycleEvent();
    ++wrong.connection;
    lifecycleOutcome(GattsLifecycleOutcome::Ignored, owner.disconnectNow(wrong));
    TEST_ASSERT_TRUE(owner.originalLive(binding));
    IdentityAccess other(f->channel.actual);
    endpoints = lifecycleEndpoints();
    endpoints.profile = endpoints.command;
    FreeRtosGattsConnection invalid(other, lifecycleClock, endpoints);
    BoundConnection output;
    output.token = 99;
    lifecycleOutcome(GattsLifecycleOutcome::Invalid, invalid.connectNow(lifecycleEvent(), output));
    TEST_ASSERT_EQUAL_UINT32(99, output.token);
}
void test_lifecycle_unexpected_connect_closes_old_transport_and_rights() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    BoundConnection output;
    output.token = 99;
    lifecycleOutcome(GattsLifecycleOutcome::Conflict, owner.connectNow(lifecycleEvent(), output));
    TEST_ASSERT_EQUAL_UINT32(99, output.token);
    TEST_ASSERT_FALSE(owner.originalLive(binding));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_FALSE(current.gameMaster);
    TEST_ASSERT_TRUE(current.epoch > binding.authority.epoch);
}
void test_lifecycle_freezes_callback_bytes_before_authority_observation() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(commands.begin());
    FreeRtosGattsResponseQueue responses(owner);
    TEST_ASSERT_TRUE(responses.begin());
    auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto original = bytes;
    static std::string *borrowed;
    borrowed = &bytes;
    host_rtos::beforeEnter = [] { borrowed->assign(borrowed->size(), 'X'); };
    const auto result = admitOriginalGattsWrite(gattEvent(binding, bytes), owner, f->channel.actual,
                                                commands, responses, f->now);
    borrowed = nullptr;
    TEST_ASSERT_TRUE(result.responseCopied);
    CopiedCommandFrame copied;
    TEST_ASSERT_TRUE(commands.take(copied));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), copied.bytes.data(), original.size());
}
void test_lifecycle_retirement_closes_transport_before_external_authority_observation() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    // An observation hook on the channel read/close must see transport closed.
    // First owner lock gets a hook that installs the later policy-lock hook.
    static FreeRtosGattsConnection *liveOwner;
    static GattsWriteBinding old;
    static bool observedClosed;
    liveOwner = &owner;
    old = binding;
    observedClosed = false;
    host_rtos::beforeEnter = [] {
        host_rtos::beforeEnter = [] { observedClosed = !liveOwner->originalLive(old); };
    };
    lifecycleOutcome(GattsLifecycleOutcome::Closed, owner.retireOriginal(binding));
    liveOwner = nullptr;
    TEST_ASSERT_TRUE(observedClosed);
    TEST_ASSERT_FALSE(owner.originalLive(binding));
}
void test_lifecycle_current_role_revoke_keeps_error_response_context() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    accessOutcome(IdentityAccessOutcome::Revoked, identity.revokeGameMaster(testIdentity(2)));
    TEST_ASSERT_TRUE(owner.originalLive(binding));
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    FreeRtosGattsResponseQueue responses(owner);
    TEST_ASSERT_TRUE(commands.begin() && responses.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto result = admitOriginalGattsWrite(gattEvent(binding, bytes), owner, f->channel.actual,
                                                commands, responses, f->now);
    gattOutcome(GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization, result);
    TEST_ASSERT_TRUE(result.responseCopied);
    RoutedWriteResponse response;
    TEST_ASSERT_TRUE(responses.takeCurrent(response));
    TEST_ASSERT_FALSE(response.original.authority.gameMaster);
}
void test_lifecycle_profile_endpoint_uses_actual_independent_queue() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    auto binding = openLifecycle(identity, owner, connection);
    binding.attribute = 0x43;
    FreeRtosProfileFrameQueue profiles(f->channel.actual);
    FreeRtosGattsResponseQueue responses(owner);
    TEST_ASSERT_TRUE(profiles.begin() && responses.begin());
    const auto bytes = queuedProfileEnvelope(importExample());
    const auto result = admitOriginalGattsProfileWrite(
        gattEvent(binding, bytes), owner, f->channel.actual, profiles, responses, f->now);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    CopiedProfileFrame output;
    TEST_ASSERT_TRUE(profiles.take(output));
    TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, output.epoch);
    TEST_ASSERT_EQUAL_MEMORY(bytes.data(), output.bytes.data(), bytes.size());
}
void test_lifecycle_failed_replacement_closes_transport_preserving_new_authority() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    const auto replacement = authority(binding.authority.epoch + 1, true, 99);
    f->channel.actual.updateAuthority(replacement);
    lifecycleOutcome(GattsLifecycleOutcome::Unavailable, owner.retireOriginal(binding));
    TEST_ASSERT_FALSE(owner.originalLive(binding));
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(current == replacement);
}
void test_lifecycle_reentrant_callback_does_not_relabel_original() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    static FreeRtosGattsConnection *liveOwner;
    static GattsLifecycleOutcome result;
    liveOwner = &owner;
    host_rtos::beforeEnter = [] { result = liveOwner->disconnectNow(lifecycleEvent()); };
    GattsWriteBinding output;
    TEST_ASSERT_TRUE(
        owner.captureWriteNow(gattEvent(binding, "123456789"), GattsEndpoint::Command, output));
    liveOwner = nullptr;
    lifecycleOutcome(GattsLifecycleOutcome::Busy, result);
    TEST_ASSERT_TRUE(owner.originalLive(output));
}
void test_lifecycle_first_time_is_not_refreshed_by_capture() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    FreeRtosGattsResponseQueue responses(owner);
    TEST_ASSERT_TRUE(commands.begin() && responses.begin());
    const auto first = f->now;
    f->now += GattsResponseLimits::MaxAgeMilliseconds;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto result = admitOriginalGattsWrite(gattEvent(binding, bytes), owner, f->channel.actual,
                                                commands, responses, first);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_FALSE(result.responseCopied);
    CopiedCommandFrame value;
    TEST_ASSERT_TRUE(commands.take(value));
    TEST_ASSERT_EQUAL_UINT32(first, value.receivedAt);
}

void test_lifecycle_original_live_freezes_binding_before_lock() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    auto binding = openLifecycle(identity, owner, connection);
    static GattsWriteBinding *borrowed;
    borrowed = &binding;
    host_rtos::beforeEnter = [] { ++borrowed->connection.token; };
    TEST_ASSERT_TRUE(owner.originalLive(binding));
    borrowed = nullptr;
    TEST_ASSERT_FALSE(owner.originalLive(binding));
}
void test_lifecycle_payload_bounds_preparation_and_offset_retain_original_error_context() {
    IdentityAccess identity(f->channel.actual);
    FreeRtosGattsConnection owner(identity, lifecycleClock, lifecycleEndpoints());
    BoundConnection connection;
    const auto binding = openLifecycle(identity, owner, connection);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    FreeRtosGattsResponseQueue responses(owner);
    TEST_ASSERT_TRUE(commands.begin() && responses.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    for (unsigned i = 0; i < 4; ++i) {
        auto event = gattEvent(binding, bytes);
        const auto expected = i == 0 || i == 1 ? AttWriteStatus::InvalidValueLength
                              : i == 2         ? AttWriteStatus::RequestUnsupported
                                               : AttWriteStatus::InvalidOffset;
        if (i == 0) {
            event.count = 129;
            event.bytes = reinterpret_cast<const char *>(1);
        }
        if (i == 1)
            event.bytes = nullptr;
        if (i == 2)
            event.prepared = true;
        if (i == 3)
            event.offset = 1;
        const auto result =
            admitOriginalGattsWrite(event, owner, f->channel.actual, commands, responses, f->now);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(result.status));
        TEST_ASSERT_TRUE(result.responseCopied);
        RoutedWriteResponse response;
        TEST_ASSERT_TRUE(responses.takeCurrent(response));
        TEST_ASSERT_TRUE(response.original.connection == connection);
        CopiedCommandFrame output;
        TEST_ASSERT_FALSE(commands.take(output));
    }
}
