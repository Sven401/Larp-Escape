#pragma once
GattsWriteBinding profileGattBinding() {
    GattsWriteBinding binding;
    binding.interface = 1;
    binding.attribute = 0x42;
    binding.peerAddress = {{1, 2, 3, 4, 5, 6}};
    binding.connection.handle = 5;
    binding.connection.token = 1;
    binding.authority = authority(1);
    return binding; // Explicit Host fake lifecycle.
}
void test_gatts_profile_actual_raw_queue_control_output_sender_confirmed_journal() {
    ControlFixture x;
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    ProfileResponseReader reader(output, sender);
    const auto bytes = importExample();
    unsigned writes = 0;
    const auto admit = [&](const std::string &value) {
        auto event = gattEvent(binding, value);
        event.transaction = ++writes;
        const auto result = admitGattsProfileWrite(event, binding, f->channel, x.queue, replies, 0);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        TEST_ASSERT_TRUE(result.responseCopied);
        TEST_ASSERT_EQUAL_UINT32(event.transaction, replies.last.transaction);
    };
    admit(queuedProfileEnvelope(bytes));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Empty),
                      static_cast<unsigned>(x.source.phase()));
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(bytes, 128)) {
        admit(value);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    TEST_ASSERT_FALSE(f->application.storage().draftReady);
    TEST_ASSERT_FALSE(f->application.busy());
    admit(controlBytes(controlCommand(ProfileOperation::Finish)));
    TEST_ASSERT_EQUAL_UINT32(2, f->application.storage().draftRevision);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(0)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(0)));
    ProfileResponseFrame wire;
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, wire));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseCode::DraftReady),
                      static_cast<unsigned>(wire.code));
    admit(controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(f->application.busy());
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(0)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(0)));
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, wire));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseCode::ApplyQueued),
                      static_cast<unsigned>(wire.code));
    pumpProfileApply();
    x.control.service();
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(0)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(0)));
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, wire));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseCode::Saved),
                      static_cast<unsigned>(wire.code));
    RoutedProfileResponse saved;
    saved.original = binding.authority;
    saved.response = wire;
    responseFixture("gatts-profile-saved-v1.txt", saved);
    TEST_ASSERT_EQUAL_UINT32(172, writes);
    TEST_ASSERT_EQUAL_UINT32(writes, replies.copies);
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    char observed[512];
    const int count =
        std::snprintf(observed, sizeof(observed),
                      "{\"scope\":\"host_gatts_profile\",\"writeCopies\":%u,\"attResponseCopies\":%"
                      "u,\"semanticResponseCopies\":%u,"
                      "\"code\":%u,\"configurationRevision\":%lu,\"storedGeneration\":%lu,"
                      "\"peerDeliveryMeasured\":false,\"radioInitialized\":false}",
                      writes, replies.copies, bearer.calls, static_cast<unsigned>(wire.code),
                      static_cast<unsigned long>(wire.configurationRevision),
                      static_cast<unsigned long>(f->application.snapshot().storedGeneration));
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observed));
    profileSourceFixture("gatts-profile-observation.json", std::string(observed, count) + char(10));
}
void test_gatts_profile_full_retry_att_resource_error_is_not_success() {
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = controlBytes(controlCommand(ProfileOperation::Abort));
    const auto event = gattEvent(binding, value);
    for (unsigned i = 0; i < 4; ++i)
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok,
                    admitGattsProfileWrite(event, binding, f->channel, queue, replies, i));
    gattOutcome(GattsWriteOutcome::QueueFull, AttWriteStatus::InsufficientResources,
                admitGattsProfileWrite(event, binding, f->channel, queue, replies, 4));
    CopiedProfileFrame out;
    TEST_ASSERT_TRUE(queue.take(out));
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok,
                admitGattsProfileWrite(event, binding, f->channel, queue, replies, 4));
    for (unsigned i = 1; i < 5; ++i) {
        TEST_ASSERT_TRUE(queue.take(out));
        TEST_ASSERT_EQUAL_UINT32(i, out.receivedAt);
    }
    TEST_ASSERT_FALSE(queue.take(out));
}
void test_gatts_profile_context_prepare_offset_type_and_shape_gates() {
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto valid = controlBytes(controlCommand(ProfileOperation::Abort));
    for (unsigned bad = 0; bad < 9; ++bad) {
        auto value = valid;
        auto event = gattEvent(binding, value);
        GattsWriteOutcome expected = GattsWriteOutcome::InvalidFrame;
        AttWriteStatus status = AttWriteStatus::InvalidValueLength;
        if (bad == 0) {
            event.interface = 2;
            expected = GattsWriteOutcome::WrongInterface;
            status = AttWriteStatus::InsufficientAuthorization;
        }
        if (bad == 1) {
            event.connection = 6;
            expected = GattsWriteOutcome::WrongConnection;
            status = AttWriteStatus::InsufficientAuthorization;
        }
        if (bad == 2) {
            event.attribute = 0x43;
            expected = GattsWriteOutcome::WrongAttribute;
            status = AttWriteStatus::InvalidHandle;
        }
        if (bad == 3) {
            event.prepared = true;
            expected = GattsWriteOutcome::PreparedUnsupported;
            status = AttWriteStatus::RequestUnsupported;
        }
        if (bad == 4) {
            event.offset = 1;
            expected = GattsWriteOutcome::InvalidOffset;
            status = AttWriteStatus::InvalidOffset;
        }
        if (bad == 5)
            event.bytes = nullptr;
        if (bad == 6)
            event.count = 129;
        if (bad == 7) {
            value[1] = 1;
            expected = GattsWriteOutcome::UnsupportedFrame;
            status = AttWriteStatus::RequestUnsupported;
        }
        if (bad == 8) {
            value[5] = 1;
            status = AttWriteStatus::InvalidPdu;
        }
        const auto calls = replies.calls;
        const auto result = admitGattsProfileWrite(event, binding, f->channel, queue, replies, 0);
        gattOutcome(expected, status, result);
        TEST_ASSERT_EQUAL_UINT32(calls + (bad > 1 ? 1 : 0), replies.calls);
    }
    CopiedProfileFrame out;
    TEST_ASSERT_FALSE(queue.take(out));
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_gatts_profile_without_att_response_still_only_copies_queue() {
    ControlFixture x;
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    const auto value = queuedProfileEnvelope("abc");
    auto event = gattEvent(binding, value);
    event.needResponse = false;
    const auto result = admitGattsProfileWrite(event, binding, f->channel, x.queue, replies, 0);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_FALSE(result.responseWanted || result.responseCopied);
    TEST_ASSERT_EQUAL_UINT32(0, replies.calls);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Empty),
                      static_cast<unsigned>(x.source.phase()));
    TEST_ASSERT_TRUE(x.pump.service().metadata);
}
void test_gatts_profile_response_failure_does_not_undo_copy_or_relabel_epoch() {
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    replies.available = false;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = controlBytes(controlCommand(ProfileOperation::Abort));
    const auto result =
        admitGattsProfileWrite(gattEvent(binding, value), binding, f->channel, queue, replies, 0);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    TEST_ASSERT_FALSE(result.responseCopied);
    CopiedProfileFrame out;
    TEST_ASSERT_TRUE(queue.take(out));
    TEST_ASSERT_EQUAL_UINT32(1, out.epoch);
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    gattOutcome(
        GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization,
        admitGattsProfileWrite(gattEvent(binding, value), binding, f->channel, queue, replies, 0));
    TEST_ASSERT_FALSE(queue.take(out));
}
void test_gatts_profile_revoke_between_outer_and_queue_admission_prevents_copy() {
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = controlBytes(controlCommand(ProfileOperation::Abort));
    f->channel.revokeAfter = f->channel.reads + 1;
    gattOutcome(
        GattsWriteOutcome::Unauthorized, AttWriteStatus::InsufficientAuthorization,
        admitGattsProfileWrite(gattEvent(binding, value), binding, f->channel, queue, replies, 0));
    CopiedProfileFrame out;
    TEST_ASSERT_FALSE(queue.take(out));
    TEST_ASSERT_FALSE(f->application.busy());
}
GattsWriteBinding *rawCallerBinding = nullptr;
GattsWriteEvent *rawCallerEvent = nullptr;
std::string *rawCallerBytes = nullptr;
void mutateRawCaller() {
    rawCallerBinding->connection.token = 99;
    rawCallerBinding->authority = authority(3);
    rawCallerBinding->attribute = 0x99;
    rawCallerEvent->transaction = 99;
    rawCallerEvent->count = 65535;
    rawCallerEvent->needResponse = false;
    std::fill(rawCallerBytes->begin(), rawCallerBytes->end(), 'z');
}
void test_gatts_profile_freezes_event_binding_and_bytes_before_first_observation() {
    auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    auto value = queuedProfileEnvelope("abc");
    const auto original = value;
    auto event = gattEvent(binding, value);
    rawCallerBinding = &binding;
    rawCallerEvent = &event;
    rawCallerBytes = &value;
    host_rtos::beforeEnter = mutateRawCaller;
    const auto result = admitGattsProfileWrite(event, binding, f->channel, queue, replies, 17);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    rawCallerBinding = nullptr;
    rawCallerEvent = nullptr;
    rawCallerBytes = nullptr;
    CopiedProfileFrame out;
    TEST_ASSERT_TRUE(queue.take(out));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), out.bytes.data(), original.size());
    TEST_ASSERT_EQUAL_UINT32(1, out.epoch);
    TEST_ASSERT_EQUAL_UINT32(17, out.receivedAt);
    TEST_ASSERT_TRUE(result.responseWanted && result.responseCopied);
    TEST_ASSERT_EQUAL_UINT32(17, replies.last.transaction);
    TEST_ASSERT_EQUAL_UINT32(1, replies.last.original.connection.token);
    TEST_ASSERT_EQUAL_UINT32(0x42, replies.last.attribute);
}
void test_gatts_command_shared_gate_freezes_event_binding_and_bytes_too() {
    auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto document = json(f->command());
    auto value = frameBytes(document, 0, 12);
    const auto original = value;
    auto event = gattEvent(binding, value);
    rawCallerBinding = &binding;
    rawCallerEvent = &event;
    rawCallerBytes = &value;
    host_rtos::beforeEnter = mutateRawCaller;
    const auto result = admitGattsWrite(event, binding, f->channel, queue, replies, 17);
    gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
    rawCallerBinding = nullptr;
    rawCallerEvent = nullptr;
    rawCallerBytes = nullptr;
    CopiedCommandFrame out;
    TEST_ASSERT_TRUE(queue.take(out));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), out.bytes.data(), original.size());
    TEST_ASSERT_EQUAL_UINT32(1, out.epoch);
    TEST_ASSERT_EQUAL_UINT32(17, replies.last.transaction);
    TEST_ASSERT_EQUAL_UINT32(1, replies.last.original.connection.token);
}
void test_gatts_profile_uninitialized_and_no_binding_do_not_ack_success() {
    auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    const auto value = controlBytes(controlCommand(ProfileOperation::Abort));
    const auto event = gattEvent(binding, value);
    gattOutcome(GattsWriteOutcome::Unavailable, AttWriteStatus::UnlikelyError,
                admitGattsProfileWrite(event, binding, f->channel, queue, replies, 0));
    const auto calls = replies.calls;
    binding.connection.token = 0;
    gattOutcome(GattsWriteOutcome::NoBinding, AttWriteStatus::InsufficientAuthorization,
                admitGattsProfileWrite(event, binding, f->channel, queue, replies, 0));
    TEST_ASSERT_EQUAL_UINT32(calls, replies.calls);
}
