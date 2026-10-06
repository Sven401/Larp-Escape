#pragma once
// Included inside the host fixture namespace; uses the real core/adapter.
void streamOutcome(CommandStreamOutcome expected, const CommandStreamResult &value) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(value.outcome));
}
std::string frameBytes(const std::string &document, std::size_t offset, std::size_t count,
                       std::uint16_t message = 1, std::uint16_t total = 0) {
    char bytes[CommandFrameLimits::Frame];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeCommandFrame(message, total ? total : document.size(), offset,
                                        document.data() + offset, count, bytes, sizeof(bytes),
                                        written));
    return std::string(bytes, written);
}
CommandStreamResult sendDocument(CommandStream &stream, const std::string &document,
                                 std::size_t chunk = 12, std::uint32_t principal = 7,
                                 std::uint32_t epoch = 1, std::uint32_t now = 0) {
    CommandStreamResult value;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min(chunk, document.size() - offset);
        auto bytes = frameBytes(document, offset, count);
        value = stream.receive(bytes.data(), bytes.size(), principal, epoch, now);
        bytes.assign(bytes.size(), '\0'); // Receiver cannot borrow frame storage.
        if (offset + count < document.size())
            streamOutcome(CommandStreamOutcome::Receiving, value);
        offset += count;
    }
    return value;
}
void test_frame_codec_limits_endianness_and_target_preservation() {
    const std::string document(1024, 'x');
    auto bytes = frameBytes(document, 256, 120, 0xabcd);
    const unsigned char expected[] = {1, 1, 0xcd, 0xab, 0, 4, 0, 1};
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes.data(), sizeof(expected));
    CommandFrame frame;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Success),
                      static_cast<unsigned>(decodeCommandFrame(bytes.data(), bytes.size(), frame)));
    TEST_ASSERT_EQUAL_UINT16(0xabcd, frame.message);
    TEST_ASSERT_EQUAL_UINT16(1024, frame.total);
    TEST_ASSERT_EQUAL_UINT16(256, frame.offset);
    TEST_ASSERT_EQUAL_UINT32(120, frame.count);
    const auto original = frame;
    const auto preserved = [&]() {
        TEST_ASSERT_EQUAL_UINT16(original.message, frame.message);
        TEST_ASSERT_EQUAL_UINT16(original.total, frame.total);
        TEST_ASSERT_EQUAL_UINT16(original.offset, frame.offset);
        TEST_ASSERT_TRUE(original.payload == frame.payload);
        TEST_ASSERT_EQUAL_UINT32(original.count, frame.count);
    };
    auto invalid = bytes;
    for (const auto index : {2, 4, 6}) {
        invalid = bytes;
        invalid[index] = invalid[index + 1] = static_cast<char>(0xff);
        if (index == 2)
            invalid[index] = invalid[index + 1] = 0;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(FrameDecodeResult::Invalid),
            static_cast<unsigned>(decodeCommandFrame(invalid.data(), invalid.size(), frame)));
        preserved();
    }
    invalid = bytes;
    invalid[0] = 2;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(FrameDecodeResult::Unsupported),
        static_cast<unsigned>(decodeCommandFrame(invalid.data(), invalid.size(), frame)));
    preserved();
    invalid[0] = 1;
    invalid[1] = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(FrameDecodeResult::Unsupported),
        static_cast<unsigned>(decodeCommandFrame(invalid.data(), invalid.size(), frame)));
    preserved();
    for (const auto count : {std::size_t(0), std::size_t(8), std::size_t(129)}) {
        TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                          static_cast<unsigned>(decodeCommandFrame(bytes.data(), count, frame)));
        preserved();
    }
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameDecodeResult::Invalid),
                      static_cast<unsigned>(decodeCommandFrame(nullptr, bytes.size(), frame)));
    preserved();
    char out[130] = {};
    std::size_t written = 42;
    TEST_ASSERT_FALSE(encodeCommandFrame(1, 1024, 256, document.data(), 120, out, 127, written));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, out[0]);
    TEST_ASSERT_FALSE(
        encodeCommandFrame(1, 1025, 0, document.data(), 1, out, sizeof(out), written));
    TEST_ASSERT_FALSE(
        encodeCommandFrame(0, 1024, 0, document.data(), 1, out, sizeof(out), written));
    TEST_ASSERT_FALSE(encodeCommandFrame(1, 1, 1, document.data(), 1, out, sizeof(out), written));
    TEST_ASSERT_FALSE(encodeCommandFrame(1, 1024, 0, nullptr, 1, out, sizeof(out), written));
}
void test_fragmented_command_runs_through_real_queue_application_and_reply() {
    CommandStream stream(f->channel.actual);
    const auto value = sendDocument(stream, json(f->command()), 12, 7, 1, f->now);
    streamOutcome(CommandStreamOutcome::Queued, value);
    TEST_ASSERT_EQUAL_UINT32(json(f->command()).size(), value.received);
    TEST_ASSERT_FALSE(stream.snapshot().complete);
    TEST_ASSERT_FALSE(f->application.gameRunning()); // Queued does not mean execution.
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_EQUAL_UINT32(7, reply.principal);
    TEST_ASSERT_EQUAL_UINT32(1, reply.connectionEpoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_partial_and_duplicate_fragments_do_not_execute_or_extend_deadline() {
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    const auto bytes = frameBytes(document, 0, 12);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 0));
    streamOutcome(CommandStreamOutcome::DuplicateFragment,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 1999));
    TEST_ASSERT_EQUAL_UINT16(12, stream.snapshot().received);
    f->control.service(++f->now);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    streamOutcome(CommandStreamOutcome::Expired, stream.poll(2000));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
}
void test_fragment_gaps_overlap_and_other_message_preserve_pending() {
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    auto bytes = frameBytes(document, 12, 12);
    streamOutcome(CommandStreamOutcome::OutOfOrder,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 0));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    bytes = frameBytes(document, 0, 12);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 0));
    bytes = frameBytes(document, 24, 12);
    streamOutcome(CommandStreamOutcome::OutOfOrder,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 1));
    bytes = frameBytes(document, 6, 12);
    streamOutcome(CommandStreamOutcome::OutOfOrder,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 2));
    bytes = frameBytes(document, 0, 12, 2);
    streamOutcome(CommandStreamOutcome::Busy, stream.receive(bytes.data(), bytes.size(), 7, 1, 3));
    bytes = frameBytes(document, 12, 12);
    auto broken = bytes;
    broken[0] = 2;
    streamOutcome(CommandStreamOutcome::UnsupportedFrame,
                  stream.receive(broken.data(), broken.size(), 7, 1, 4));
    streamOutcome(CommandStreamOutcome::InvalidFrame, stream.receive(nullptr, 20, 7, 1, 5));
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 6));
    TEST_ASSERT_EQUAL_UINT16(24, stream.snapshot().received);
}
void test_conflicting_duplicate_and_total_discard_pending() {
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    auto bytes = frameBytes(document, 0, 12);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 0));
    auto conflict = bytes;
    conflict[8] ^= 1;
    streamOutcome(CommandStreamOutcome::FragmentConflict,
                  stream.receive(conflict.data(), conflict.size(), 7, 1, 1));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(bytes.data(), bytes.size(), 7, 1, 2));
    conflict = frameBytes(document, 0, 12, 1, document.size() + 1);
    streamOutcome(CommandStreamOutcome::FragmentConflict,
                  stream.receive(conflict.data(), conflict.size(), 7, 1, 3));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
}
void test_reconnect_cannot_join_old_fragments_or_erase_new_pending() {
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    const auto first = frameBytes(document, 0, 12);
    const auto second = frameBytes(document, 12, 12);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 1, 0));
    f->channel.actual.updateAuthority(authority(2, false));
    f->channel.actual.updateAuthority(authority(3));
    streamOutcome(CommandStreamOutcome::OutOfOrder,
                  stream.receive(second.data(), second.size(), 7, 3, 1));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 3, 2));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  stream.receive(second.data(), second.size(), 7, 1, 3));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  stream.receive(second.data(), second.size(), 8, 3, 3));
    TEST_ASSERT_EQUAL_UINT16(12, stream.snapshot().received);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(second.data(), second.size(), 7, 3, 4));
    TEST_ASSERT_EQUAL_UINT16(24, stream.snapshot().received);
}
void fillCommandQueue() {
    auto wrongBoot = f->command();
    wrongBoot.boot.low ^= 1;
    for (unsigned i = 0; i < ControlLimits::Queue; ++i)
        TEST_ASSERT_TRUE(f->submit(wrongBoot));
}
void test_revocation_discards_complete_queuefull_message() {
    CommandStream stream(f->channel.actual);
    fillCommandQueue();
    streamOutcome(CommandStreamOutcome::QueueFull, sendDocument(stream, json(f->command())));
    TEST_ASSERT_TRUE(stream.snapshot().complete);
    f->channel.actual.suspendAuthority();
    streamOutcome(CommandStreamOutcome::Unauthorized, stream.retry(1));
    TEST_ASSERT_FALSE(stream.snapshot().complete);
    f->control.service(++f->now);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
}
void test_queuefull_retains_exact_document_and_explicit_retry_executes_once() {
    CommandStream stream(f->channel.actual);
    fillCommandQueue();
    const auto command = f->command();
    const auto document = json(command);
    streamOutcome(CommandStreamOutcome::QueueFull, sendDocument(stream, document));
    const auto received = stream.snapshot().received;
    streamOutcome(CommandStreamOutcome::QueueFull, stream.poll(1)); // No queue submit.
    streamOutcome(CommandStreamOutcome::QueueFull, stream.retry(2));
    const auto other = frameBytes(document, 0, 12, 2);
    streamOutcome(CommandStreamOutcome::Busy, stream.receive(other.data(), other.size(), 7, 1, 3));
    TEST_ASSERT_EQUAL_UINT16(received, stream.snapshot().received);
    f->control.service(++f->now);
    RoutedGameReply reply;
    for (unsigned i = 0; i < ControlLimits::Queue; ++i) {
        TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::WrongBoot),
                          static_cast<unsigned>(reply.reply.outcome));
    }
    streamOutcome(CommandStreamOutcome::Queued, stream.retry(4));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    const auto state = reply.reply.stateRevision;
    // Lost frame acknowledgment: restart WHOLE identical document, application receipts
    // deduplicate.
    streamOutcome(CommandStreamOutcome::Queued, sendDocument(stream, document, 20, 7, 1, 5));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_TRUE(reply.reply.duplicate);
    TEST_ASSERT_EQUAL_UINT32(state, f->application.execution().stateRevision);
}
void test_fragment_expiry_across_clock_wrap_and_busy_frames() {
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    const auto first = frameBytes(document, 0, 12);
    const auto other = frameBytes(document, 0, 12, 2);
    const std::uint32_t start = UINT32_MAX - 1000;
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 1, start));
    streamOutcome(CommandStreamOutcome::Busy,
                  stream.receive(other.data(), other.size(), 7, 1, start + 1999));
    streamOutcome(CommandStreamOutcome::Expired,
                  stream.receive(first.data(), first.size(), 7, 1, start + 2000));
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 1, start + 2001));
}
void test_queuefull_expiry_never_submits_after_deadline() {
    CommandStream stream(f->channel.actual);
    fillCommandQueue();
    streamOutcome(CommandStreamOutcome::QueueFull, sendDocument(stream, json(f->command())));
    f->control.service(++f->now);
    RoutedGameReply reply;
    while (f->channel.actual.takeReply(reply)) {
    }
    streamOutcome(CommandStreamOutcome::Expired, stream.retry(2000));
    TrustedGameRequest request;
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_fragmented_malformed_json_cannot_consume_game_request() {
    CommandStream stream(f->channel.actual);
    const auto malformed = replace(json(f->command()), "\"request\":1", "\"request\":1.0");
    const auto value = sendDocument(stream, malformed);
    streamOutcome(CommandStreamOutcome::DecodeRejected, value);
    TEST_ASSERT_FALSE(value.submission.decode);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    streamOutcome(CommandStreamOutcome::Queued, sendDocument(stream, json(f->command()), 1));
}
void test_maximum_document_and_variable_chunk_sizes() {
    CommandStream stream(f->channel.actual);
    auto document = json(f->command());
    document.append(maxGameCommandDocumentBytes - document.size(), ' ');
    streamOutcome(CommandStreamOutcome::Queued, sendDocument(stream, document, 120));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    auto pause = f->command(GameCommandKind::Pause);
    streamOutcome(CommandStreamOutcome::Queued, sendDocument(stream, json(pause), 1, 7, 1, 1));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_TRUE(f->application.execution().paused);
}
void test_revoke_after_fragment_observation_still_fails_admission() {
    // Revoke after the final fragment's copied authority read. The actual
    // decoder/adapter must still reject before copied queue admission.
    class LastReadIngress : public ICommandIngress {
      public:
        mutable unsigned reads = 0;
        bool readAuthority(TrustedAuthority &value) const noexcept override {
            const bool okay = f->channel.actual.readAuthority(value);
            if (++reads == 2)
                f->channel.actual.updateAuthority(authority(2, false));
            return okay;
        }
        GameSubmission submitJson(const char *bytes, std::size_t count, std::uint32_t principal,
                                  std::uint32_t epoch) noexcept override {
            return f->channel.actual.submitJson(bytes, count, principal, epoch);
        }
    } lastRead;
    CommandStream active(lastRead);
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  sendDocument(active, json(f->command()), 120));
    TrustedGameRequest request;
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
    TEST_ASSERT_EQUAL_UINT16(0, active.snapshot().received);
}
void test_frame_fixture_matches_canonical_producer() {
    const auto document = json(f->command());
    std::string expected;
    const char hex[] = "0123456789ABCDEF";
    for (std::size_t offset = 0; offset < document.size(); offset += 12) {
        const auto bytes =
            frameBytes(document, offset, std::min<std::size_t>(12, document.size() - offset));
        for (const unsigned char byte : bytes) {
            expected += hex[byte >> 4];
            expected += hex[byte & 15];
        }
        expected += '\n';
    }
    const char *path = "docs/companion/fixtures/command-frames-v1.txt";
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *out = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT_EQUAL_UINT32(expected.size(),
                                 std::fwrite(expected.data(), 1, expected.size(), out));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(out));
    }
    auto *input = std::fopen(path, "rt"); // Text display lines may use platform newlines.
    TEST_ASSERT_NOT_NULL(input);
    char buffer[2048];
    const auto count = std::fread(buffer, 1, sizeof(buffer), input);
    TEST_ASSERT_EQUAL_INT(0, std::fclose(input));
    TEST_ASSERT_EQUAL_UINT32(expected.size(), count);
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), buffer, count);
}
