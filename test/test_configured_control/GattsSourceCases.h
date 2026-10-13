#pragma once
class SourceClock : public IProfileImportClock {
  public:
    mutable void (*before)() = nullptr;
    mutable void (*after)() = nullptr;
    std::uint32_t nowMilliseconds() const noexcept override {
        if (before) {
            auto hook = before;
            before = nullptr;
            hook();
        }
        const auto value = f->now;
        if (after) {
            auto hook = after;
            after = nullptr;
            hook();
        }
        return value;
    }
};
struct SourceFixture {
    const std::string document = json(f->command());
    IdentityAccess identity{f->channel.actual};
    SourceClock clock;
    FreeRtosCommandFrameQueue commands{f->channel.actual};
    FreeRtosProfileFrameQueue profiles{f->channel.actual};
    Esp32GattsSource source{identity, clock, f->channel.actual, commands, profiles};
    FreeRtosGattsResponseQueue responses{source};
    GattsResponseDispatcher worker{responses, source};
    BoundConnection connection;
    std::uint8_t address[6] = {1, 2, 3, 4, 5, 6};
    SourceFixture() {
        host_sdk_source::reset();
        btu_thread = reinterpret_cast<void *>(2);
        TEST_ASSERT_TRUE(commands.begin() && profiles.begin() && responses.begin());
        accessOutcome(IdentityAccessOutcome::Initialized, identity.begin(testIdentity(1)));
        TEST_ASSERT_TRUE(source.registeredSourceNow(1));
        TEST_ASSERT_TRUE(source.configureEndpointsNow(0x42, 0x43, responses));
    }
    void open(bool gm = true) {
        lifecycleOutcome(GattsLifecycleOutcome::Opened,
                         source.connectionNow(1, address, 0x101, true));
        TEST_ASSERT_TRUE(source.originalConnectionNow(connection));
        if (gm) {
            const auto assigned = identity.assignGameMaster(testIdentity(2), 7);
            TEST_ASSERT_TRUE(assigned == IdentityAccessOutcome::Assigned ||
                             assigned == IdentityAccessOutcome::Duplicate);
            accessOutcome(IdentityAccessOutcome::Verified,
                          identity.authenticate(connection, testVerifiedPeer()));
        }
        f->control.service(++f->now);
    }
    void close() {
        lifecycleOutcome(GattsLifecycleOutcome::Closed,
                         source.connectionNow(1, address, 0x101, false));
    }
    void write(const std::string &bytes, std::uint32_t transaction = 17,
               std::uint16_t attribute = 0x42, bool response = true, bool prepared = false,
               std::uint16_t offset = 0) {
        sdk446::Request request = {};
        request.write.handle = attribute;
        request.write.offset = offset;
        request.write.length = static_cast<std::uint16_t>(bytes.size());
        request.write.needResponse = response;
        request.write.prepared = prepared;
        std::memcpy(request.write.value, bytes.data(),
                    std::min(bytes.size(), sizeof(request.write.value)));
        TEST_ASSERT_TRUE(source.requestNow(0x101, transaction, 2, &request));
        std::memset(&request, 0, sizeof(request)); // Borrowed source union no longer valid.
    }
    void post() {
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Submitted),
                          static_cast<unsigned>(worker.service()));
    }
};
void test_source_original_sdk_post_actual_frames_and_application() {
    SourceFixture sdk;
    sdk.open();
    const auto document = json(f->command());
    CommandStream stream(f->channel.actual);
    unsigned copies = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        sdk.write(frameBytes(document, offset, count), static_cast<std::uint32_t>(copies + 1));
        sdk.post();
        TEST_ASSERT_EQUAL(copies, host_sdk_source::sent.size()); // Posted, not yet consumed.
        host_sdk_source::drainOne();
        TEST_ASSERT_EQUAL(copies + 1, host_sdk_source::sent.size());
        TEST_ASSERT_EQUAL_HEX16(0x101, host_sdk_source::sent.back().original.connection.handle);
        TEST_ASSERT_EQUAL_UINT32(copies + 1, host_sdk_source::sent.back().transaction);
        TEST_ASSERT_EQUAL_UINT8(0, static_cast<unsigned>(host_sdk_source::sent.back().status));
        const auto dispatch = dispatchCommandFrame(sdk.commands, stream, f->now);
        TEST_ASSERT_TRUE(dispatch.received);
        offset += count;
        ++copies;
    }
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    TEST_ASSERT_EQUAL_UINT32(2, status.status.nextRequest);
    char observation[512];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_pinned_source_model\",\"frameCopies\":%u,\"sdkPosts\":%u,"
        "\"sourceCalls\":%u,\"packedConnection\":257,\"firstTransaction\":1,"
        "\"lastTransaction\":%u,\"nextRequest\":2,\"running\":true,"
        "\"rfInitialized\":false,\"deviceMeasured\":false}\n",
        copies, host_sdk_source::posts, unsigned(host_sdk_source::sent.size()), copies);
    TEST_ASSERT_TRUE(count > 0 && std::size_t(count) < sizeof(observation));
    if (std::getenv("LARP_EXPORT_GATT_SOURCE_FIXTURE")) {
        auto *file =
            std::fopen("docs/companion/fixtures/gatts/source-posting-observation.json", "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL(count, std::fwrite(observation, 1, count, file));
        TEST_ASSERT_EQUAL(0, std::fclose(file));
    }
    sdkFixture("source-posting-observation.json", std::string(observation, count));
    sdk.close();
}
void test_source_posted_old_connection_dropped_after_same_handle_reconnect() {
    SourceFixture sdk;
    sdk.open();
    const auto first = sdk.connection;
    sdk.write(frameBytes(json(f->command()), 0, 12));
    sdk.post();
    sdk.close();
    sdk.open();
    TEST_ASSERT_TRUE(sdk.connection.token > first.token);
    host_sdk_source::drain();
    TEST_ASSERT_TRUE(host_sdk_source::sent.empty());
    CommandStream stream(f->channel.actual);
    const auto old = dispatchCommandFrame(sdk.commands, stream, f->now);
    TEST_ASSERT_TRUE(old.received);
    streamOutcome(CommandStreamOutcome::Unauthorized, old.stream);
    sdk.write(frameBytes(json(f->command()), 0, 12), 18);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    TEST_ASSERT_EQUAL_UINT32(18, host_sdk_source::sent.back().transaction);
    sdk.close();
}
void test_source_full_fixed_pool_failed_post_and_slot_reuse() {
    SourceFixture sdk;
    sdk.open(false);
    const auto bytes = frameBytes(sdk.document, 0, 12);
    for (unsigned i = 0; i < 4; ++i) {
        sdk.write(bytes, i);
        sdk.post();
    }
    TEST_ASSERT_EQUAL(4, host_sdk_source::work.size());
    sdk.write(bytes, 99);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Rejected),
                      static_cast<unsigned>(sdk.worker.service()));
    TEST_ASSERT_EQUAL(4, host_sdk_source::posts);
    host_sdk_source::drainOne();
    host_sdk_source::rejectPost = true;
    sdk.write(bytes, 100);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Rejected),
                      static_cast<unsigned>(sdk.worker.service()));
    host_sdk_source::rejectPost = false;
    sdk.write(bytes, 101);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(5, host_sdk_source::sent.size());
    TEST_ASSERT_EQUAL_UINT32(101, host_sdk_source::sent.back().transaction);
    sdk.close();
}
void test_source_deadline_rechecked_after_post_without_refresh() {
    SourceFixture sdk;
    sdk.open(false);
    const auto bytes = frameBytes(sdk.document, 0, 12);
    sdk.write(bytes, 11);
    sdk.post();
    f->now += 1000;
    host_sdk_source::drain();
    TEST_ASSERT_TRUE(host_sdk_source::sent.empty());
    f->now = UINT32_MAX - 3;
    sdk.write(bytes, 12);
    sdk.post();
    f->now = 995;
    host_sdk_source::drain(); // Delta999 across wrap.
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    f->now = UINT32_MAX - 3;
    sdk.write(bytes, 13);
    sdk.post();
    f->now = 996;
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    sdk.close();
}
void test_source_wrong_task_sdk_instance_and_reentrant_consumption() {
    SourceFixture sdk;
    sdk.open(false);
    const auto bytes = frameBytes(sdk.document, 0, 12);
    sdk.write(bytes);
    sdk.post();
    host_sdk_source::currentTask = reinterpret_cast<void *>(3);
    BoundConnection unchanged;
    unchanged.token = 99;
    TEST_ASSERT_FALSE(sdk.source.originalConnectionNow(unchanged));
    TEST_ASSERT_EQUAL_UINT32(99, unchanged.token);
    host_sdk_source::drain();
    TEST_ASSERT_TRUE(host_sdk_source::sent.empty());
    host_sdk_source::currentTask = reinterpret_cast<void *>(1);
    sdk.write(bytes, 18);
    sdk.post();
    btu_thread = reinterpret_cast<void *>(4);
    host_sdk_source::drain();
    TEST_ASSERT_TRUE(host_sdk_source::sent.empty());
    btu_thread = reinterpret_cast<void *>(2);
    sdk.write(bytes, 19);
    sdk.post();
    // SDK local failure is consumed once; it cannot retry the copied input.
    host_sdk_source::sendResult = 0x82;
    host_sdk_source::replayWork = host_sdk_source::work.front();
    host_sdk_source::beforeSend = []() {
        const auto work = host_sdk_source::replayWork;
        work.function(work.context); // Same slot while consuming cannot recurse into SDK.
    };
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    host_sdk_source::synchronous = true;
    sdk.write(bytes, 20);
    sdk.post();
    TEST_ASSERT_EQUAL(2, host_sdk_source::sent.size());
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
    sdk.close();
}
void test_source_revoked_gm_original_copy_ack_never_executes() {
    SourceFixture sdk;
    sdk.open();
    sdk.write(frameBytes(json(f->command()), 0, 12));
    sdk.post();
    accessOutcome(IdentityAccessOutcome::Revoked, sdk.identity.revokeGameMaster(testIdentity(2)));
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<unsigned>(host_sdk_source::sent.back().status));
    CommandStream stream(f->channel.actual);
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  dispatchCommandFrame(sdk.commands, stream, f->now).stream);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    sdk.close();
}
void test_source_null_overlong_prepared_execute_and_unknown_writes() {
    SourceFixture sdk;
    sdk.open();
    TEST_ASSERT_TRUE(sdk.source.requestNow(0x101, 1, 2, nullptr));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Idle),
                      static_cast<unsigned>(sdk.worker.service()));
    sdk.write(std::string(129, 'x'), 2);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL_UINT8(0x0d, static_cast<unsigned>(host_sdk_source::sent.back().status));
    sdk.write("", 3, 0x42, true, true);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL_UINT8(6, static_cast<unsigned>(host_sdk_source::sent.back().status));
    sdk446::Request execute = {};
    execute.execute = 1;
    TEST_ASSERT_TRUE(sdk.source.requestNow(0x101, 4, 3, &execute));
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL_UINT8(6, static_cast<unsigned>(host_sdk_source::sent.back().status));
    sdk.write("123456789", 5, 0x99);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Idle),
                      static_cast<unsigned>(sdk.worker.service()));
    TEST_ASSERT_FALSE(sdk.source.requestNow(0x101, 6, 1, &execute));
    TEST_ASSERT_EQUAL(3, host_sdk_source::sent.size());
    sdk.close();
}
void test_source_owned_write_and_pending_response_survive_external_mutation() {
    SourceFixture sdk;
    sdk.open();
    static sdk446::Request request;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    request = {};
    request.write.handle = 0x42;
    request.write.needResponse = true;
    request.write.length = static_cast<std::uint16_t>(bytes.size());
    std::memcpy(request.write.value, bytes.data(), bytes.size());
    sdk.clock.before = []() { std::memset(&request, 0, sizeof(request)); };
    TEST_ASSERT_TRUE(sdk.source.requestNow(0x101, 23, 2, &request));
    CopiedCommandFrame copied;
    TEST_ASSERT_TRUE(sdk.commands.take(copied));
    TEST_ASSERT_EQUAL_MEMORY(bytes.data(), copied.bytes.data(), bytes.size());
    static RoutedWriteResponse response;
    TEST_ASSERT_TRUE(sdk.responses.takeCurrent(response));
    sdk.clock.before = []() {
        response.transaction = 999;
        response.original.connection.token = 999;
    };
    TEST_ASSERT_TRUE(sdk.source.submitOriginal(response));
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL(1, host_sdk_source::sent.size());
    TEST_ASSERT_EQUAL_UINT32(23, host_sdk_source::sent.back().transaction);
    sdk.close();
}
void test_source_configuration_before_any_link_and_wrong_actor_fail_closed() {
    host_sdk_source::reset();
    IdentityAccess identity(f->channel.actual);
    SourceClock clock;
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    FreeRtosProfileFrameQueue profiles(f->channel.actual);
    Esp32GattsSource source(identity, clock, f->channel.actual, commands, profiles);
    FreeRtosGattsResponseQueue responses(source);
    btu_thread = nullptr;
    TEST_ASSERT_FALSE(source.registeredSourceNow(1));
    btu_thread = reinterpret_cast<void *>(2);
    TEST_ASSERT_FALSE(source.registeredSourceNow(0));
    TEST_ASSERT_TRUE(source.registeredSourceNow(1));
    TEST_ASSERT_FALSE(source.registeredSourceNow(2));
    TEST_ASSERT_FALSE(source.configureEndpointsNow(0x42, 0x42, responses));
    host_sdk_source::currentTask = reinterpret_cast<void *>(3);
    TEST_ASSERT_FALSE(source.configureEndpointsNow(0x42, 0x43, responses));
    host_sdk_source::currentTask = reinterpret_cast<void *>(1);
    std::uint8_t address[6] = {1, 2, 3, 4, 5, 6};
    lifecycleOutcome(GattsLifecycleOutcome::Unavailable,
                     source.connectionNow(1, address, 0x101, true));
    TEST_ASSERT_FALSE(source.configureEndpointsNow(0x42, 0x43, responses));
    BoundConnection output;
    output.token = 99;
    TEST_ASSERT_FALSE(source.originalConnectionNow(output));
    TEST_ASSERT_EQUAL_UINT32(99, output.token);
}
void test_source_closed_context_profile_admission_and_endpoint_immutability() {
    SourceFixture sdk;
    sdk.open(false);
    TEST_ASSERT_FALSE(sdk.source.configureEndpointsNow(0x44, 0x45, sdk.responses));
    ProfileControlFrame control;
    control.transfer = control.request = 1;
    control.operation = ProfileOperation::Finish;
    char bytes[20];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeProfileControl(control, bytes, sizeof(bytes), written));
    sdk.write(std::string(bytes, written), 1, 0x43);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_EQUAL_UINT8(8, static_cast<unsigned>(host_sdk_source::sent.back().status));
    CopiedProfileFrame copied;
    TEST_ASSERT_FALSE(sdk.profiles.take(copied));
    accessOutcome(IdentityAccessOutcome::Assigned,
                  sdk.identity.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  sdk.identity.authenticate(sdk.connection, testVerifiedPeer()));
    sdk.write(std::string(bytes, written), 2, 0x43);
    sdk.post();
    host_sdk_source::drain();
    TEST_ASSERT_TRUE(sdk.profiles.take(copied));
    TEST_ASSERT_EQUAL(written, copied.count);
    TEST_ASSERT_EQUAL_MEMORY(bytes, copied.bytes.data(), written);
    sdk.close();
}
void test_source_registration_wrapper_preserves_other_callbacks_and_captures_before_forward() {
    host_sdk_source::reset();
    static FreeRtosGameControlChannel channel;
    static IdentityAccess identity(channel);
    static FreeRtosCommandFrameQueue commands(channel);
    static FreeRtosProfileFrameQueue profiles(channel);
    class FixedSourceClock : public IProfileImportClock {
      public:
        std::uint32_t nowMilliseconds() const noexcept override {
            return 1;
        }
    };
    static FixedSourceClock clock;
    static Esp32GattsSource source(identity, clock, channel, commands, profiles);
    auto *registered = &source;
    TEST_ASSERT_TRUE(channel.begin());
    btu_thread = nullptr;
    TEST_ASSERT_TRUE(armGattsSourceBeforeSdkStart(0x4c45, *registered));
    TEST_ASSERT_FALSE(armGattsSourceBeforeSdkStart(0x4c45, *registered));
    sdk446::Uuid uuid = {};
    uuid.len = 2;
    uuid.uu.uuid16 = 7;
    sdk446::Callbacks callbacks = {};
    callbacks.connection = &host_sdk_source::passConnection;
    callbacks.request = &host_sdk_source::passRequest;
    callbacks.complete = &host_sdk_source::passComplete;
    callbacks.discovery = &host_sdk_source::passDiscovery;
    callbacks.discoveryComplete = &host_sdk_source::passDiscoveryComplete;
    callbacks.encrypted = &host_sdk_source::passEncrypted;
    callbacks.congestion = &host_sdk_source::passCongestion;
    TEST_ASSERT_EQUAL(1, __wrap_GATT_Register(&uuid, &callbacks));
    TEST_ASSERT_TRUE(host_sdk_source::registeredCallbacks.connection == callbacks.connection);
    uuid.uu.uuid16 = 0x4c45;
    btu_thread = reinterpret_cast<void *>(2);
    TEST_ASSERT_EQUAL(1, __wrap_GATT_Register(&uuid, &callbacks));
    TEST_ASSERT_TRUE(host_sdk_source::registeredCallbacks.connection != callbacks.connection);
    TEST_ASSERT_TRUE(host_sdk_source::registeredCallbacks.request != callbacks.request);
    auto expected = callbacks;
    expected.connection = host_sdk_source::registeredCallbacks.connection;
    expected.request = host_sdk_source::registeredCallbacks.request;
    TEST_ASSERT_EQUAL_MEMORY(&expected, &host_sdk_source::registeredCallbacks, sizeof(expected));
    accessOutcome(IdentityAccessOutcome::Initialized, identity.begin(testIdentity(1)));
    static FreeRtosGattsResponseQueue responses(source);
    TEST_ASSERT_TRUE(commands.begin() && profiles.begin() && responses.begin());
    TEST_ASSERT_TRUE(registered->configureEndpointsNow(0x42, 0x43, responses));
    std::uint8_t address[6] = {1, 2, 3, 4, 5, 6};
    host_sdk_source::registeredCallbacks.connection(1, address, 0x101, true, 0, 2);
    BoundConnection original;
    TEST_ASSERT_TRUE(registered->originalConnectionNow(original));
    TEST_ASSERT_TRUE(original.token > 0);
    sdk446::Request request = {};
    request.write.handle = 0x42;
    request.write.needResponse = true;
    request.write.length = 9;
    std::memcpy(request.write.value, "123456789", 9);
    host_sdk_source::registeredCallbacks.request(0x101, 1, 2, &request);
    TEST_ASSERT_EQUAL(0, host_sdk_source::passedRequests);
    host_sdk_source::registeredCallbacks.request(0x101, 2, 1, &request);
    TEST_ASSERT_EQUAL(1, host_sdk_source::passedRequests);
    host_sdk_source::beforeForward = []() {
        BoundConnection expired;
        TEST_ASSERT_FALSE(source.originalConnectionNow(expired));
    };
    host_sdk_source::registeredCallbacks.connection(1, address, 0x101, false, 0, 2);
    TEST_ASSERT_FALSE(registered->originalConnectionNow(original));
    TEST_ASSERT_EQUAL(2, host_sdk_source::passedConnections);
    TEST_ASSERT_EQUAL(0, __wrap_GATT_Register(&uuid, &callbacks));
    TEST_ASSERT_EQUAL(2, host_sdk_source::registrations);
    // The hook is not called again in this test process. No device run claimed.
}
