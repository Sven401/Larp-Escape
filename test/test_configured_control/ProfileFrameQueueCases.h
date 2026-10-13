#pragma once
void profileCopyOutcome(ProfileCopyOutcome expected, ProfileCopyOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
std::string queuedProfileEnvelope(const std::string &bytes) {
    char encoded[20];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(
        encodeProfileSourceEnvelope(importEnvelope(bytes), encoded, sizeof(encoded), written));
    return std::string(encoded, written);
}
void copyProfileValue(FreeRtosProfileFrameQueue &queue, const std::string &value,
                      std::uint32_t time = 0, std::uint32_t epoch = 1) {
    profileCopyOutcome(ProfileCopyOutcome::Copied,
                       queue.copyWrite(value.data(), value.size(), 7, epoch, time));
}
void test_profile_queue_actual_copied_frames_to_temp_private_apply_and_status() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    TemporaryProfileFile file;
    ImportClock clock;
    ProfileStaging source(file, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    const auto bytes = importExample();
    copyProfileValue(queue, queuedProfileEnvelope(bytes), 17);
    clock.now = 20;
    auto selected = pump.service();
    TEST_ASSERT_TRUE(selected.received && selected.metadata);
    receiveOutcome(ProfileReceiveOutcome::Receiving, selected.outcome);
    TEST_ASSERT_EQUAL_UINT32(17, source.receivedAt());
    for (const auto &value : profileFrames(bytes, 128)) {
        copyProfileValue(queue, value, clock.now);
        selected = pump.service();
        TEST_ASSERT_TRUE(selected.received && !selected.metadata);
        receiveOutcome(ProfileReceiveOutcome::Receiving, selected.outcome);
        TEST_ASSERT_EQUAL_UINT32(17, source.receivedAt());
        ++clock.now;
    }
    TEST_ASSERT_FALSE(pump.service().received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Receiving),
                      static_cast<unsigned>(source.phase()));
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, source, source.envelope(), source.original(), clock,
        source.receivedAt());
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(imported.receipt));
    pumpProfileApply();
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    char observed[512];
    const int count =
        std::snprintf(observed, sizeof(observed),
                      "{\"scope\":\"host_profile_queue\",\"dataFrames\":%lu,\"depth\":%lu,"
                      "\"valueBytes\":%lu,\"originalReceivedAt\":%lu,\"queuedIsComplete\":false,"
                      "\"configurationRevision\":%lu,\"storedGeneration\":%lu,"
                      "\"storedMatchesActive\":true,\"running\":false,\"radioMeasured\":false}",
                      static_cast<unsigned long>(profileFrames(bytes, 128).size()),
                      static_cast<unsigned long>(ProfileFrameQueueLimits::Depth),
                      static_cast<unsigned long>(ProfileFrameLimits::ValueBytes),
                      static_cast<unsigned long>(source.receivedAt()),
                      static_cast<unsigned long>(f->application.activation().configurationRevision),
                      static_cast<unsigned long>(f->application.snapshot().storedGeneration));
    TEST_ASSERT_TRUE(count > 0 && std::size_t(count) < sizeof(observed));
    profileSourceFixture("queue-observation.json", std::string(observed, count) + char(10));
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    TEST_ASSERT_EQUAL_UINT32(1, status.connectionEpoch);
    char encoded[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeConfiguredStatus(status.status, encoded,
                                                                   sizeof(encoded), written)));
    profileSourceFixture("queue-status-v2.json", std::string(encoded, written) + char(10));
}
void test_profile_queue_full_exact_retry_copies_and_preserves_false_destination() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    CopiedProfileFrame output;
    output.count = 99;
    TEST_ASSERT_FALSE(queue.take(output));
    TEST_ASSERT_EQUAL_UINT32(99, output.count);
    const auto value = queuedProfileEnvelope("abc");
    profileCopyOutcome(ProfileCopyOutcome::Uninitialized,
                       queue.copyWrite(value.data(), value.size(), 7, 1, 0));
    TEST_ASSERT_TRUE(queue.begin());
    for (unsigned i = 0; i < 4; ++i)
        copyProfileValue(queue, value, i);
    TEST_ASSERT_TRUE(queue.begin()); // Does not reset pending items.
    profileCopyOutcome(ProfileCopyOutcome::QueueFull,
                       queue.copyWrite(value.data(), value.size(), 7, 1, 4));
    TEST_ASSERT_TRUE(queue.take(output));
    TEST_ASSERT_EQUAL_UINT32(0, output.receivedAt);
    copyProfileValue(queue, value, 4);
    for (unsigned i = 1; i <= 4; ++i) {
        TEST_ASSERT_TRUE(queue.take(output));
        TEST_ASSERT_EQUAL_UINT32(i, output.receivedAt);
        TEST_ASSERT_EQUAL_UINT32(20, output.count);
    }
    output.count = 99;
    TEST_ASSERT_FALSE(queue.take(output));
    TEST_ASSERT_EQUAL_UINT32(99, output.count);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_profile_queue_callback_copy_survives_first_observation_and_caller_mutation() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    auto value = queuedProfileEnvelope("abc");
    const auto original = value;
    stageCallerFrame = &value;
    host_rtos::beforeEnter = [] {
        std::fill(stageCallerFrame->begin(), stageCallerFrame->end(), 'z');
    };
    copyProfileValue(queue, value, 42);
    stageCallerFrame = nullptr;
    CopiedProfileFrame output;
    TEST_ASSERT_TRUE(queue.take(output));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), output.bytes.data(), original.size());
    TEST_ASSERT_EQUAL_UINT32(42, output.receivedAt);
}
void test_profile_queue_rejects_prepared_offset_wrong_kind_bounds_and_untrusted_route() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = queuedProfileEnvelope("abc");
    for (unsigned bad = 0; bad < 10; ++bad) {
        auto changed = value;
        if (bad == 0)
            changed.resize(8);
        if (bad == 1)
            changed.resize(129);
        if (bad == 2)
            changed[0] = 2;
        if (bad == 3)
            changed[1] = 1;
        if (bad == 4)
            changed[2] = changed[3] = 0;
        const auto actual =
            queue.copyWrite(bad == 5 ? nullptr : changed.data(), changed.size(), bad == 6 ? 0 : 7,
                            bad == 7 ? 2 : 1, 0, bad == 8, bad == 9 ? 1 : 0);
        profileCopyOutcome(bad == 2 || bad == 3   ? ProfileCopyOutcome::UnsupportedFrame
                           : bad == 6 || bad == 7 ? ProfileCopyOutcome::Unauthorized
                                                  : ProfileCopyOutcome::InvalidWrite,
                           actual);
    }
    CopiedProfileFrame output;
    TEST_ASSERT_FALSE(queue.take(output));
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_profile_queue_pump_takes_one_frame_and_does_not_finish_empty_queue() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    copyProfileValue(queue, queuedProfileEnvelope("abc"));
    copyProfileValue(queue, profileFrames("abc", 20).front());
    TEST_ASSERT_TRUE(pump.service().received);
    TEST_ASSERT_EQUAL_UINT32(0, source.received());
    TEST_ASSERT_EQUAL_UINT32(0, store.appends);
    TEST_ASSERT_TRUE(pump.service().received);
    TEST_ASSERT_EQUAL_UINT32(3, source.received());
    TEST_ASSERT_EQUAL_UINT32(1, store.appends);
    TEST_ASSERT_FALSE(pump.service().received);
    TEST_ASSERT_EQUAL_UINT32(0, store.seals);
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_queue_original_metadata_time_counts_wait_and_deadline() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    copyProfileValue(queue, queuedProfileEnvelope("abc"), 0);
    clock.now = ProfileSourceLimits::DeadlineMilliseconds - 1;
    receiveOutcome(ProfileReceiveOutcome::Receiving, pump.service().outcome);
    TEST_ASSERT_EQUAL_UINT32(0, source.receivedAt());
    copyProfileValue(queue, profileFrames("abc", 20).front(), clock.now);
    ++clock.now;
    const auto expired = pump.service();
    TEST_ASSERT_TRUE(expired.received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                      static_cast<unsigned>(source.phase()));
    receiveOutcome(ProfileReceiveOutcome::Expired, source.outcome());
    TEST_ASSERT_EQUAL_UINT32(0, store.appends);
}
void test_profile_queue_expired_future_and_wrapped_metadata_keep_fixed_age() {
    for (unsigned bad = 0; bad < 3; ++bad) {
        FreeRtosProfileFrameQueue queue(f->channel.actual);
        TEST_ASSERT_TRUE(queue.begin());
        StageStore store;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        ProfileFramePump pump(queue, source, clock);
        const auto received = bad == 0 ? 0u : bad == 1 ? 1u : UINT32_MAX - 20;
        clock.now = bad == 0 ? ProfileSourceLimits::DeadlineMilliseconds : 0;
        copyProfileValue(queue, queuedProfileEnvelope("abc"), received);
        const auto actual = pump.service();
        TEST_ASSERT_TRUE(actual.received);
        receiveOutcome(bad < 2 ? ProfileReceiveOutcome::Expired : ProfileReceiveOutcome::Receiving,
                       actual.outcome);
        TEST_ASSERT_EQUAL_UINT32(bad < 2 ? 0 : 1, store.begins);
    }
}
void test_profile_queue_revoke_before_owner_start_never_opens_or_relabels_metadata() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    copyProfileValue(queue, queuedProfileEnvelope("abc"));
    f->channel.actual.suspendAuthority();
    f->channel.actual.updateAuthority(authority(2));
    const auto actual = pump.service();
    TEST_ASSERT_TRUE(actual.received);
    receiveOutcome(ProfileReceiveOutcome::Unauthorized, actual.outcome);
    TEST_ASSERT_EQUAL_UINT32(1, source.original().epoch);
    TEST_ASSERT_EQUAL_UINT32(0, store.begins);
}
void test_profile_queue_revoke_before_queued_data_closes_source_without_app_effect() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    copyProfileValue(queue, queuedProfileEnvelope("abc"));
    pump.service();
    copyProfileValue(queue, profileFrames("abc", 20).front());
    f->channel.actual.suspendAuthority();
    const auto actual = pump.service();
    TEST_ASSERT_TRUE(actual.received);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                      static_cast<unsigned>(source.phase()));
    TEST_ASSERT_TRUE(store.bytes.empty());
    TEST_ASSERT_EQUAL_UINT32(0, store.appends);
    TEST_ASSERT_FALSE(f->application.busy());
}
ProfileFramePump *nestedProfilePump = nullptr;
void test_profile_queue_reentrant_backend_cannot_take_next_pending_frame() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    const std::string bytes(24, 'x');
    copyProfileValue(queue, queuedProfileEnvelope(bytes));
    pump.service();
    const auto values = profileFrames(bytes, 20);
    for (const auto &value : values)
        copyProfileValue(queue, value);
    nestedProfilePump = &pump;
    store.on = 2;
    store.hook = [](StageStore &) {
        const auto nested = nestedProfilePump->service();
        TEST_ASSERT_FALSE(nested.received);
        receiveOutcome(ProfileReceiveOutcome::Busy, nested.outcome);
    };
    pump.service();
    TEST_ASSERT_EQUAL_UINT32(12, source.received());
    pump.service();
    TEST_ASSERT_EQUAL_UINT32(24, source.received());
    nestedProfilePump = nullptr;
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
}
class BadProfileQueue : public IProfileFrameQueue {
  public:
    unsigned calls = 0;
    CopiedProfileFrame value;
    bool take(CopiedProfileFrame &destination) noexcept override {
        ++calls;
        destination = value;
        return calls == 1;
    }
};
void test_profile_queue_bad_port_item_stays_bounded_without_backend_or_borrowed_result() {
    BadProfileQueue queue;
    queue.value.principal = 7;
    queue.value.epoch = 1;
    queue.value.count = 65535;
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    ProfileFramePump pump(queue, source, clock);
    const auto actual = pump.service();
    TEST_ASSERT_TRUE(actual.received);
    receiveOutcome(ProfileReceiveOutcome::InvalidFrame, actual.outcome);
    TEST_ASSERT_EQUAL_UINT32(0, store.begins);
    TEST_ASSERT_FALSE(pump.service().received);
    TEST_ASSERT_EQUAL_UINT32(2, queue.calls);
}
