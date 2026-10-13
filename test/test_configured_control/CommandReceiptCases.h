#pragma once
void receiptCopy(ReceiptCopyOutcome expected, ReceiptCopyOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
CommandReceipt receiptOf(const RoutedReplyFrame &packet) {
    CommandReceipt receipt;
    TEST_ASSERT_EQUAL_UINT16(CommandReceiptLimits::Bytes, packet.count);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(FrameDecodeResult::Success),
        static_cast<unsigned>(decodeCommandReceipt(packet.bytes.data(), packet.count, receipt)));
    return receipt;
}
FrameDispatchResult deliverReceipt(FreeRtosCommandFrameQueue &queue, CommandStream &stream,
                                   TestReplyBearer &bearer, const std::string &document,
                                   std::size_t offset, std::size_t count,
                                   std::uint16_t message = 1) {
    const auto bytes = frameBytes(document, offset, count, message);
    copyOutcome(FrameCopyOutcome::Copied, queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 100));
    const auto dispatch = dispatchCommandFrame(queue, stream, 100);
    receiptCopy(bearer.busy ? ReceiptCopyOutcome::Busy : ReceiptCopyOutcome::Copied,
                copyCommandReceipt(dispatch, f->channel.actual, bearer));
    return dispatch;
}
void test_command_receipt_codec_codes_bounds_and_target_preservation() {
    CommandReceipt value;
    value.input.message = 0xabcd;
    value.input.total = 1024;
    value.input.offset = 256;
    value.input.count = 120;
    value.message = 7;
    value.total = 1024;
    value.received = 512;
    const CommandStreamOutcome outcomes[] = {CommandStreamOutcome::Receiving,
                                             CommandStreamOutcome::DuplicateFragment,
                                             CommandStreamOutcome::Queued,
                                             CommandStreamOutcome::QueueFull,
                                             CommandStreamOutcome::DecodeRejected,
                                             CommandStreamOutcome::InvalidFrame,
                                             CommandStreamOutcome::UnsupportedFrame,
                                             CommandStreamOutcome::Unauthorized,
                                             CommandStreamOutcome::Busy,
                                             CommandStreamOutcome::OutOfOrder,
                                             CommandStreamOutcome::FragmentConflict,
                                             CommandStreamOutcome::Expired,
                                             CommandStreamOutcome::Uninitialized};
    char bytes[20];
    for (unsigned i = 0; i < 13; ++i) {
        std::memset(bytes, '#', sizeof(bytes));
        value.outcome = outcomes[i];
        value.message = i == 8 ? 7 : value.input.message;
        value.total = 1024;
        value.received = i == 2 || i == 3 || i == 4 || i == 12 ? 1024 : 512;
        std::size_t written = 99;
        TEST_ASSERT_TRUE(encodeCommandReceipt(value, bytes + 1, 18, written));
        TEST_ASSERT_EQUAL_UINT32(18, written);
        TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
        TEST_ASSERT_EQUAL_CHAR('#', bytes[19]);
        const unsigned char header[] = {1,
                                        3,
                                        static_cast<unsigned char>(i + 1),
                                        0,
                                        0xcd,
                                        0xab,
                                        0,
                                        4,
                                        0,
                                        1,
                                        120,
                                        0,
                                        static_cast<unsigned char>(value.message & 0xff),
                                        static_cast<unsigned char>(value.message >> 8),
                                        0,
                                        4,
                                        static_cast<unsigned char>(value.received & 0xff),
                                        static_cast<unsigned char>(value.received >> 8)};
        TEST_ASSERT_EQUAL_MEMORY(header, bytes + 1, 18);
        CommandReceipt decoded;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                          static_cast<unsigned>(decodeCommandReceipt(bytes + 1, 18, decoded)));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(value.outcome),
                          static_cast<unsigned>(decoded.outcome));
        TEST_ASSERT_EQUAL_UINT16(value.received, decoded.received);
        ReplyFrame ordinary;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                          static_cast<unsigned>(decodeReplyFrame(bytes + 1, 18, ordinary)));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Unsupported),
                          static_cast<unsigned>(decodeCommandFrame(bytes + 1, 18, ordinary)));
    }
    CommandReceipt target;
    target.input.message = 999;
    for (unsigned index : {0U, 1U, 2U, 3U, 4U, 6U, 10U, 14U, 16U}) {
        auto bad = std::string(bytes + 1, 18);
        bad[index] = (index == 0 || index == 1 || index == 3) ? 9 : 0;
        if (index == 4 || index == 6 || index == 10 || index == 14)
            bad[index + 1] = 0;
        if (index == 16) {
            bad[index] = -1;
            bad[index + 1] = -1;
        }
        TEST_ASSERT_TRUE(decodeCommandReceipt(bad.data(), bad.size(), target) !=
                         FrameDecodeResult::Success);
        TEST_ASSERT_EQUAL_UINT16(999, target.input.message);
    }
    for (std::size_t count : {0U, 17U, 19U})
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                          static_cast<unsigned>(decodeCommandReceipt(bytes, count, target)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                      static_cast<unsigned>(decodeCommandReceipt(nullptr, 18, target)));
    std::size_t written = 99;
    TEST_ASSERT_FALSE(encodeCommandReceipt(value, bytes, 17, written));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[0]);
    value.outcome = CommandStreamOutcome::Idle;
    TEST_ASSERT_FALSE(encodeCommandReceipt(value, bytes, sizeof(bytes), written));
    value.outcome = CommandStreamOutcome::Expired;
    value.message = value.total = value.received = 0;
    TEST_ASSERT_TRUE(encodeCommandReceipt(value, bytes, sizeof(bytes), written));
}
void test_actual_fragment_receipts_precede_execution_and_match_fixture() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    const auto document = json(f->command());
    std::string lines;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        deliverReceipt(queue, stream, bearer, document, offset, count);
        const auto receipt = receiptOf(bearer.last);
        const auto expected = offset + count == document.size() ? CommandStreamOutcome::Queued
                                                                : CommandStreamOutcome::Receiving;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(receipt.outcome));
        TEST_ASSERT_EQUAL_UINT16(1, receipt.input.message);
        TEST_ASSERT_EQUAL_UINT16(document.size(), receipt.input.total);
        TEST_ASSERT_EQUAL_UINT16(offset, receipt.input.offset);
        TEST_ASSERT_EQUAL_UINT16(count, receipt.input.count);
        TEST_ASSERT_EQUAL_UINT16(offset + count, receipt.received);
        TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
        TEST_ASSERT_FALSE(f->application.gameRunning());
        for (std::size_t i = 0; i < bearer.last.count; ++i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X",
                          static_cast<unsigned char>(bearer.last.bytes[i]));
            lines += hex;
        }
        lines += '\n';
        offset += count;
    }
    TEST_ASSERT_EQUAL_UINT32(13, bearer.calls);
    const char *path = "docs/companion/fixtures/command-receipts-v1.txt";
    const char *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(lines.size(), std::fwrite(lines.data(), 1, lines.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rt");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(lines.c_str(), actual.c_str());
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_receipt_busy_does_not_undo_queued_command_or_retry() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    bearer.busy = true;
    const auto document = json(f->command());
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(120, document.size() - offset);
        deliverReceipt(queue, stream, bearer, document, offset, count);
        offset += count;
    }
    const auto calls = bearer.calls;
    receiptCopy(
        ReceiptCopyOutcome::NoFrame,
        copyCommandReceipt(dispatchCommandFrame(queue, stream, 101), f->channel.actual, bearer));
    TEST_ASSERT_EQUAL_UINT32(calls, bearer.calls);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_FALSE(reply.reply.duplicate);
    bearer.busy = false;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(120, document.size() - offset);
        deliverReceipt(queue, stream, bearer, document, offset, count);
        offset += count;
    }
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Queued),
                      static_cast<unsigned>(receiptOf(bearer.last).outcome));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_TRUE(reply.reply.duplicate);
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().nextRequest);
}
void test_command_queue_full_receipt_and_explicit_duplicate_retry() {
    auto wrong = f->command();
    wrong.boot.low ^= 1;
    for (unsigned i = 0; i < ControlLimits::Queue; ++i)
        TEST_ASSERT_TRUE(f->submit(wrong));
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    const auto document = json(f->command());
    std::size_t last = 0, lastCount = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(120, document.size() - offset);
        deliverReceipt(queue, stream, bearer, document, offset, count);
        last = offset;
        lastCount = count;
        offset += count;
    }
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::QueueFull),
                      static_cast<unsigned>(receiptOf(bearer.last).outcome));
    TEST_ASSERT_TRUE(stream.snapshot().complete);
    f->control.service(++f->now);
    RoutedGameReply reply;
    for (unsigned i = 0; i < ControlLimits::Queue; ++i) {
        TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::WrongBoot),
                          static_cast<unsigned>(reply.reply.outcome));
    }
    TEST_ASSERT_FALSE(f->application.gameRunning());
    deliverReceipt(queue, stream, bearer, document, last, lastCount);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Queued),
                      static_cast<unsigned>(receiptOf(bearer.last).outcome));
    TEST_ASSERT_FALSE(f->application.gameRunning());
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
}
void test_receipt_decode_rejection_does_not_consume_game_request() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    deliverReceipt(queue, stream, bearer, "{}", 0, 2);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::DecodeRejected),
                      static_cast<unsigned>(receiptOf(bearer.last).outcome));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_receipt_distinguishes_input_from_busy_pending_message() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    const auto document = json(f->command());
    deliverReceipt(queue, stream, bearer, document, 0, 12, 4);
    deliverReceipt(queue, stream, bearer, document, 0, 12, 5);
    auto receipt = receiptOf(bearer.last);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Busy),
                      static_cast<unsigned>(receipt.outcome));
    TEST_ASSERT_EQUAL_UINT16(5, receipt.input.message);
    TEST_ASSERT_EQUAL_UINT16(4, receipt.message);
    TEST_ASSERT_EQUAL_UINT16(12, receipt.received);
    deliverReceipt(queue, stream, bearer, document, 13, 12, 4);
    receipt = receiptOf(bearer.last);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::OutOfOrder),
                      static_cast<unsigned>(receipt.outcome));
    TEST_ASSERT_EQUAL_UINT16(13, receipt.input.offset);
    TEST_ASSERT_EQUAL_UINT16(12, receipt.received);
    auto changed = document;
    changed[0] = 'x';
    deliverReceipt(queue, stream, bearer, changed, 0, 12, 4);
    receipt = receiptOf(bearer.last);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::FragmentConflict),
                      static_cast<unsigned>(receipt.outcome));
    TEST_ASSERT_EQUAL_UINT16(4, receipt.message); // Result before stream discard.
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().message);
}
void test_receipt_expired_queuewait_and_empty_poll() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    const auto bytes = frameBytes("{}", 0, 2);
    copyOutcome(FrameCopyOutcome::Copied,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, UINT32_MAX - 9));
    const auto dispatch = dispatchCommandFrame(queue, stream, 1990); // Exactly2000ms.
    receiptCopy(ReceiptCopyOutcome::Copied,
                copyCommandReceipt(dispatch, f->channel.actual, bearer));
    const auto receipt = receiptOf(bearer.last);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(CommandStreamOutcome::Expired),
                      static_cast<unsigned>(receipt.outcome));
    TEST_ASSERT_EQUAL_UINT16(1, receipt.input.message);
    TEST_ASSERT_EQUAL_UINT16(0, receipt.message);
    receiptCopy(
        ReceiptCopyOutcome::NoFrame,
        copyCommandReceipt(dispatchCommandFrame(queue, stream, 1991), f->channel.actual, bearer));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.calls);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_receipt_reconnect_and_bearer_revoke_keep_original_binding() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    TestReplyBearer bearer;
    const auto document = json(f->command());
    const auto bytes = frameBytes(document, 0, 12);
    copyOutcome(FrameCopyOutcome::Copied, queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 100));
    const auto dispatch = dispatchCommandFrame(queue, stream, 100);
    bearer.revokeBefore = true;
    receiptCopy(ReceiptCopyOutcome::TransportUnavailable,
                copyCommandReceipt(dispatch, f->channel.actual, bearer));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.frames.size());
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    receiptCopy(ReceiptCopyOutcome::Unauthorized,
                copyCommandReceipt(dispatch, f->channel.actual, bearer));
    TEST_ASSERT_EQUAL_UINT32(1, bearer.calls);
    const auto fresh = frameBytes(json(f->command()), 0, 12, 2);
    copyOutcome(FrameCopyOutcome::Copied, queue.copyWrite(fresh.data(), fresh.size(), 7, 3, 101));
    const auto current = dispatchCommandFrame(queue, stream, 101);
    receiptCopy(ReceiptCopyOutcome::Copied, copyCommandReceipt(current, f->channel.actual, bearer));
    TEST_ASSERT_EQUAL_UINT32(3, bearer.last.epoch);
    TEST_ASSERT_EQUAL_UINT16(2, receiptOf(bearer.last).input.message);
    TEST_ASSERT_EQUAL_UINT32(1, dispatch.epoch); // Old copied metadata unchanged.
}
