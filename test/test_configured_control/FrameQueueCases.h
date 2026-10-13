#pragma once
// Actual statically allocated adapter compiled against the copying RTOS model.
void copyOutcome(FrameCopyOutcome expected, FrameCopyOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void test_frame_queue_validates_outer_write_and_preserves_empty_destination() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    auto bytes = frameBytes("{}", 0, 2);
    copyOutcome(FrameCopyOutcome::Uninitialized,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 0));
    CopiedCommandFrame target;
    target.principal = 99;
    target.bytes[0] = '#';
    TEST_ASSERT_FALSE(queue.take(target));
    TEST_ASSERT_EQUAL_UINT32(99, target.principal);
    TEST_ASSERT_EQUAL_CHAR('#', target.bytes[0]);
    TEST_ASSERT_TRUE(queue.begin());
    copyOutcome(FrameCopyOutcome::InvalidWrite,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 0, true));
    copyOutcome(FrameCopyOutcome::InvalidWrite,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 0, false, 1));
    copyOutcome(FrameCopyOutcome::InvalidWrite, queue.copyWrite(nullptr, 10, 7, 1, 0));
    const std::string oversized(129, 'x');
    for (const std::size_t count : {0U, 8U, 129U})
        copyOutcome(FrameCopyOutcome::InvalidWrite,
                    queue.copyWrite(oversized.data(), count, 7, 1, 0));
    bytes[1] = 2;
    copyOutcome(FrameCopyOutcome::UnsupportedFrame,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 0));
    bytes[1] = 1;
    for (const auto principal : {0U, 8U})
        copyOutcome(FrameCopyOutcome::Unauthorized,
                    queue.copyWrite(bytes.data(), bytes.size(), principal, 1, 0));
    for (const auto epoch : {0U, 2U})
        copyOutcome(FrameCopyOutcome::Unauthorized,
                    queue.copyWrite(bytes.data(), bytes.size(), 7, epoch, 0));
    TEST_ASSERT_FALSE(queue.take(target));
    TEST_ASSERT_EQUAL_UINT32(99, target.principal);
    TEST_ASSERT_EQUAL_CHAR('#', target.bytes[0]);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_frame_queue_owns_bytes_and_trusted_metadata() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    auto bytes = frameBytes(std::string(120, 'x'), 0, 120, 0xabcd);
    const auto original = bytes;
    copyOutcome(FrameCopyOutcome::Copied,
                queue.copyWrite(bytes.data(), bytes.size(), 7, 1, UINT32_MAX));
    bytes.assign(bytes.size(), '\0');
    CopiedCommandFrame received;
    TEST_ASSERT_TRUE(queue.take(received));
    TEST_ASSERT_EQUAL_UINT32(7, received.principal);
    TEST_ASSERT_EQUAL_UINT32(1, received.epoch);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, received.receivedAt);
    TEST_ASSERT_EQUAL_UINT16(128, received.count);
    TEST_ASSERT_EQUAL_MEMORY(original.data(), received.bytes.data(), original.size());
    TEST_ASSERT_FALSE(queue.take(received));
    TEST_ASSERT_EQUAL_UINT16(128, received.count);
    TEST_ASSERT_FALSE(f->application.gameRunning()); // Only fixed bytes copied.
}
void test_frame_queue_full_does_not_report_copied_or_consume_command() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto bytes = frameBytes("{}", 0, 2);
    for (unsigned i = 0; i < CommandFrameQueueLimits::Depth; ++i)
        copyOutcome(FrameCopyOutcome::Copied, queue.copyWrite(bytes.data(), bytes.size(), 7, 1, i));
    TEST_ASSERT_TRUE(queue.begin()); // Idempotent begin must not erase pending copies.
    copyOutcome(FrameCopyOutcome::QueueFull, queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 99));
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
    TrustedGameRequest request;
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
    CopiedCommandFrame received;
    for (unsigned i = 0; i < CommandFrameQueueLimits::Depth; ++i) {
        TEST_ASSERT_TRUE(queue.take(received));
        TEST_ASSERT_EQUAL_UINT32(i, received.receivedAt);
    }
    TEST_ASSERT_FALSE(queue.take(received));
    TEST_ASSERT_EQUAL_UINT32(3, received.receivedAt);
}
void test_queued_frames_reach_actual_stored_game_and_current_reply() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    const auto document = json(f->command());
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        auto bytes = frameBytes(document, offset, count);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 100));
        bytes.assign(bytes.size(), '\0');
        TEST_ASSERT_FALSE(f->application.gameRunning());
        const auto dispatched = dispatchCommandFrame(queue, stream, 100);
        TEST_ASSERT_TRUE(dispatched.received);
        streamOutcome(offset + count == document.size() ? CommandStreamOutcome::Queued
                                                        : CommandStreamOutcome::Receiving,
                      dispatched.stream);
        offset += count;
    }
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
    TEST_ASSERT_FALSE(f->application.gameRunning()); // Queued is still not execution.
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_EQUAL_UINT32(7, reply.principal);
    TEST_ASSERT_EQUAL_UINT32(1, reply.connectionEpoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_FALSE(dispatchCommandFrame(queue, stream, 101).received);
}
void test_frame_queue_revoke_and_reconnect_reject_buffered_old_frames() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    const auto old = json(f->command());
    for (unsigned i = 0; i < 2; ++i) {
        const auto bytes = frameBytes(old, i * 12, 12);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 10));
    }
    f->channel.actual.updateAuthority(authority(2, false));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  dispatchCommandFrame(queue, stream, 11).stream);
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    const auto current = json(f->command());
    const auto first = frameBytes(current, 0, 12);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 3, 12));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  dispatchCommandFrame(queue, stream, 13).stream);
    TEST_ASSERT_EQUAL_UINT16(12, stream.snapshot().received); // Old cannot erase/join new.
    for (std::size_t offset = 12; offset < current.size();) {
        const auto count = std::min<std::size_t>(120, current.size() - offset);
        const auto bytes = frameBytes(current, offset, count);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(bytes.data(), bytes.size(), 7, 3, 14));
        const auto delivered = dispatchCommandFrame(queue, stream, 14);
        streamOutcome(offset + count == current.size() ? CommandStreamOutcome::Queued
                                                       : CommandStreamOutcome::Receiving,
                      delivered.stream);
        offset += count;
    }
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(reply));
    TEST_ASSERT_EQUAL_UINT32(3, reply.connectionEpoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
}
void test_frame_queue_wait_does_not_extend_absolute_deadline() {
    const auto document = json(f->command());
    TEST_ASSERT_TRUE(document.size() > 120 && document.size() <= 240);
    const auto first = frameBytes(document, 0, 120);
    const auto last = frameBytes(document, 120, document.size() - 120);
    for (const std::uint32_t base : {100U, UINT32_MAX - 1000U}) {
        FreeRtosCommandFrameQueue queue(f->channel.actual);
        TEST_ASSERT_TRUE(queue.begin());
        CommandStream stream(f->channel.actual);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(first.data(), first.size(), 7, 1, base));
        streamOutcome(CommandStreamOutcome::Receiving,
                      dispatchCommandFrame(queue, stream, base + 1900U).stream);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(last.data(), last.size(), 7, 1, base + 1999U));
        streamOutcome(CommandStreamOutcome::Expired,
                      dispatchCommandFrame(queue, stream, base + 2000U).stream);
        TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
        TrustedGameRequest request;
        TEST_ASSERT_FALSE(f->channel.actual.receive(request));
        // Expired first frame and future clock value also cannot begin/submit.
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(first.data(), first.size(), 7, 1, base));
        streamOutcome(CommandStreamOutcome::Expired,
                      dispatchCommandFrame(queue, stream, base + 2000U).stream);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(first.data(), first.size(), 7, 1, base + 10U));
        streamOutcome(CommandStreamOutcome::Expired,
                      dispatchCommandFrame(queue, stream, base + 9U).stream);
        TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
        // One millisecond before deadline remains usable, including wrap.
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(first.data(), first.size(), 7, 1, base));
        streamOutcome(CommandStreamOutcome::Receiving,
                      dispatchCommandFrame(queue, stream, base + 1999U).stream);
        copyOutcome(FrameCopyOutcome::Copied,
                    queue.copyWrite(last.data(), last.size(), 7, 1, base + 1999U));
        streamOutcome(CommandStreamOutcome::Queued,
                      dispatchCommandFrame(queue, stream, base + 1999U).stream);
        TEST_ASSERT_TRUE(f->channel.actual.receive(request));
        TEST_ASSERT_EQUAL_UINT32(1, request.command.request);
    }
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_empty_frame_queue_poll_revokes_without_retry_or_game_tick() {
    FreeRtosCommandFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    for (unsigned i = 0; i < ControlLimits::Queue; ++i)
        TEST_ASSERT_TRUE(f->submit(f->command()));
    streamOutcome(CommandStreamOutcome::QueueFull,
                  sendDocument(stream, json(f->command()), 120, 7, 1, 100));
    const auto revision = f->application.execution().stateRevision;
    auto result = dispatchCommandFrame(queue, stream, 101);
    TEST_ASSERT_FALSE(result.received);
    streamOutcome(CommandStreamOutcome::QueueFull, result.stream);
    TrustedGameRequest request;
    TEST_ASSERT_TRUE(f->channel.actual.receive(request));
    result = dispatchCommandFrame(queue, stream, 102);
    streamOutcome(CommandStreamOutcome::QueueFull, result.stream);
    unsigned remaining = 0;
    while (f->channel.actual.receive(request))
        ++remaining;
    TEST_ASSERT_EQUAL_UINT32(3, remaining); // Empty FIFO service never auto-retries.
    streamOutcome(CommandStreamOutcome::Queued, stream.retry(103));
    TEST_ASSERT_TRUE(f->channel.actual.receive(request));
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
    const auto first = frameBytes("{}", 0, 1);
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 1, 200));
    f->channel.actual.updateAuthority(authority(2, false));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  dispatchCommandFrame(queue, stream, 201).stream);
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    f->channel.actual.updateAuthority(authority(3));
    streamOutcome(CommandStreamOutcome::Receiving,
                  stream.receive(first.data(), first.size(), 7, 3, 3000));
    streamOutcome(CommandStreamOutcome::Expired, dispatchCommandFrame(queue, stream, 5000).stream);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.execution().stateRevision);
}
void test_frame_copy_after_authority_observation_is_rechecked_by_stream() {
    FreeRtosCommandFrameQueue queue(f->channel);
    TEST_ASSERT_TRUE(queue.begin());
    CommandStream stream(f->channel.actual);
    const auto bytes = frameBytes(json(f->command()), 0, 12);
    f->channel.revokeAfter = f->channel.reads + 1;
    copyOutcome(FrameCopyOutcome::Copied, queue.copyWrite(bytes.data(), bytes.size(), 7, 1, 0));
    streamOutcome(CommandStreamOutcome::Unauthorized,
                  dispatchCommandFrame(queue, stream, 0).stream);
    TEST_ASSERT_EQUAL_UINT16(0, stream.snapshot().received);
    TrustedGameRequest request;
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
    f->control.service(++f->now);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
