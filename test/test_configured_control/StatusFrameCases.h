#pragma once
void statusOutcome(StatusSendOutcome expected, StatusSendOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
RoutedConfiguredStatus currentStatus() {
    RoutedConfiguredStatus value;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(value));
    return value;
}
std::string statusJson(const ConfiguredStatus &value) {
    char bytes[maxConfiguredStatusJsonBytes];
    std::size_t count = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeConfiguredStatus(value, bytes, sizeof(bytes), count)));
    return std::string(bytes, count);
}
void completeStatus(StatusFrameSender &sender, std::uint32_t now, std::size_t frameBytes = 20) {
    for (unsigned i = 0; i < maxConfiguredStatusJsonBytes && sender.pending(); ++i) {
        const auto result = sender.service(now, frameBytes);
        TEST_ASSERT_TRUE(result == StatusSendOutcome::SentFrame ||
                         result == StatusSendOutcome::Completed);
    }
    TEST_ASSERT_FALSE(sender.pending());
}
std::string reconstructedStatus(const TestReplyBearer &bearer, std::uint16_t message = 1,
                                std::uint32_t epoch = 1) {
    std::string output;
    std::uint16_t total = 0;
    for (const auto &packet : bearer.frames) {
        TEST_ASSERT_EQUAL_UINT32(7, packet.principal);
        TEST_ASSERT_EQUAL_UINT32(epoch, packet.epoch);
        StatusFrame value;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(FrameDecodeResult::Success),
            static_cast<unsigned>(decodeStatusFrame(packet.bytes.data(), packet.count, value)));
        TEST_ASSERT_EQUAL_UINT16(message, value.message);
        TEST_ASSERT_EQUAL_UINT16(output.size(), value.offset);
        if (!total)
            total = value.total;
        TEST_ASSERT_EQUAL_UINT16(total, value.total);
        output.append(value.payload, value.count);
    }
    TEST_ASSERT_EQUAL_UINT32(total, output.size());
    return output;
}
void statusFrameFixture(const char *name, const TestReplyBearer &bearer) {
    std::string lines;
    for (const auto &packet : bearer.frames) {
        for (std::size_t i = 0; i < packet.count; ++i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned char>(packet.bytes[i]));
            lines += hex;
        }
        lines += '\n';
    }
    const auto path = std::string("docs/companion/fixtures/") + name + ".txt";
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *file = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(lines.size(), std::fwrite(lines.data(), 1, lines.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path.c_str(), "rt");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(lines.c_str(), actual.c_str());
}
void test_status_frame_codec_kind_capacity_and_target_preservation() {
    char bytes[130];
    std::memset(bytes, '#', sizeof(bytes));
    const std::string payload(120, 'x');
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeStatusFrame(0xabcd, 6143, 256, payload.data(), payload.size(), bytes + 1,
                                       128, written));
    const unsigned char header[] = {1, 4, 0xcd, 0xab, 0xff, 0x17, 0, 1};
    TEST_ASSERT_EQUAL_MEMORY(header, bytes + 1, sizeof(header));
    TEST_ASSERT_EQUAL_UINT32(128, written);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[129]);
    StatusFrame value;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(decodeStatusFrame(bytes + 1, written, value)));
    TEST_ASSERT_TRUE(value.payload == bytes + 9);
    TEST_ASSERT_EQUAL_UINT16(6143, value.total);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                      static_cast<unsigned>(decodeCommandFrame(bytes + 1, written, value)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                      static_cast<unsigned>(decodeReplyFrame(bytes + 1, written, value)));
    for (const auto kind : {1, 2, 3}) {
        bytes[2] = static_cast<char>(kind);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                          static_cast<unsigned>(decodeStatusFrame(bytes + 1, written, value)));
        TEST_ASSERT_EQUAL_UINT16(6143, value.total);
    }
    bytes[2] = 4;
    bytes[5] = 0;
    bytes[6] = 0x18;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                      static_cast<unsigned>(decodeStatusFrame(bytes + 1, written, value)));
    TEST_ASSERT_EQUAL_UINT16(6143, value.total);
    for (const auto length : {0U, 6144U})
        TEST_ASSERT_FALSE(encodeStatusFrame(1, length, 0, "x", 1, bytes, sizeof(bytes), written));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[0]);
    TEST_ASSERT_FALSE(encodeStatusFrame(0, 1, 0, "x", 1, bytes, sizeof(bytes), written));
    TEST_ASSERT_FALSE(encodeStatusFrame(1, 1, 0, "x", 1, bytes, 8, written));
    TEST_ASSERT_FALSE(
        encodeStatusFrame(1, 120, 0, payload.data(), 121, bytes, sizeof(bytes), written));
}
void test_ready_mailbox_status_frames_match_encoder_and_independent_fixture() {
    const auto routed = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    completeStatus(sender, f->now);
    TEST_ASSERT_EQUAL_UINT32(143, bearer.frames.size());
    const auto output = reconstructedStatus(bearer);
    TEST_ASSERT_EQUAL_STRING(statusJson(routed.status).c_str(), output.c_str());
    fixture("status-ready", output.data(), output.size());
    statusFrameFixture("status-ready-frames-v1", bearer);
    bearer.frames.clear();
    StatusFrameSender wide(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, wide.load(routed, f->now));
    completeStatus(wide, f->now, 128);
    TEST_ASSERT_EQUAL_UINT32(15, bearer.frames.size());
    TEST_ASSERT_EQUAL_STRING(output.c_str(), reconstructedStatus(bearer).c_str());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
}
void test_running_mailbox_status_128_frames_match_actual_start_fixture() {
    f->execute(f->command());
    const auto routed = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    completeStatus(sender, f->now, 128);
    TEST_ASSERT_EQUAL_UINT32(15, bearer.frames.size());
    const auto output = reconstructedStatus(bearer);
    TEST_ASSERT_EQUAL_STRING(statusJson(routed.status).c_str(), output.c_str());
    fixture("status-running", output.data(), output.size());
    statusFrameFixture("status-running128-frames-v1", bearer);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_status_snapshot_stays_frozen_when_real_mailbox_is_overwritten() {
    const auto original = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(original, f->now));
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(f->now, 20));
    f->execute(f->command());
    const auto next = currentStatus();
    TEST_ASSERT_TRUE(next.status.sample > original.status.sample);
    TEST_ASSERT_TRUE(next.status.running);
    statusOutcome(StatusSendOutcome::Busy, sender.load(next, f->now));
    completeStatus(sender, f->now);
    const auto output = reconstructedStatus(bearer);
    TEST_ASSERT_EQUAL_STRING(statusJson(original.status).c_str(), output.c_str());
    TEST_ASSERT_FALSE(original.status.running);
    bearer.frames.clear();
    statusOutcome(StatusSendOutcome::Loaded, sender.load(next, f->now));
    completeStatus(sender, f->now, 128);
    TEST_ASSERT_EQUAL_STRING(statusJson(next.status).c_str(),
                             reconstructedStatus(bearer, 2).c_str());
}
void test_real_reply_priority_reserves_whole_step_including_completion() {
    TestReplyBearer bearer;
    StatusFrameSender status(f->channel.actual, bearer);
    const auto original = currentStatus();
    statusOutcome(StatusSendOutcome::Loaded, status.load(original, f->now));
    // First status value was copied before the game command became ready.
    statusOutcome(StatusSendOutcome::SentFrame, status.service(f->now, 20));
    const auto reply = executeWithCurrentEpoch(GameCommandKind::Start);
    ReplyFrameSender replies(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, replies.load(reply, f->now));
    for (unsigned i = 0; i < 100 && replies.pending(); ++i) {
        const auto calls = bearer.calls;
        const auto step = serviceConfiguredFrames(replies, status, f->now, 128);
        TEST_ASSERT_TRUE(step.reply == ReplySendOutcome::SentFrame ||
                         step.reply == ReplySendOutcome::Completed);
        statusOutcome(StatusSendOutcome::Deferred, step.status);
        TEST_ASSERT_EQUAL_UINT32(calls + 1, bearer.calls);
        TEST_ASSERT_EQUAL_UINT8(2, bearer.last.bytes[1]);
    }
    TEST_ASSERT_FALSE(replies.pending());
    TEST_ASSERT_TRUE(status.pending());
    const auto calls = bearer.calls;
    const auto step = serviceConfiguredFrames(replies, status, f->now, 128);
    sendOutcome(ReplySendOutcome::Idle, step.reply);
    statusOutcome(StatusSendOutcome::SentFrame, step.status);
    TEST_ASSERT_EQUAL_UINT32(calls + 1, bearer.calls);
    TEST_ASSERT_EQUAL_UINT8(4, bearer.last.bytes[1]);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().nextRequest);
    // Deferral cannot renew an already-started snapshot's deadline.
    statusOutcome(StatusSendOutcome::Expired,
                  status.service(original.status.sampledMilliseconds + 30000, 20, true));
}
void test_status_busy_keeps_exact_frame_then_first_copy_freshness_expires() {
    const auto routed = currentStatus();
    const auto base = routed.status.sampledMilliseconds;
    TestReplyBearer bearer;
    bearer.busy = true;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, base));
    statusOutcome(StatusSendOutcome::Busy, sender.service(base, 20));
    const auto attempted = bearer.attempt;
    statusOutcome(StatusSendOutcome::Busy, sender.service(base + 999, 20));
    TEST_ASSERT_EQUAL_MEMORY(attempted.bytes.data(), bearer.attempt.bytes.data(), attempted.count);
    statusOutcome(StatusSendOutcome::StaleStatus, sender.service(base + 1000, 20));
    TEST_ASSERT_EQUAL_UINT32(2, bearer.calls);
    TEST_ASSERT_FALSE(sender.pending());
    statusOutcome(StatusSendOutcome::AlreadySampled, sender.load(routed, base + 1000));
    auto next = routed;
    ++next.status.sample;
    next.status.sampledMilliseconds = base + 1000;
    bearer.busy = false;
    statusOutcome(StatusSendOutcome::Loaded, sender.load(next, base + 1000));
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base + 1000, 20));
    StatusFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeStatusFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(2, frame.message);
}
void test_status_partial_busy_and_deferral_never_renew_absolute_deadline() {
    const auto routed = currentStatus();
    const auto base = f->now;
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, base));
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base, 20));
    bearer.busy = true;
    statusOutcome(StatusSendOutcome::Busy, sender.service(base + 29999, 20));
    const auto attempted = bearer.attempt;
    bearer.busy = false;
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base + 29999, 20));
    TEST_ASSERT_EQUAL_MEMORY(attempted.bytes.data(), bearer.last.bytes.data(), attempted.count);
    statusOutcome(StatusSendOutcome::Expired, sender.service(base + 30000, 20, true));
    TEST_ASSERT_EQUAL_UINT32(3, bearer.calls);
    TEST_ASSERT_FALSE(sender.pending());
}
void test_status_deadline_clock_wrap_capacity_invalid_and_cancel() {
    f->execute(f->command());
    auto routed = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    auto invalid = routed;
    invalid.status.boot = {};
    statusOutcome(StatusSendOutcome::InvalidStatus, sender.load(invalid, f->now));
    invalid = routed;
    invalid.status.deviceCount = larp::configuration::Limits::Components + 1;
    statusOutcome(StatusSendOutcome::InvalidStatus, sender.load(invalid, f->now));
    TEST_ASSERT_FALSE(sender.pending());
    const std::uint32_t base = UINT32_MAX - 500U;
    routed.status.sampledMilliseconds = base;
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, base + 999U));
    invalid = routed;
    statusOutcome(StatusSendOutcome::Busy, sender.load(invalid, base + 999U));
    for (const auto bytes : {0U, 8U, 129U})
        statusOutcome(StatusSendOutcome::InvalidFrameSize, sender.service(base + 999U, bytes));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base + 999U, 9));
    TEST_ASSERT_EQUAL_UINT16(9, bearer.last.count);
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base + 30998U, 128));
    statusOutcome(StatusSendOutcome::Expired, sender.service(base + 30999U, 128));
    TEST_ASSERT_FALSE(sender.pending());
    ++routed.status.sample;
    routed.status.sampledMilliseconds = base + 31000U;
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, base + 31000U));
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(base + 31000U, 20));
    const auto calls = bearer.calls;
    sender.cancel();
    statusOutcome(StatusSendOutcome::Idle, sender.service(base + 31000U, 20));
    TEST_ASSERT_EQUAL_UINT32(calls, bearer.calls);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    statusOutcome(StatusSendOutcome::AlreadySampled, sender.load(routed, base + 31000U));
}
void test_status_source_age_sample_and_boot_cannot_reopen_old_document() {
    auto routed = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::StaleStatus, sender.load(routed, f->now + 1000));
    // Future producer timestamps are not treated as fresh on a wrapping clock.
    ++routed.status.sampledMilliseconds;
    statusOutcome(StatusSendOutcome::StaleStatus, sender.load(routed, f->now));
    --routed.status.sampledMilliseconds;
    routed.status.sample = 2;
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    completeStatus(sender, f->now, 128);
    statusOutcome(StatusSendOutcome::AlreadySampled, sender.load(routed, f->now));
    --routed.status.sample;
    statusOutcome(StatusSendOutcome::StaleStatus, sender.load(routed, f->now));
    routed.status.sample = 3;
    ++routed.status.boot.low;
    statusOutcome(StatusSendOutcome::InvalidStatus, sender.load(routed, f->now));
    TEST_ASSERT_FALSE(sender.pending());
}
void test_status_reconnect_rejects_old_tags_and_new_producer_resets_counter() {
    const auto old = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(old, f->now));
    f->channel.actual.updateAuthority(authority(2, false));
    statusOutcome(StatusSendOutcome::Unauthorized, sender.service(f->now, 20, true));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    f->channel.actual.updateAuthority(authority(3));
    statusOutcome(StatusSendOutcome::Unauthorized, sender.load(old, f->now));
    RoutedConfiguredStatus current;
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(current));
    f->control.service(++f->now);
    current = currentStatus();
    statusOutcome(StatusSendOutcome::Loaded, sender.load(current, f->now));
    completeStatus(sender, f->now, 128);
    TEST_ASSERT_EQUAL_STRING(statusJson(current.status).c_str(),
                             reconstructedStatus(bearer, 1, 3).c_str());
    TEST_ASSERT_EQUAL_UINT32(2, current.status.controlLease);
    auto wrong = current;
    wrong.principal = 8;
    statusOutcome(StatusSendOutcome::Unauthorized, sender.load(wrong, f->now));
}
void test_status_bearer_revoke_before_and_after_copy_observer_and_unavailable() {
    f->execute(f->command());
    auto routed = currentStatus();
    TestReplyBearer bearer;
    StatusFrameSender sender(f->channel.actual, bearer);
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    bearer.revokeBefore = true;
    statusOutcome(StatusSendOutcome::TransportUnavailable, sender.service(f->now, 20));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.frames.size());
    TEST_ASSERT_TRUE(f->application.gameRunning());
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    routed = currentStatus();
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    statusOutcome(StatusSendOutcome::SentFrame, sender.service(f->now, 20));
    f->channel.actual.updateAuthority(authority(4, false));
    statusOutcome(StatusSendOutcome::Unauthorized, sender.service(f->now, 20));
    TEST_ASSERT_EQUAL_UINT32(3,
                             bearer.last.epoch); // Already copied frame keeps its original binding.
    TEST_ASSERT_TRUE(f->application.gameRunning());
    f->channel.actual.updateAuthority(authority(5));
    f->control.service(++f->now);
    routed = currentStatus();
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    bearer.available = false;
    statusOutcome(StatusSendOutcome::TransportUnavailable, sender.service(f->now, 20));
    f->channel.actual.updateAuthority(authority(6, false));
    statusOutcome(StatusSendOutcome::Unauthorized, sender.load(routed, f->now));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_status_message_exhaustion_no_wrap_and_cancel_consumes_identity() {
    auto routed = currentStatus();
    TestReplyBearer bearer;
    bearer.record = false;
    StatusFrameSender sender(f->channel.actual, bearer);
    for (std::uint32_t message = 1; message <= UINT16_MAX; ++message) {
        routed.status.sample = message;
        statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
        if (message != UINT16_MAX)
            sender.cancel();
    }
    completeStatus(sender, f->now, 128);
    StatusFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeStatusFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, frame.message);
    ++routed.status.sample;
    statusOutcome(StatusSendOutcome::MessageExhausted, sender.load(routed, f->now));
    TEST_ASSERT_FALSE(sender.pending());
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    routed = currentStatus();
    statusOutcome(StatusSendOutcome::Loaded, sender.load(routed, f->now));
    completeStatus(sender, f->now, 128);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeStatusFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(1, frame.message);
    TEST_ASSERT_EQUAL_UINT32(3, bearer.last.epoch);
}
