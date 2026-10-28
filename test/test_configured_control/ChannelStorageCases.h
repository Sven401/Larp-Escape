#pragma once
void test_channel_fifo_wrap_full_copies_and_empty_preserve_output() {
    FreeRtosGameControlChannel channel;
    TEST_ASSERT_TRUE(channel.begin());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(channel.updateAuthority(authority(1))));
    TrustedGameRequest input, output;
    input.principal = 7;
    input.connectionEpoch = 1;
    output.command.request = 999;
    TEST_ASSERT_FALSE(channel.receive(output));
    TEST_ASSERT_EQUAL_UINT32(999, output.command.request);
    RoutedGameReply reply, received;
    reply.principal = 7;
    reply.connectionEpoch = 1;
    received.reply.request = 999;
    TEST_ASSERT_FALSE(channel.takeReply(received));
    TEST_ASSERT_EQUAL_UINT32(999, received.reply.request);
    // Partial draining advances the head before refilling across its wrap point.
    std::uint32_t next = 1, oldest = 1;
    for (unsigned round = 0; round < 12; ++round) {
        const unsigned fill = round == 0 ? ControlLimits::Queue : 2;
        for (unsigned i = 0; i < fill; ++i) {
            input.command.request = next;
            reply.reply.request = next++;
            TEST_ASSERT_TRUE(channel.submit(input));
            TEST_ASSERT_TRUE(channel.send(reply));
            input.command.request = reply.reply.request = 999;
        }
        TEST_ASSERT_FALSE(channel.submit(input));
        TEST_ASSERT_FALSE(channel.send(reply));
        TEST_ASSERT_FALSE(channel.begin()); // A repeated begin cannot erase data.
        for (unsigned i = 0; i < 2; ++i) {
            TEST_ASSERT_TRUE(channel.receive(output));
            TEST_ASSERT_TRUE(channel.takeReply(received));
            TEST_ASSERT_EQUAL_UINT32(oldest, output.command.request);
            TEST_ASSERT_EQUAL_UINT32(oldest++, received.reply.request);
        }
    }
    for (; oldest < next; ++oldest) {
        TEST_ASSERT_TRUE(channel.receive(output));
        TEST_ASSERT_TRUE(channel.takeReply(received));
        TEST_ASSERT_EQUAL_UINT32(oldest, output.command.request);
        TEST_ASSERT_EQUAL_UINT32(oldest, received.reply.request);
    }
    const auto last = output.command.request;
    TEST_ASSERT_FALSE(channel.receive(output));
    TEST_ASSERT_FALSE(channel.takeReply(received));
    TEST_ASSERT_EQUAL_UINT32(last, output.command.request);
    TEST_ASSERT_EQUAL_UINT32(last, received.reply.request);
}
void test_channel_status_empty_overwrite_copy_and_repeated_begin() {
    FreeRtosGameControlChannel channel;
    TEST_ASSERT_TRUE(channel.begin());
    ConfiguredStatus target;
    target.sample = 999;
    TEST_ASSERT_FALSE(channel.readStatus(target));
    TEST_ASSERT_EQUAL_UINT32(999, target.sample);
    channel.updateAuthority(authority(1));
    RoutedConfiguredStatus value;
    value.principal = 7;
    value.connectionEpoch = 1;
    for (unsigned i = 1; i < 8; ++i) {
        value.status.sample = i;
        TEST_ASSERT_TRUE(channel.publish(value));
    }
    value.status.sample = 999;
    TEST_ASSERT_FALSE(channel.begin());
    TEST_ASSERT_TRUE(channel.readStatus(target));
    TEST_ASSERT_EQUAL_UINT32(7, target.sample);
    TEST_ASSERT_TRUE(channel.readCurrentStatus(value));
    TEST_ASSERT_EQUAL_UINT32(7, value.status.sample);
}
void test_rtos_api_guard_counts_every_modeled_api_separately_from_wait() {
    // Verify the guard itself, then clear this deliberate violation before tearDown.
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    StaticQueue_t storage;
    std::uint8_t bytes[sizeof(unsigned)] = {};
    StaticSemaphore_t mutex;
    unsigned value = 7;
    const auto before = host_rtos::callsInsideCritical();
    portENTER_CRITICAL(&lock);
    auto queue = xQueueCreateStatic(1, sizeof(value), bytes, &storage);
    xQueueSendToBack(queue, &value, 0);
    xQueuePeek(queue, &value, 0);
    xQueueReceive(queue, &value, 0);
    xQueueOverwrite(queue, &value);
    auto handle = xSemaphoreCreateMutexStatic(&mutex);
    xSemaphoreTake(handle, 0);
    xSemaphoreGive(handle);
    xTaskGetCurrentTaskHandle();
    portEXIT_CRITICAL(&lock);
    TEST_ASSERT_EQUAL_UINT32(9, host_rtos::callsInsideCritical() - before);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    host_rtos::callsInsideCritical() = before;
}
