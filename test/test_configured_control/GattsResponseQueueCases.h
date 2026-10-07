#pragma once
#include <functional>
// Explicit single-thread lifecycle fake. It never infers a token from SDK/MAC.
class GattsResponseContext : public IGattsResponseContext {
  public:
    GattsWriteBinding live = profileGattBinding();
    mutable std::uint32_t now = 17;
    bool available = true;
    mutable unsigned observations = 0;
    mutable std::function<void()> onClock, afterLive;
    std::uint32_t nowMilliseconds() const noexcept override {
        const auto result = now;
        if (onClock) {
            auto hook = onClock;
            onClock = nullptr;
            hook();
        }
        return result;
    }
    bool originalLive(const GattsWriteBinding &value) const noexcept override {
        ++observations;
        const bool result =
            available && value.connection == live.connection && value.interface == live.interface &&
            value.attribute == live.attribute && value.peerAddress == live.peerAddress;
        if (afterLive) {
            auto hook = afterLive;
            afterLive = nullptr;
            hook();
        }
        return result;
    }
};
class GattsResponseSubmitter : public IGattsResponseSubmitter {
  public:
    explicit GattsResponseSubmitter(GattsResponseContext &context) : context(context) {}
    GattsResponseContext &context;
    std::vector<RoutedWriteResponse> posts;
    unsigned calls = 0;
    bool fail = false;
    std::function<void()> beforeSubmit;
    bool submitOriginal(const RoutedWriteResponse &caller) noexcept override {
        const auto value = caller;
        ++calls;
        if (beforeSubmit) {
            auto hook = beforeSubmit;
            beforeSubmit = nullptr;
            hook();
        }
        if (fail ||
            context.nowMilliseconds() - value.receivedAt >=
                GattsResponseLimits::MaxAgeMilliseconds ||
            !context.originalLive(value.original))
            return false;
        posts.push_back(value); // Fake posting only, never a physical ATT response.
        return true;
    }
};
RoutedWriteResponse gattsResponse(const GattsResponseContext &context,
                                  std::uint32_t transaction = 1) {
    RoutedWriteResponse value;
    value.original = context.live;
    value.transaction = transaction;
    value.receivedAt = context.now;
    value.attribute = value.original.attribute;
    value.status = AttWriteStatus::Ok;
    return value;
}
void responseDispatch(GattsResponseDispatch expected, GattsResponseDispatcher &worker) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(worker.service()));
}
void test_gatts_response_queue_actual_command_application_and_tool_observation() {
    IdentityAccess access(f->channel.actual);
    BoundConnection connection;
    GattsResponseContext context;
    context.live = approvedGattBinding(access, connection);
    FreeRtosGattsResponseQueue output(context);
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin() && output.begin());
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    unsigned frames = 0;
    for (std::size_t offset = 0; offset < document.size(); offset += 12) {
        auto bytes =
            frameBytes(document, offset, std::min<std::size_t>(12, document.size() - offset));
        auto event = gattEvent(context.live, bytes);
        event.transaction = frames + 1;
        context.now = 17 + frames * 2;
        const auto result =
            admitGattsWrite(event, context.live, f->channel.actual, input, output, context.now);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        TEST_ASSERT_TRUE(result.responseCopied);
        TEST_ASSERT_EQUAL_UINT32(frames, port.calls); // No post within admission.
        bytes.assign(bytes.size(), '\0');
        ++context.now;
        responseDispatch(GattsResponseDispatch::Submitted, worker);
        TEST_ASSERT_EQUAL_UINT32(event.transaction, port.posts.back().transaction);
        TEST_ASSERT_EQUAL_UINT32(context.now - 1, port.posts.back().receivedAt);
        TEST_ASSERT_TRUE(port.posts.back().original.connection == connection);
        TEST_ASSERT_TRUE(dispatchCommandFrame(input, stream, context.now).received);
        TEST_ASSERT_FALSE(f->application.gameRunning());
        ++frames;
    }
    TEST_ASSERT_EQUAL_UINT32(13, frames);
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    char observation[768];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_gatts_response_queue\",\"depth\":%lu,\"itemBytes\":%lu,"
        "\"maxAgeMilliseconds\":%lu,\"frameCopies\":%u,\"responseCopies\":%lu,"
        "\"workerPosts\":%u,\"callbackPosts\":0,\"firstReceivedAt\":%lu,"
        "\"lastReceivedAt\":%lu,\"lastWorkerAt\":%lu,\"runningBeforeOwner\":false,"
        "\"runningAfterOwner\":%s,\"peerDeliveryMeasured\":false,\"rfInitialized\":false}\n",
        static_cast<unsigned long>(GattsResponseLimits::Depth),
        static_cast<unsigned long>(sizeof(RoutedWriteResponse)),
        static_cast<unsigned long>(GattsResponseLimits::MaxAgeMilliseconds), frames,
        static_cast<unsigned long>(port.posts.size()), port.calls,
        static_cast<unsigned long>(port.posts.front().receivedAt),
        static_cast<unsigned long>(port.posts.back().receivedAt),
        static_cast<unsigned long>(context.now), f->application.gameRunning() ? "true" : "false");
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observation));
    sdkFixture("response-queue-observation.json", std::string(observation, count));
    TEST_ASSERT_EQUAL_UINT32(44, sizeof(RoutedWriteResponse));
}
void test_gatts_response_queue_profile_control_private_import_and_saved_reply() {
    ControlFixture x;
    GattsResponseContext context;
    context.now = 0;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    unsigned writes = 0;
    const auto admit = [&](const std::string &value) {
        auto event = gattEvent(context.live, value);
        event.transaction = ++writes;
        const auto result =
            admitGattsProfileWrite(event, context.live, f->channel, x.queue, output, 0);
        TEST_ASSERT_TRUE(result.responseCopied);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        TEST_ASSERT_EQUAL_UINT32(writes - 1, port.calls);
        responseDispatch(GattsResponseDispatch::Submitted, worker);
        TEST_ASSERT_EQUAL_UINT32(writes, port.posts.back().transaction);
        TEST_ASSERT_EQUAL_UINT32(0, port.posts.back().receivedAt);
    };
    const auto document = importExample();
    admit(queuedProfileEnvelope(document));
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(document, 128)) {
        admit(value);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    admit(controlBytes(controlCommand(ProfileOperation::Finish)));
    TEST_ASSERT_TRUE(x.pump.service().control);
    x.take(ProfileResponseCode::DraftReady);
    admit(controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    TEST_ASSERT_TRUE(x.pump.service().control);
    x.take(ProfileResponseCode::ApplyQueued);
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
    TEST_ASSERT_EQUAL_UINT32(172, writes);
    TEST_ASSERT_EQUAL_UINT32(writes, port.posts.size());
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.snapshot().storedGeneration);
}
void test_gatts_response_queue_uninitialized_invalid_status_binding_and_future() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    auto value = gattsResponse(context);
    RoutedWriteResponse destination;
    destination.transaction = 99;
    TEST_ASSERT_FALSE(output.copyWriteResponse(value));
    TEST_ASSERT_FALSE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(99, destination.transaction);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 7; ++i) {
        auto bad = value;
        if (i == 0)
            bad.status = static_cast<AttWriteStatus>(0xff);
        if (i == 1)
            bad.original.connection.token = 0;
        if (i == 2)
            bad.original.peerAddress.fill(0);
        if (i == 3)
            bad.original.interface = 0xff;
        if (i == 4)
            bad.original.attribute = 0;
        if (i == 5)
            bad.attribute = 0;
        if (i == 6)
            ++bad.receivedAt;
        TEST_ASSERT_FALSE(output.copyWriteResponse(bad));
    }
    TEST_ASSERT_FALSE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(99, destination.transaction);
}
void test_gatts_response_queue_full_fifo_begin_idempotent_and_no_retry() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 4; ++i)
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, i + 1)));
    TEST_ASSERT_FALSE(output.copyWriteResponse(gattsResponse(context, 5)));
    TEST_ASSERT_TRUE(output.begin());
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    for (unsigned i = 0; i < 4; ++i) {
        responseDispatch(GattsResponseDispatch::Submitted, worker);
        TEST_ASSERT_EQUAL_UINT32(i + 1, port.calls);
        TEST_ASSERT_EQUAL_UINT32(i + 1, port.posts.back().transaction);
    }
    responseDispatch(GattsResponseDispatch::Idle, worker);
    TEST_ASSERT_EQUAL_UINT32(4, port.calls);
}
void test_gatts_response_queue_freezes_all_fields_before_clock_observation() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    auto value = gattsResponse(context);
    context.onClock = [&] {
        value = {};
        value.transaction = 99;
    };
    TEST_ASSERT_TRUE(output.copyWriteResponse(value));
    RoutedWriteResponse destination;
    TEST_ASSERT_TRUE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(1, destination.transaction);
    TEST_ASSERT_EQUAL_UINT32(17, destination.receivedAt);
    TEST_ASSERT_TRUE(destination.original.connection == context.live.connection);
    TEST_ASSERT_TRUE(destination.original.authority == context.live.authority);
    TEST_ASSERT_EQUAL_UINT32(0x42, destination.attribute);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<unsigned>(destination.status));
}
void test_gatts_response_queue_original_handle_interface_peer_attribute_never_relabels() {
    for (unsigned i = 0; i < 5; ++i) {
        GattsResponseContext context;
        FreeRtosGattsResponseQueue output(context);
        TEST_ASSERT_TRUE(output.begin());
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context)));
        if (i == 0)
            ++context.live.connection.token;
        if (i == 1)
            ++context.live.connection.handle;
        if (i == 2)
            ++context.live.interface;
        if (i == 3)
            ++context.live.peerAddress[5];
        if (i == 4)
            ++context.live.attribute;
        RoutedWriteResponse destination;
        destination.transaction = 99;
        TEST_ASSERT_FALSE(output.takeCurrent(destination));
        TEST_ASSERT_EQUAL_UINT32(99, destination.transaction);
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, 2)));
        TEST_ASSERT_TRUE(output.takeCurrent(destination));
        TEST_ASSERT_EQUAL_UINT32(2, destination.transaction);
        TEST_ASSERT_TRUE(destination.original.connection == context.live.connection);
    }
}
void test_gatts_response_queue_drops_only_four_stale_and_keeps_output_unchanged() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 4; ++i)
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, i + 1)));
    context.available = false;
    const auto reads = context.observations;
    RoutedWriteResponse destination;
    destination.transaction = 99;
    TEST_ASSERT_FALSE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(4, context.observations - reads);
    TEST_ASSERT_EQUAL_UINT32(99, destination.transaction);
    context.available = true;
    TEST_ASSERT_FALSE(output.takeCurrent(destination));
}
void test_gatts_response_queue_absolute_age_boundary_and_clock_wrap() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    const auto value = gattsResponse(context);
    TEST_ASSERT_TRUE(output.copyWriteResponse(value));
    context.now = 1016;
    RoutedWriteResponse destination;
    TEST_ASSERT_TRUE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(17, destination.receivedAt);
    TEST_ASSERT_TRUE(output.copyWriteResponse(value));
    context.now = 1017;
    destination.transaction = 99;
    TEST_ASSERT_FALSE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(99, destination.transaction);
    TEST_ASSERT_FALSE(output.copyWriteResponse(value));
    context.now = UINT32_MAX - 2;
    const auto wrapped = gattsResponse(context);
    TEST_ASSERT_TRUE(output.copyWriteResponse(wrapped));
    context.now = 2;
    TEST_ASSERT_TRUE(output.takeCurrent(destination));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 2, destination.receivedAt);
}
void test_gatts_response_queue_close_after_admission_is_rechecked_by_consumer() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    context.afterLive = [&] { context.available = false; };
    TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context)));
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    responseDispatch(GattsResponseDispatch::Idle, worker);
    TEST_ASSERT_EQUAL_UINT32(0, port.calls);
}
void test_gatts_response_dispatch_close_or_expire_after_selection_never_posts() {
    for (unsigned i = 0; i < 2; ++i) {
        GattsResponseContext context;
        FreeRtosGattsResponseQueue output(context);
        TEST_ASSERT_TRUE(output.begin());
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context)));
        context.afterLive = [&] {
            if (i)
                context.now += 1000;
            else
                context.available = false;
        };
        GattsResponseSubmitter port(context);
        GattsResponseDispatcher worker(output, port);
        responseDispatch(GattsResponseDispatch::Rejected, worker);
        TEST_ASSERT_EQUAL_UINT32(0, port.calls);
        responseDispatch(GattsResponseDispatch::Idle, worker);
    }
}
void test_gatts_response_dispatch_submitter_rechecks_original_boundary_without_retry() {
    for (unsigned i = 0; i < 3; ++i) {
        GattsResponseContext context;
        FreeRtosGattsResponseQueue output(context);
        TEST_ASSERT_TRUE(output.begin());
        TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context)));
        GattsResponseSubmitter port(context);
        port.beforeSubmit = [&] {
            if (i == 0)
                ++context.live.connection.token;
            if (i == 1)
                context.now += 1000;
            if (i == 2)
                port.fail = true;
        };
        GattsResponseDispatcher worker(output, port);
        responseDispatch(GattsResponseDispatch::Rejected, worker);
        TEST_ASSERT_EQUAL_UINT32(1, port.calls);
        TEST_ASSERT_TRUE(port.posts.empty());
        responseDispatch(GattsResponseDispatch::Idle, worker);
        TEST_ASSERT_EQUAL_UINT32(1, port.calls);
    }
}
void test_gatts_response_dispatch_reentry_cannot_take_next_response() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, 1)));
    TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, 2)));
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    port.beforeSubmit = [&] { responseDispatch(GattsResponseDispatch::Busy, worker); };
    responseDispatch(GattsResponseDispatch::Submitted, worker);
    TEST_ASSERT_EQUAL_UINT32(1, port.posts.back().transaction);
    responseDispatch(GattsResponseDispatch::Submitted, worker);
    TEST_ASSERT_EQUAL_UINT32(2, port.posts.back().transaction);
    TEST_ASSERT_EQUAL_UINT32(2, port.calls);
}
void test_gatts_response_queue_closed_gm_can_receive_authorization_error_on_original_link() {
    GattsResponseContext context;
    context.live.authority = {};
    FreeRtosGattsResponseQueue output(context);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin() && output.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    auto event = gattEvent(context.live, bytes);
    event.attribute += 1;
    const auto result = admitGattsWrite(event, context.live, f->channel.actual, input, output, 17);
    gattOutcome(GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization, result);
    TEST_ASSERT_TRUE(result.responseCopied);
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    responseDispatch(GattsResponseDispatch::Submitted, worker);
    TEST_ASSERT_EQUAL_UINT32(8, static_cast<unsigned>(port.posts.back().status));
    TEST_ASSERT_EQUAL_UINT32(event.attribute, port.posts.back().attribute);
    CopiedCommandFrame copy;
    TEST_ASSERT_FALSE(input.take(copy));
}
void test_gatts_response_queue_full_or_failed_post_preserves_original_input() {
    for (unsigned i = 0; i < 2; ++i) {
        GattsResponseContext context;
        FreeRtosGattsResponseQueue output(context);
        FreeRtosCommandFrameQueue input(f->channel.actual);
        TEST_ASSERT_TRUE(input.begin() && output.begin());
        if (!i)
            for (unsigned j = 0; j < 4; ++j)
                TEST_ASSERT_TRUE(output.copyWriteResponse(gattsResponse(context, j + 1)));
        const auto bytes = frameBytes(json(f->command()), 0, 12);
        const auto result = admitGattsWrite(gattEvent(context.live, bytes), context.live,
                                            f->channel.actual, input, output, 17);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        TEST_ASSERT_EQUAL_INT(i != 0, result.responseCopied);
        if (i) {
            GattsResponseSubmitter port(context);
            port.fail = true;
            GattsResponseDispatcher worker(output, port);
            responseDispatch(GattsResponseDispatch::Rejected, worker);
            TEST_ASSERT_EQUAL_UINT32(1, port.calls);
        }
        CopiedCommandFrame copy;
        TEST_ASSERT_TRUE(input.take(copy));
        TEST_ASSERT_EQUAL_UINT32(17, copy.receivedAt);
        TEST_ASSERT_EQUAL_UINT32(context.live.authority.epoch, copy.epoch);
        TEST_ASSERT_FALSE(input.take(copy));
        TEST_ASSERT_FALSE(f->application.gameRunning());
    }
}
void test_gatts_response_queue_gm_revoke_keeps_copy_ack_but_cannot_execute_input() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin() && output.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    const auto result = admitGattsWrite(gattEvent(context.live, bytes), context.live,
                                        f->channel.actual, input, output, 17);
    TEST_ASSERT_TRUE(result.responseCopied);
    f->channel.actual.suspendAuthority();
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    responseDispatch(GattsResponseDispatch::Submitted, worker);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<unsigned>(port.posts.back().status));
    TEST_ASSERT_TRUE(port.posts.back().original.authority == context.live.authority);
    CommandStream stream(f->channel.actual);
    const auto dispatched = dispatchCommandFrame(input, stream, 18);
    TEST_ASSERT_TRUE(dispatched.received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Unauthorized),
                      static_cast<unsigned>(dispatched.stream.outcome));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_gatts_response_queue_all_sdk_statuses_and_zero_transaction_are_bounded() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    TEST_ASSERT_TRUE(output.begin());
    const AttWriteStatus statuses[] = {AttWriteStatus::Ok,
                                       AttWriteStatus::InvalidHandle,
                                       AttWriteStatus::InvalidPdu,
                                       AttWriteStatus::RequestUnsupported,
                                       AttWriteStatus::InvalidOffset,
                                       AttWriteStatus::InsufficientAuthorization,
                                       AttWriteStatus::InvalidValueLength,
                                       AttWriteStatus::UnlikelyError,
                                       AttWriteStatus::InsufficientResources};
    for (const auto status : statuses) {
        auto value = gattsResponse(context, 0); // SDK transaction 0 is not a new authority.
        value.status = status;
        if (status == AttWriteStatus::InvalidHandle)
            ++value.attribute;
        TEST_ASSERT_TRUE(output.copyWriteResponse(value));
        RoutedWriteResponse destination;
        TEST_ASSERT_TRUE(output.takeCurrent(destination));
        TEST_ASSERT_EQUAL_UINT32(0, destination.transaction);
        TEST_ASSERT_EQUAL_UINT32(value.attribute, destination.attribute);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(status), static_cast<unsigned>(destination.status));
    }
}
void test_gatts_response_queue_raw_write_without_response_never_enqueues_output() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin() && output.begin());
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    auto event = gattEvent(context.live, bytes);
    event.needResponse = false;
    const auto result = admitGattsWrite(event, context.live, f->channel.actual, input, output, 17);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_FALSE(result.responseWanted || result.responseCopied);
    TEST_ASSERT_EQUAL_UINT32(0, context.observations);
    RoutedWriteResponse response;
    TEST_ASSERT_FALSE(output.takeCurrent(response));
    CopiedCommandFrame copy;
    TEST_ASSERT_TRUE(input.take(copy));
    TEST_ASSERT_FALSE(input.take(copy));
}
void test_gatts_response_queue_actual_input_full_is_resource_error_not_success() {
    GattsResponseContext context;
    FreeRtosGattsResponseQueue output(context);
    FreeRtosCommandFrameQueue input(f->channel.actual);
    TEST_ASSERT_TRUE(input.begin() && output.begin());
    GattsResponseSubmitter port(context);
    GattsResponseDispatcher worker(output, port);
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    for (unsigned i = 0; i < CommandFrameQueueLimits::Depth; ++i) {
        auto event = gattEvent(context.live, bytes);
        event.transaction = i + 1;
        const auto result =
            admitGattsWrite(event, context.live, f->channel.actual, input, output, 17);
        TEST_ASSERT_TRUE(result.responseCopied);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        responseDispatch(GattsResponseDispatch::Submitted, worker);
    }
    auto event = gattEvent(context.live, bytes);
    event.transaction = 5;
    const auto result = admitGattsWrite(event, context.live, f->channel.actual, input, output, 17);
    TEST_ASSERT_TRUE(result.responseCopied);
    gattOutcome(GattsWriteOutcome::QueueFull, AttWriteStatus::InsufficientResources, result);
    responseDispatch(GattsResponseDispatch::Submitted, worker);
    TEST_ASSERT_EQUAL_UINT32(0x11, static_cast<unsigned>(port.posts.back().status));
    TEST_ASSERT_EQUAL_UINT32(5, port.posts.back().transaction);
    CopiedCommandFrame copy;
    unsigned copied = 0;
    while (input.take(copy))
        ++copied;
    TEST_ASSERT_EQUAL_UINT32(CommandFrameQueueLimits::Depth, copied);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
