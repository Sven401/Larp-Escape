#pragma once
void test_current_status_filters_reconnect_until_new_producer_snapshot() {
    RoutedConfiguredStatus original;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(original));
    TEST_ASSERT_EQUAL_UINT32(7, original.principal);
    TEST_ASSERT_EQUAL_UINT32(1, original.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(1, original.status.controlLease);
    f->channel.actual.updateAuthority(authority(3));
    RoutedConfiguredStatus target;
    target.principal = 99;
    target.status.sample = 999;
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_EQUAL_UINT32(99, target.principal);
    TEST_ASSERT_EQUAL_UINT32(999, target.status.sample);
    ConfiguredStatus diagnostic;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1, diagnostic.controlLease); // Historical raw diagnostic only.
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_EQUAL_UINT32(3, target.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(2, target.status.controlLease);
    TEST_ASSERT_EQUAL_UINT32(1, original.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(1, original.status.controlLease);
    char encoded[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeConfiguredStatus(target.status, encoded,
                                                                   sizeof(encoded), written)));
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_NULL(std::strstr(encoded, "principal"));
    TEST_ASSERT_NULL(std::strstr(encoded, "connectionEpoch"));
}
void test_delayed_old_status_cannot_be_relabelled_to_current_epoch() {
    RoutedConfiguredStatus old;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(old));
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    RoutedConfiguredStatus current;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(current));
    TEST_ASSERT_TRUE(f->channel.actual.publish(old)); // Trusted delayed producer copy.
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(current));
    TEST_ASSERT_EQUAL_UINT32(3, current.connectionEpoch); // False preserves destination.
    TEST_ASSERT_TRUE(f->channel.actual.publish(current));
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(current));
    auto wrong = current;
    wrong.principal = 8;
    TEST_ASSERT_TRUE(f->channel.actual.publish(wrong));
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(current));
    TEST_ASSERT_EQUAL_UINT32(7, current.principal);
}
void test_status_selection_revoke_observer_suspend_preserves_game() {
    f->execute(f->command());
    TEST_ASSERT_TRUE(f->application.gameRunning());
    RoutedConfiguredStatus target;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(target));
    host_rtos::beforeEnter = [] { f->channel.actual.updateAuthority(authority(2, false)); };
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_EQUAL_UINT32(1, target.connectionEpoch);
    f->control.service(++f->now);
    ConfiguredStatus diagnostic;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0, diagnostic.controlLease);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    auto observer = authority(3);
    observer.gameMaster = false;
    observer.principal = 7;
    f->channel.actual.updateAuthority(observer);
    f->control.service(++f->now);
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(target));
    f->channel.actual.updateAuthority(authority(4));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_EQUAL_UINT32(4, target.connectionEpoch);
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_status_producer_observation_race_keeps_original_epoch() {
    f->execute(f->command());
    const auto revision = f->application.execution().stateRevision;
    f->channel.reads = 0;
    f->channel.revokeAfter = 1; // Channel gives epoch1 then actual authority changes.
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus target;
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(target));
    ConfiguredStatus diagnostic;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1, diagnostic.controlLease);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    f->channel.revokeAfter = 0;
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(target));
    TEST_ASSERT_EQUAL_UINT32(3, target.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(2, target.status.controlLease);
    TEST_ASSERT_EQUAL_UINT32(revision, target.status.stateRevision);
    TEST_ASSERT_TRUE(target.status.running);
}
