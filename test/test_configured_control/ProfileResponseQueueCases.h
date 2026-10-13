#pragma once
void responseCopyOutcome(ProfileResponseCopy expected, ProfileResponseCopy actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
RoutedProfileResponse sampleProfileResponse(std::uint32_t request = 1,
                                            std::uint32_t produced = 17) {
    RoutedProfileResponse value;
    value.original = authority(1);
    value.producedAt = produced;
    value.response.command = controlCommand(ProfileOperation::Abort, request);
    value.response.code = ProfileResponseCode::Aborted;
    value.response.detail = ProfileResponseDetail::None;
    value.response.draftRevision = 2;
    value.response.configurationRevision = 1;
    return value;
}
void test_profile_response_queue_actual_owner_copied_output_sender_saved_status() {
    ControlFixture x;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    x.receive(importExample());
    x.command(ProfileOperation::Finish);
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    RoutedProfileResponse response;
    TEST_ASSERT_TRUE(output.takeCurrent(response));
    responseCode(ProfileResponseCode::DraftReady, response);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, x.clock.now)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(x.clock.now)));
    x.command(ProfileOperation::Apply, 2);
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    TEST_ASSERT_TRUE(output.takeCurrent(response));
    responseCode(ProfileResponseCode::ApplyQueued, response);
    pumpProfileApply();
    x.clock.now = 23;
    x.control.service();
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    TEST_ASSERT_TRUE(output.takeCurrent(response));
    responseCode(ProfileResponseCode::Saved, response);
    TEST_ASSERT_EQUAL_UINT32(23, response.producedAt);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, 24)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(25)));
    ProfileResponseFrame wire;
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), bearer.last.count, wire));
    TEST_ASSERT_EQUAL_UINT32(2, wire.configurationRevision);
    TEST_ASSERT_EQUAL_UINT32(2, wire.command.request);
    TEST_ASSERT_EQUAL_UINT32(4, wire.draftRevision);
    responseFixture("response-queue-saved-v1.txt", response);
    char observation[512];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_profile_response_queue\",\"depth\":%lu,\"itemBytes\":%lu,"
        "\"producedAt\":%lu,\"senderServiceAt\":25,\"code\":%u,\"request\":%lu,"
        "\"configurationRevision\":%lu,\"storedGeneration\":%lu,\"radioMeasured\":false}",
        static_cast<unsigned long>(ProfileResponseQueueLimits::Depth),
        static_cast<unsigned long>(sizeof(RoutedProfileResponse)),
        static_cast<unsigned long>(response.producedAt), static_cast<unsigned>(wire.code),
        static_cast<unsigned long>(wire.command.request),
        static_cast<unsigned long>(wire.configurationRevision),
        static_cast<unsigned long>(f->application.snapshot().storedGeneration));
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observation));
    profileSourceFixture("response-queue-observation.json",
                         std::string(observation, count) + char(10));

    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_profile_response_queue_full_owner_retains_exact_output_and_input() {
    ControlFixture x;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 4; ++i)
        responseCopyOutcome(ProfileResponseCopy::Copied,
                            output.copyResponse(sampleProfileResponse(i + 1, 0)));
    TEST_ASSERT_TRUE(output.begin());
    x.receive(importExample());
    x.command(ProfileOperation::Finish);
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    TEST_ASSERT_FALSE(x.control.offerResponse(output));
    TEST_ASSERT_FALSE(x.control.ready());
    TEST_ASSERT_FALSE(x.pump.service().received);
    RoutedProfileResponse response;
    TEST_ASSERT_TRUE(output.takeCurrent(response));
    x.clock.now = 9;
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    for (unsigned i = 1; i < 4; ++i) {
        TEST_ASSERT_TRUE(output.takeCurrent(response));
        TEST_ASSERT_EQUAL_UINT32(i + 1, response.response.command.request);
    }
    TEST_ASSERT_TRUE(output.takeCurrent(response));
    responseCode(ProfileResponseCode::DraftReady, response);
    TEST_ASSERT_EQUAL_UINT32(0, response.producedAt);
    TEST_ASSERT_TRUE(x.pump.service().control);
    x.take(ProfileResponseCode::ApplyQueued);
}
void test_profile_response_queue_invalid_uninitialized_future_and_wrong_route() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    auto value = sampleProfileResponse();
    RoutedProfileResponse out;
    out.producedAt = 99;
    responseCopyOutcome(ProfileResponseCopy::Uninitialized, output.copyResponse(value));
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(99, out.producedAt);
    TEST_ASSERT_TRUE(output.begin());
    auto bad = value;
    bad.response.command.request = 0;
    responseCopyOutcome(ProfileResponseCopy::Invalid, output.copyResponse(bad));
    bad = value;
    bad.response.code = ProfileResponseCode::Saved;
    responseCopyOutcome(ProfileResponseCopy::Invalid, output.copyResponse(bad));
    bad = value;
    bad.producedAt = 18;
    responseCopyOutcome(ProfileResponseCopy::Expired, output.copyResponse(bad));
    bad = value;
    bad.original.epoch = 2;
    responseCopyOutcome(ProfileResponseCopy::Unauthorized, output.copyResponse(bad));
    bad = value;
    bad.original.gameMaster = false;
    responseCopyOutcome(ProfileResponseCopy::Unauthorized, output.copyResponse(bad));
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(99, out.producedAt);
}
void test_profile_response_queue_freezes_all_producer_scalars_before_observation() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    auto input = sampleProfileResponse();
    static RoutedProfileResponse *caller;
    caller = &input;
    host_rtos::beforeEnter = [] {
        *caller = sampleProfileResponse(42, 18);
        caller->original = authority(3);
    };
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(input));
    caller = nullptr;
    RoutedProfileResponse out;
    TEST_ASSERT_TRUE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(1, out.response.command.request);
    TEST_ASSERT_EQUAL_UINT32(17, out.producedAt);
    TEST_ASSERT_EQUAL_UINT32(1, out.original.epoch);
}
void test_profile_response_queue_consumer_discards_four_stale_without_relabel() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 4; ++i)
        responseCopyOutcome(ProfileResponseCopy::Copied,
                            output.copyResponse(sampleProfileResponse(i + 1)));
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    RoutedProfileResponse out;
    out.producedAt = 99;
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(99, out.producedAt);
    auto current = sampleProfileResponse();
    current.original = authority(3);
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(current));
    TEST_ASSERT_TRUE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(3, out.original.epoch);
}
void test_profile_response_queue_expiry_queue_wait_wrap_preserves_original_time() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    auto value = sampleProfileResponse();
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(value));
    clock.now = 10017;
    RoutedProfileResponse out;
    out.producedAt = 99;
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(99, out.producedAt);
    responseCopyOutcome(ProfileResponseCopy::Expired, output.copyResponse(value));
    clock.now = 2;
    value.producedAt = UINT32_MAX - 2;
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(value));
    TEST_ASSERT_TRUE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 2, out.producedAt);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(out, 2)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Expired),
                      static_cast<unsigned>(sender.service(9997)));
    TEST_ASSERT_TRUE(bearer.frames.empty());
}
void test_profile_response_queue_rights_loss_after_copy_selection_is_not_delivery() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    const auto value = sampleProfileResponse();
    f->channel.revokeAfter = f->channel.reads + 1;
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(value));
    RoutedProfileResponse out;
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_response_queue_rights_loss_after_take_selection_sender_rechecks() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(sampleProfileResponse()));
    f->channel.revokeAfter = f->channel.reads + 1;
    RoutedProfileResponse out;
    TEST_ASSERT_TRUE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(1, out.original.epoch);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Unauthorized),
                      static_cast<unsigned>(sender.load(out, 17)));
    TEST_ASSERT_TRUE(bearer.frames.empty());
}
ProfileTransferControl *offeringControl = nullptr;
IProfileResponseQueue *offeringQueue = nullptr;
void test_profile_response_owner_offer_reentrant_and_expired_retains_bounds() {
    ControlFixture x;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    x.receive("abc");
    x.command(ProfileOperation::Abort);
    offeringControl = &x.control;
    offeringQueue = &output;
    host_rtos::beforeEnter = [] {
        TEST_ASSERT_FALSE(offeringControl->offerResponse(*offeringQueue));
    };
    TEST_ASSERT_TRUE(x.control.offerResponse(output));
    offeringControl = nullptr;
    offeringQueue = nullptr;
    RoutedProfileResponse out;
    TEST_ASSERT_TRUE(output.takeCurrent(out));
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    x.receive("abc");
    x.command(ProfileOperation::Abort, 2);
    x.clock.now = 10000;
    TEST_ASSERT_FALSE(x.control.offerResponse(output));
    TEST_ASSERT_TRUE(x.control.ready());
    TEST_ASSERT_FALSE(output.takeCurrent(out));
}
void test_profile_response_owner_full_port_revoke_closes_without_game_or_store_effect() {
    ControlFixture x;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    for (unsigned i = 0; i < 4; ++i)
        responseCopyOutcome(ProfileResponseCopy::Copied,
                            output.copyResponse(sampleProfileResponse(i + 1, 0)));
    x.receive("abc");
    x.command(ProfileOperation::Abort);
    TEST_ASSERT_FALSE(x.control.offerResponse(output));
    const auto writes = f->store.writeCalls;
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_FALSE(x.control.offerResponse(output));
    TEST_ASSERT_TRUE(x.control.ready());
    RoutedProfileResponse out;
    TEST_ASSERT_FALSE(output.takeCurrent(out));
    TEST_ASSERT_EQUAL_UINT32(writes, f->store.writeCalls);
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
}

void test_profile_response_selection_busy_does_not_take_and_load_preserves_time() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(sampleProfileResponse(1)));
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(sampleProfileResponse(2)));
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    ProfileResponseReader reader(output, sender);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(18)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Busy),
                      static_cast<unsigned>(reader.select(19)));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(20)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(21)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Expired),
                      static_cast<unsigned>(sender.service(10017)));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.calls);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Idle),
                      static_cast<unsigned>(reader.select(10017)));
}

ProfileResponseReader *nestedReader = nullptr;
void test_profile_response_selection_reentrant_clock_authority_does_not_take_twice() {
    ImportClock clock;
    clock.now = 17;
    FreeRtosProfileResponseQueue output(f->channel, clock);
    TEST_ASSERT_TRUE(output.begin());
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(sampleProfileResponse(1)));
    responseCopyOutcome(ProfileResponseCopy::Copied, output.copyResponse(sampleProfileResponse(2)));
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    ProfileResponseReader reader(output, sender);
    nestedReader = &reader;
    host_rtos::beforeEnter = [] {
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Busy),
                          static_cast<unsigned>(nestedReader->select(17)));
    };
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(17)));
    nestedReader = nullptr;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(17)));
    ProfileResponseFrame value;
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, value));
    TEST_ASSERT_EQUAL_UINT32(1, value.command.request);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(reader.select(17)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(17)));
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, value));
    TEST_ASSERT_EQUAL_UINT32(2, value.command.request);
    TEST_ASSERT_EQUAL_UINT32(2, bearer.calls);
}
