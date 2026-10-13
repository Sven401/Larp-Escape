#pragma once
GattsWriteBinding approvedGattBinding(IdentityAccess &access, BoundConnection &connection) {
    openIdentity(access, connection);
    accessOutcome(IdentityAccessOutcome::Assigned, access.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  access.authenticate(connection, testVerifiedPeer()));
    GattsWriteBinding binding;
    binding.connection = connection;
    binding.authority = assignedRoute(access, connection);
    binding.interface = 1;
    binding.attribute = 0x42;
    binding.peerAddress = {{1, 2, 3, 4, 5, 6}};
    f->control.service(++f->now);
    return binding;
}
GattsWriteEvent gattEvent(const GattsWriteBinding &binding, const std::string &bytes) {
    GattsWriteEvent event;
    event.interface = binding.interface;
    event.connection = binding.connection.handle;
    event.attribute = binding.attribute;
    event.peerAddress = binding.peerAddress;
    event.bytes = bytes.data();
    event.count = bytes.size();
    event.transaction = 17;
    event.needResponse = true;
    return event;
}
class TestWriteResponse : public IWriteResponseBearer {
  public:
    GattsWriteBinding live;
    RoutedWriteResponse last, attempted;
    bool available = true, revokeBeforeCopy = false;
    unsigned calls = 0, copies = 0;
    bool copyWriteResponse(const RoutedWriteResponse &response) noexcept override {
        ++calls;
        attempted = response;
        if (revokeBeforeCopy) {
            revokeBeforeCopy = false;
            f->channel.actual.suspendAuthority();
        }
        if (!available || !(response.original.connection == live.connection) ||
            response.original.interface != live.interface ||
            response.original.peerAddress != live.peerAddress)
            return false;
        last = response;
        ++copies;
        return true;
    }
};
void gattOutcome(GattsWriteOutcome expected, AttWriteStatus status,
                 const GattsWriteResult &result) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(result.outcome));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(status), static_cast<unsigned>(result.status));
}
void sdkFixture(const char *name, const std::string &value) {
    const auto path = std::string("docs/companion/fixtures/gatts/") + name;
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *file = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(value.size(), std::fwrite(value.data(), 1, value.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path.c_str(), "rt");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(value.c_str(), actual.c_str());
}
void test_gatts_complete_frames_actual_application_and_independent_tool_fixture() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestWriteResponse responses;
    responses.live = binding;
    const auto document = json(f->command());
    std::string frames;
    unsigned copied = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        auto bytes = frameBytes(document, offset, count);
        const auto event = gattEvent(binding, bytes);
        const auto accepted =
            admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, accepted);
        TEST_ASSERT_TRUE(accepted.responseWanted && accepted.responseCopied);
        TEST_ASSERT_EQUAL_UINT32(event.transaction, responses.last.transaction);
        for (const auto byte : bytes) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned char>(byte));
            frames += hex;
        }
        frames += '\n';
        bytes.assign(bytes.size(), '\0'); // Both queue and response own copies.
        const auto dispatched = dispatchCommandFrame(queue, stream, f->now);
        TEST_ASSERT_TRUE(dispatched.received);
        TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, dispatched.epoch);
        TEST_ASSERT_EQUAL_UINT32(offset, dispatched.input.offset);
        streamOutcome(offset + count == document.size() ? CommandStreamOutcome::Queued
                                                        : CommandStreamOutcome::Receiving,
                      dispatched.stream);
        TEST_ASSERT_FALSE(f->application.gameRunning());
        TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
        offset += count;
        ++copied;
    }
    TEST_ASSERT_EQUAL_UINT32(13, copied);
    TEST_ASSERT_EQUAL_UINT32(copied, responses.copies);
    sdkFixture("command-start-v2.json", document + "\n");
    sdkFixture("command-frames-v1.txt", frames);
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    char observed[512];
    const int count = std::snprintf(
        observed, sizeof(observed),
        "{\"scope\":\"host_observation\",\"frameCopies\":%u,\"responseCopies\":%u,"
        "\"beforeOwnerService\":{\"running\":false,\"nextRequest\":1},"
        "\"afterOwnerService\":{\"running\":%s,\"outcome\":\"%s\",\"request\":%lu},"
        "\"peerDeliveryMeasured\":false,\"rfInitialized\":false}\n",
        copied, responses.copies, f->application.gameRunning() ? "true" : "false",
        gameReplyOutcomeName(reply.reply.outcome), static_cast<unsigned long>(reply.reply.request));
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observed));
    sdkFixture("admission-observation.json", std::string(observed, count));
}
void test_gatts_wrong_sdk_context_never_copies_or_responds_on_other_peer() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    for (unsigned bad = 0; bad < 3; ++bad) {
        auto event = gattEvent(binding, bytes);
        if (bad == 0)
            ++event.interface;
        if (bad == 1)
            ++event.connection;
        if (bad == 2)
            ++event.peerAddress[5];
        const auto accepted =
            admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now);
        TEST_ASSERT_FALSE(accepted.responseWanted || accepted.responseCopied);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(bad ? GattsWriteOutcome::WrongConnection
                                                    : GattsWriteOutcome::WrongInterface),
                          static_cast<unsigned>(accepted.outcome));
    }
    TEST_ASSERT_EQUAL_UINT32(0, responses.calls);
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(queue.take(copied));
}
void test_gatts_missing_original_binding_does_not_infer_current_authority() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    GattsWriteBinding binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto event = gattEvent(binding, bytes);
    gattOutcome(GattsWriteOutcome::NoBinding, AttWriteStatus::InsufficientAuthorization,
                admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now));
    TEST_ASSERT_EQUAL_UINT32(0, responses.calls);
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(queue.take(copied));
}
void test_gatts_attribute_prepare_offset_length_and_header_errors_are_explicit() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const GattsWriteOutcome expected[] = {
        GattsWriteOutcome::WrongAttribute, GattsWriteOutcome::PreparedUnsupported,
        GattsWriteOutcome::InvalidOffset,  GattsWriteOutcome::InvalidFrame,
        GattsWriteOutcome::InvalidFrame,   GattsWriteOutcome::InvalidFrame,
        GattsWriteOutcome::InvalidFrame};
    const AttWriteStatus statuses[] = {
        AttWriteStatus::InvalidHandle,      AttWriteStatus::RequestUnsupported,
        AttWriteStatus::InvalidOffset,      AttWriteStatus::InvalidValueLength,
        AttWriteStatus::InvalidValueLength, AttWriteStatus::InvalidValueLength,
        AttWriteStatus::InvalidValueLength};
    for (unsigned bad = 0; bad < 7; ++bad) {
        auto event = gattEvent(binding, bytes);
        if (bad == 0)
            ++event.attribute;
        if (bad == 1)
            event.prepared = true;
        if (bad == 2)
            event.offset = 1;
        if (bad == 3)
            event.bytes = nullptr;
        if (bad == 4)
            event.count = 0;
        if (bad == 5)
            event.count = 8;
        if (bad == 6)
            event.count = 129;
        const auto result =
            admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now);
        gattOutcome(expected[bad], statuses[bad], result);
        TEST_ASSERT_TRUE(result.responseCopied);
    }
    auto badHeader = bytes;
    badHeader[1] = 4;
    gattOutcome(GattsWriteOutcome::UnsupportedFrame, AttWriteStatus::RequestUnsupported,
                admitGattsWrite(gattEvent(binding, badHeader), binding, f->channel.actual, queue,
                                responses, f->now));
    badHeader = bytes;
    badHeader[2] = badHeader[3] = 0;
    gattOutcome(GattsWriteOutcome::InvalidFrame, AttWriteStatus::InvalidPdu,
                admitGattsWrite(gattEvent(binding, badHeader), binding, f->channel.actual, queue,
                                responses, f->now));
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(queue.take(copied));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_queue_full_reports_resources_without_positive_copy_or_retry() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto event = gattEvent(binding, bytes);
    for (unsigned i = 0; i < CommandFrameQueueLimits::Depth; ++i)
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok,
                    admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now));
    const auto rejected =
        admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now);
    gattOutcome(GattsWriteOutcome::QueueFull, AttWriteStatus::InsufficientResources, rejected);
    TEST_ASSERT_TRUE(rejected.responseCopied);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AttWriteStatus::InsufficientResources),
                      static_cast<unsigned>(responses.last.status));
    CopiedCommandFrame copied;
    unsigned taken = 0;
    while (queue.take(copied))
        ++taken;
    TEST_ASSERT_EQUAL_UINT32(CommandFrameQueueLimits::Depth, taken);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_response_loss_preserves_copied_input_without_reexecution() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    responses.available = false;
    const auto document = json(f->command());
    auto bytes = frameBytes(document, 0, 12);
    const auto result = admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual,
                                        queue, responses, f->now);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_TRUE(result.responseWanted);
    TEST_ASSERT_FALSE(result.responseCopied);
    bytes.assign(bytes.size(), '\0');
    CommandStream stream(f->channel.actual);
    streamOutcome(CommandStreamOutcome::Receiving,
                  dispatchCommandFrame(queue, stream, f->now).stream);
    TEST_ASSERT_EQUAL_UINT32(12, stream.snapshot().received);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_write_without_response_still_copies_only_one_bounded_frame() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    auto event = gattEvent(binding, bytes);
    event.needResponse = false;
    const auto result =
        admitGattsWrite(event, binding, f->channel.actual, queue, responses, f->now);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_FALSE(result.responseWanted || result.responseCopied);
    TEST_ASSERT_EQUAL_UINT32(0, responses.calls);
    CopiedCommandFrame copied;
    TEST_ASSERT_TRUE(queue.take(copied));
    TEST_ASSERT_FALSE(queue.take(copied));
}
void test_gatts_revocation_before_copy_and_between_observations_rejects() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    host_rtos::beforeEnter = [] {
        host_rtos::beforeEnter = [] { f->channel.actual.suspendAuthority(); };
    };
    // First guard observation sees current; actual queue observation sees closure.
    gattOutcome(GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization,
                admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual, queue,
                                responses, f->now));
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(queue.take(copied));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_copied_before_revoke_retains_original_epoch_and_cannot_execute() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    responses.revokeBeforeCopy = true; // After framecopy, before response copy.
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok,
                admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual, queue,
                                responses, f->now));
    TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, responses.last.original.authority.epoch);
    CommandStream stream(f->channel.actual);
    const auto dispatched = dispatchCommandFrame(queue, stream, f->now);
    TEST_ASSERT_TRUE(dispatched.received);
    TEST_ASSERT_EQUAL_UINT32(binding.authority.epoch, dispatched.epoch);
    streamOutcome(CommandStreamOutcome::Unauthorized, dispatched.stream);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_recycled_handle_old_envelope_never_gets_new_response_binding() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto old = approvedGattBinding(access, connection);
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto oldEvent = gattEvent(old, bytes);
    accessOutcome(IdentityAccessOutcome::Disconnected, access.disconnect(connection));
    BoundConnection next;
    accessOutcome(IdentityAccessOutcome::Connected, access.connect(connection.handle, next));
    accessOutcome(IdentityAccessOutcome::Verified, access.authenticate(next, testVerifiedPeer()));
    auto current = old;
    current.connection = next;
    current.authority = assignedRoute(access, next);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = current;
    const auto result = admitGattsWrite(oldEvent, old, f->channel.actual, queue, responses, f->now);
    gattOutcome(GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization, result);
    TEST_ASSERT_FALSE(result.responseCopied);
    TEST_ASSERT_EQUAL_UINT32(old.authority.epoch, responses.attempted.original.authority.epoch);
    TEST_ASSERT_EQUAL_UINT32(old.connection.token, responses.attempted.original.connection.token);
    TEST_ASSERT_EQUAL_UINT32(oldEvent.transaction, responses.attempted.transaction);
    TEST_ASSERT_EQUAL_UINT32(0, responses.copies);
    TEST_ASSERT_TRUE(current.connection.token > old.connection.token);
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(queue.take(copied));
}
void test_gatts_queue_wait_uses_original_callback_time_not_dispatch_time() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto base = UINT32_MAX - 1000U;
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok,
                admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual, queue,
                                responses, base));
    CommandStream stream(f->channel.actual);
    streamOutcome(CommandStreamOutcome::Expired,
                  dispatchCommandFrame(queue, stream, base + 2000U).stream);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_uninitialized_queue_cannot_ack_positive_copy() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    const auto binding = approvedGattBinding(access, connection);
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TestWriteResponse responses;
    responses.live = binding;
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto result = admitGattsWrite(gattEvent(binding, bytes), binding, f->channel.actual,
                                        queue, responses, f->now);
    gattOutcome(GattsWriteOutcome::Unavailable, AttWriteStatus::UnlikelyError, result);
    TEST_ASSERT_TRUE(result.responseCopied);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AttWriteStatus::UnlikelyError),
                      static_cast<unsigned>(responses.last.status));
}
