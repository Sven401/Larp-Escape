#pragma once
void sendOutcome(ReplySendOutcome expected, ReplySendOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
class TestReplyBearer : public IReplyFrameBearer {
  public:
    bool busy = false, available = true, revokeBefore = false, revokeAfter = false;
    bool record = true;
    unsigned calls = 0;
    RoutedReplyFrame last, attempt;
    std::vector<RoutedReplyFrame> frames;
    ReplyFrameCopy copyReplyFrame(const RoutedReplyFrame &frame) noexcept override {
        ++calls;
        attempt = frame;
        if (revokeBefore) {
            revokeBefore = false;
            f->channel.actual.updateAuthority(authority(2, false));
        }
        TrustedAuthority current;
        if (!available || !f->channel.actual.readAuthority(current) || !current.gameMaster ||
            current.principal != frame.principal || current.epoch != frame.epoch)
            return ReplyFrameCopy::Unavailable;
        if (busy)
            return ReplyFrameCopy::Busy;
        last = frame;
        if (record)
            frames.push_back(frame);
        if (revokeAfter) {
            revokeAfter = false;
            f->channel.actual.updateAuthority(authority(2, false));
        }
        return ReplyFrameCopy::Copied;
    }
};
void completeReply(ReplyFrameSender &sender, std::uint32_t now = 100, std::size_t frameBytes = 20) {
    for (unsigned i = 0; i < maxGameReplyJsonBytes && sender.pending(); ++i) {
        const auto outcome = sender.service(now, frameBytes);
        TEST_ASSERT_TRUE(outcome == ReplySendOutcome::SentFrame ||
                         outcome == ReplySendOutcome::Completed);
    }
    TEST_ASSERT_FALSE(sender.pending());
}
RoutedGameReply executeWithCurrentEpoch(GameCommandKind kind) {
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_TRUE(f->submit(f->command(kind), current.epoch, current.principal));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    return reply;
}
void test_reply_frame_codec_separate_kind_bounds_and_target_preservation() {
    char bytes[CommandFrameLimits::Frame + 2];
    std::memset(bytes, '#', sizeof(bytes));
    std::size_t written = 99;
    const std::string payload(120, 'x');
    TEST_ASSERT_TRUE(encodeReplyFrame(0xabcd, 511, 256, payload.data(), payload.size(), bytes + 1,
                                      128, written));
    TEST_ASSERT_EQUAL_UINT32(128, written);
    const unsigned char header[] = {1, 2, 0xcd, 0xab, 0xff, 1, 0, 1};
    TEST_ASSERT_EQUAL_MEMORY(header, bytes + 1, sizeof(header));
    TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[129]);
    ReplyFrame decoded;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(decodeReplyFrame(bytes + 1, written, decoded)));
    TEST_ASSERT_TRUE(decoded.payload == bytes + 9);
    TEST_ASSERT_EQUAL_UINT16(511, decoded.total);
    TEST_ASSERT_EQUAL_UINT16(256, decoded.offset);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                      static_cast<unsigned>(decodeCommandFrame(bytes + 1, written, decoded)));
    TEST_ASSERT_EQUAL_UINT16(511, decoded.total);
    bytes[2] = 1;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                      static_cast<unsigned>(decodeReplyFrame(bytes + 1, written, decoded)));
    TEST_ASSERT_EQUAL_UINT16(256, decoded.offset);
    bytes[2] = 2;
    bytes[5] = 0;
    bytes[6] = 2; // 512 exceeds reply JSON payload capacity.
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                      static_cast<unsigned>(decodeReplyFrame(bytes + 1, written, decoded)));
    TEST_ASSERT_EQUAL_UINT16(511, decoded.total);
    for (const auto total : {0U, 512U})
        TEST_ASSERT_FALSE(encodeReplyFrame(1, total, 0, "x", 1, bytes + 1, 128, written));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[1]);
    TEST_ASSERT_FALSE(encodeReplyFrame(0, 1, 0, "x", 1, bytes, sizeof(bytes), written));
    TEST_ASSERT_FALSE(encodeReplyFrame(1, 1, 0, "x", 1, bytes, 8, written));
}
void test_executed_reply_frames_reconstruct_actual_firmware_fixture() {
    TEST_ASSERT_TRUE(f->submit(f->command()));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
    completeReply(sender);
    std::string reconstructed, lines;
    std::uint16_t total = 0;
    for (const auto &packet : bearer.frames) {
        TEST_ASSERT_EQUAL_UINT32(7, packet.principal);
        TEST_ASSERT_EQUAL_UINT32(1, packet.epoch);
        TEST_ASSERT_TRUE(packet.count <= 20);
        ReplyFrame frame;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(FrameDecodeResult::Success),
            static_cast<unsigned>(decodeReplyFrame(packet.bytes.data(), packet.count, frame)));
        TEST_ASSERT_EQUAL_UINT16(1, frame.message);
        TEST_ASSERT_EQUAL_UINT16(reconstructed.size(), frame.offset);
        if (!total)
            total = frame.total;
        TEST_ASSERT_EQUAL_UINT16(total, frame.total);
        reconstructed.append(frame.payload, frame.count);
        for (std::size_t i = 0; i < packet.count; ++i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned char>(packet.bytes[i]));
            lines += hex;
        }
        lines += '\n';
    }
    TEST_ASSERT_EQUAL_UINT32(total, reconstructed.size());
    auto *file = std::fopen("docs/companion/fixtures/v2/reply-executed.json", "rt");
    TEST_ASSERT_NOT_NULL(file);
    char expected[maxGameReplyJsonBytes + 1];
    const auto read = std::fread(expected, 1, sizeof(expected), file);
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_UINT32(reconstructed.size() + 1, read);
    TEST_ASSERT_EQUAL_MEMORY(reconstructed.data(), expected, reconstructed.size());
    const char *path = "docs/companion/fixtures/reply-frames-v1.txt";
    const char *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(lines.size(), std::fwrite(lines.data(), 1, lines.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    file = std::fopen(path, "rt");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(lines.c_str(), actual.c_str());
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().nextRequest);
}
void test_reply_sender_busy_retains_exact_frame_and_original_deadline() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
    bearer.busy = true;
    sendOutcome(ReplySendOutcome::Busy, sender.service(101, 20));
    const auto first = bearer.attempt;
    sendOutcome(ReplySendOutcome::Busy, sender.service(2099, 20));
    TEST_ASSERT_EQUAL_MEMORY(first.bytes.data(), bearer.attempt.bytes.data(), first.count);
    TEST_ASSERT_EQUAL_UINT32(0, bearer.frames.size());
    bearer.busy = false;
    sendOutcome(ReplySendOutcome::SentFrame, sender.service(2099, 20));
    TEST_ASSERT_EQUAL_MEMORY(first.bytes.data(), bearer.last.bytes.data(), first.count);
    sendOutcome(ReplySendOutcome::Expired, sender.service(2100, 20));
    TEST_ASSERT_FALSE(sender.pending());
    TEST_ASSERT_EQUAL_UINT32(3, bearer.calls);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_reply_sender_frame_capacity_and_pending_reply_preservation() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
    auto invalid = reply;
    invalid.reply.boot = {};
    sendOutcome(ReplySendOutcome::Busy, sender.load(invalid, 101));
    for (const auto count : {0U, 8U, 129U})
        sendOutcome(ReplySendOutcome::InvalidFrameSize, sender.service(101, count));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    completeReply(sender, 102, 128);
    TEST_ASSERT_TRUE(!bearer.frames.empty());
    ReplyFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(decodeReplyFrame(bearer.frames[0].bytes.data(),
                                                             bearer.frames[0].count, frame)));
    TEST_ASSERT_EQUAL_UINT16(1, frame.message);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 103));
    sendOutcome(ReplySendOutcome::SentFrame, sender.service(103, 9));
    TEST_ASSERT_EQUAL_UINT16(9, bearer.last.count); // Smallest legal outer value.
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeReplyFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(2, frame.message);
}
void test_reply_sender_reconnect_rejects_old_reply_and_binds_new_epoch() {
    const auto old = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(old, 100));
    f->channel.actual.updateAuthority(authority(2, false));
    sendOutcome(ReplySendOutcome::Unauthorized, sender.service(101, 20));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    sendOutcome(ReplySendOutcome::Unauthorized, sender.load(old, 102));
    auto wrong = old;
    wrong.principal = 8;
    sendOutcome(ReplySendOutcome::Unauthorized, sender.load(wrong, 102));
    const auto current = executeWithCurrentEpoch(GameCommandKind::Stop);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(current, 103));
    completeReply(sender, 104);
    TEST_ASSERT_TRUE(!bearer.frames.empty());
    for (const auto &packet : bearer.frames) {
        TEST_ASSERT_EQUAL_UINT32(7, packet.principal);
        TEST_ASSERT_EQUAL_UINT32(3, packet.epoch);
    }
    ReplyFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeReplyFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(1, frame.message); // Reset only with new verified epoch.
}
void test_reply_bearer_rechecks_revoke_before_copy() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
    bearer.revokeBefore = true;
    sendOutcome(ReplySendOutcome::TransportUnavailable, sender.service(101, 20));
    TEST_ASSERT_FALSE(sender.pending());
    TEST_ASSERT_EQUAL_UINT32(0, bearer.frames.size());
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_reply_copy_before_later_revoke_keeps_original_epoch() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
    bearer.revokeAfter = true;
    sendOutcome(ReplySendOutcome::SentFrame, sender.service(101, 20));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.last.epoch);
    sendOutcome(ReplySendOutcome::Unauthorized, sender.service(102, 20));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.calls);
    TEST_ASSERT_EQUAL_UINT32(1, bearer.last.epoch); // No retroactive unsend/relabel.
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_reply_sender_deadline_wrap_and_invalid_reply() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    ReplyFrameSender sender(f->channel.actual, bearer);
    auto invalid = reply;
    invalid.reply.boot = {};
    sendOutcome(ReplySendOutcome::InvalidReply, sender.load(invalid, 0));
    TEST_ASSERT_FALSE(sender.pending());
    const std::uint32_t base = UINT32_MAX - 1000U;
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, base));
    sendOutcome(ReplySendOutcome::SentFrame, sender.service(base + 1999U, 20));
    sendOutcome(ReplySendOutcome::Expired, sender.service(base + 2000U, 20));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.calls);
    sendOutcome(ReplySendOutcome::Idle, sender.service(base + 2001U, 20));
    sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 3000));
    bearer.available = false;
    sendOutcome(ReplySendOutcome::TransportUnavailable, sender.service(3000, 20));
    TEST_ASSERT_FALSE(sender.pending());
}
void test_reply_message_counter_exhaustion_never_wraps_and_new_epoch_recovers() {
    const auto reply = f->execute(f->command());
    TestReplyBearer bearer;
    bearer.record = false; // Fixed last copy; no unbounded test capture for this loop.
    ReplyFrameSender sender(f->channel.actual, bearer);
    for (std::uint32_t message = 1; message <= UINT16_MAX; ++message) {
        sendOutcome(ReplySendOutcome::Loaded, sender.load(reply, 100));
        completeReply(sender, 100, 128);
        ReplyFrame frame;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                          static_cast<unsigned>(decodeReplyFrame(bearer.last.bytes.data(),
                                                                 bearer.last.count, frame)));
        TEST_ASSERT_EQUAL_UINT16(message, frame.message);
    }
    const auto calls = bearer.calls;
    sendOutcome(ReplySendOutcome::MessageExhausted, sender.load(reply, 100));
    TEST_ASSERT_FALSE(sender.pending());
    sendOutcome(ReplySendOutcome::Idle, sender.service(100, 20));
    TEST_ASSERT_EQUAL_UINT32(calls, bearer.calls);
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    sendOutcome(ReplySendOutcome::Unauthorized, sender.load(reply, 100));
    const auto current = executeWithCurrentEpoch(GameCommandKind::Stop);
    sendOutcome(ReplySendOutcome::Loaded, sender.load(current, 100));
    completeReply(sender, 100, 128);
    ReplyFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(
                          decodeReplyFrame(bearer.last.bytes.data(), bearer.last.count, frame)));
    TEST_ASSERT_EQUAL_UINT16(1, frame.message);
    TEST_ASSERT_EQUAL_UINT32(3, bearer.last.epoch);
}
