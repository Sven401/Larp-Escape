#pragma once
#include "../../lib/ClosedBleStackOwner/ClosedBleAdvertiseGate.h"
#include "../../lib/ClosedBleStackOwner/ClosedBleSecurityGate.h"
#include "../../lib/ClosedBleStackOwner/ProductGattsOwnerTelemetry.h"
#include "PeerRegistryActorCases.h"

void test_ble_discovery_gate_requires_both_original_closed_acks_and_never_retries_failure() {
    using Phase = ClosedBleAdvertiseGate::Phase;
    for (unsigned missing = 0; missing < 3; ++missing) {
        ClosedBleAdvertiseGate gate;
        TEST_ASSERT_FALSE(gate.configure(missing != 0, missing != 1, missing != 2));
        TEST_ASSERT_EQUAL(static_cast<int>(Phase::Failed), static_cast<int>(gate.phase()));
        TEST_ASSERT_FALSE(gate.configure(true, true, true));
    }
    ClosedBleAdvertiseGate gate;
    TEST_ASSERT_TRUE(gate.configure(true, true, true));
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::DataPending), static_cast<int>(gate.phase()));
    TEST_ASSERT_FALSE(gate.configure(true, true, true));
    TEST_ASSERT_TRUE(gate.dataComplete(true));
    TEST_ASSERT_TRUE(gate.startComplete(true));
    TEST_ASSERT_TRUE(gate.connected());
    TEST_ASSERT_FALSE(gate.disconnected(true, false, true));
    TEST_ASSERT_EQUAL(static_cast<int>(Phase::Failed), static_cast<int>(gate.phase()));
    TEST_ASSERT_FALSE(gate.startComplete(true));
    ClosedBleAdvertiseGate sdkFailure;
    TEST_ASSERT_TRUE(sdkFailure.configure(true, true, true));
    TEST_ASSERT_FALSE(sdkFailure.dataComplete(false));
    TEST_ASSERT_FALSE(sdkFailure.startComplete(true));
    ClosedBleAdvertiseGate reconnect;
    TEST_ASSERT_TRUE(reconnect.configure(true, true, true));
    TEST_ASSERT_TRUE(reconnect.dataComplete(true));
    TEST_ASSERT_TRUE(reconnect.startComplete(true));
    ClosedBleAdvertiseGate missed;
    TEST_ASSERT_TRUE(missed.configure(true, true, true));
    TEST_ASSERT_TRUE(missed.dataComplete(true));
    TEST_ASSERT_TRUE(missed.startComplete(true));
    TEST_ASSERT_TRUE(missed.missedOriginalBirth(true, true, true));
    TEST_ASSERT_TRUE(missed.startComplete(true));
    TEST_ASSERT_TRUE(reconnect.connected());
    TEST_ASSERT_TRUE(reconnect.disconnected(true, true, true));
    TEST_ASSERT_TRUE(reconnect.startComplete(true));
}

void test_ble_original_link_publication_closes_on_disconnect_and_recycled_birth() {
    RegistryActorFixture fixture;
    fixture.ready();
    PeerTransportBinding original, observed;
    std::uint32_t lifecycle = 0;
    TEST_ASSERT_FALSE(fixture.source.readOriginalTransport(observed));
    TEST_ASSERT_TRUE(fixture.source.readOriginalLifecycle(lifecycle));
    const auto before = lifecycle;
    const auto first = fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(original));
    TEST_ASSERT_TRUE(original.valid());
    TEST_ASSERT_TRUE(original.connection == first);
    TEST_ASSERT_EQUAL_MEMORY(fixture.address, original.address.data(), 6);
    TEST_ASSERT_TRUE(fixture.source.readOriginalLifecycle(lifecycle));
    TEST_ASSERT_EQUAL(before + 1, lifecycle);
    fixture.close();
    fixture.actor();
    observed = original;
    TEST_ASSERT_FALSE(fixture.source.readOriginalTransport(observed));
    TEST_ASSERT_TRUE(observed == original); // Failed read preserves output.
    TEST_ASSERT_TRUE(fixture.source.readOriginalLifecycle(lifecycle));
    TEST_ASSERT_EQUAL(before + 2, lifecycle);
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(observed));
    TEST_ASSERT_FALSE(observed == original); // Same packed handle, new birth.
    TEST_ASSERT_EQUAL(original.lifecycle + 2, observed.lifecycle);
}

void test_ble_pairing_surface_has_only_mitm_required_closed_writes() {
    host_service_sdk::reset();
    SdkGattsServiceDriver driver;
    TEST_ASSERT_TRUE(driver.create(1));
    TEST_ASSERT_TRUE(driver.addCommand(0x20));
    TEST_ASSERT_TRUE(driver.addProfile(0x20));
    TEST_ASSERT_EQUAL(3, host_service_sdk::calls.size());
    TEST_ASSERT_EQUAL(GattsServiceDefinition::Handles, host_service_sdk::calls[0].handles);
    TEST_ASSERT_EQUAL(ESP_GATT_PERM_WRITE_ENC_MITM,
                      host_service_sdk::calls[1].permission);
    TEST_ASSERT_EQUAL(ESP_GATT_PERM_WRITE_ENC_MITM,
                      host_service_sdk::calls[2].permission);
    TEST_ASSERT_TRUE(host_service_sdk::calls[1].uuid == GattsServiceDefinition::command());
    TEST_ASSERT_TRUE(host_service_sdk::calls[2].uuid == GattsServiceDefinition::profile());
    RegistryActorFixture fixture;
    fixture.ready();
    (void)fixture.open();
    TrustedAuthority authority;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(authority));
    TEST_ASSERT_EQUAL(0, authority.principal);
    TEST_ASSERT_FALSE(authority.gameMaster);
    TEST_ASSERT_TRUE(fixture.source.closedChannelState(testIdentity()));
    fixture.close();
    TEST_ASSERT_TRUE(fixture.source.closedChannelState(testIdentity()));
    TrustedAuthority foreign;
    TEST_ASSERT_EQUAL(static_cast<int>(AuthorityUpdate::Published),
                      static_cast<int>(f->channel.actual.advanceAuthority(17, true, foreign)));
    TEST_ASSERT_FALSE(fixture.source.closedChannelState(testIdentity()));
}

void test_ble_security_request_requires_current_original_birth_and_address() {
    RegistryActorFixture fixture;
    fixture.ready();
    const auto firstConnection = fixture.open();
    fixture.actor();
    PeerTransportBinding first, current;
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(first));
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(current));
    TEST_ASSERT_TRUE(first.connection == firstConnection);
    TEST_ASSERT_TRUE(ClosedBleSecurityGate::allow(true, true, true,
                                                  first, current, first.address));
    auto wrongAddress = first.address;
    wrongAddress[5] ^= 1;
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, true,
                                                   first, current, wrongAddress));
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(false, true, true,
                                                   first, current, first.address));
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, false, true,
                                                   first, current, first.address));
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, false,
                                                   first, current, first.address));
    fixture.close();
    fixture.actor();
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(current));
    TEST_ASSERT_FALSE(first == current);
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, true,
                                                   first, current, first.address));
    PeerTransportBinding absent;
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, true,
                                                   absent, current, first.address));
}

void test_ble_security_telemetry_names_closed_unpublished_stale_and_peer_mismatch() {
    using T = ProductGattsOwnerTelemetry;
    RegistryActorFixture fixture;
    fixture.ready();
    PeerTransportBinding absent, first, current;
    std::array<std::uint8_t, 6> address = {};
    for (unsigned i = 0; i < address.size(); ++i)
        address[i] = fixture.address[i];
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::NoOriginalLink),
                      static_cast<int>(T::classify(true, true, true, false, false,
                                                   absent, absent, address)));
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, true,
                                                   absent, absent, address));
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(first));
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(current));
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::Allowed),
                      static_cast<int>(T::classify(true, true, true, true, true,
                                                   first, current, address)));
    TEST_ASSERT_TRUE(ClosedBleSecurityGate::allow(true, true, true,
                                                  first, current, address));
    auto wrongAddress = address;
    wrongAddress[5] ^= 1;
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::PeerMismatch),
                      static_cast<int>(T::classify(true, true, true, true, true,
                                                   first, current, wrongAddress)));
    fixture.close();
    fixture.actor();
    (void)fixture.open();
    fixture.actor();
    TEST_ASSERT_TRUE(fixture.source.readOriginalTransport(current));
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::RecycledLink),
                      static_cast<int>(T::classify(true, true, true, true, true,
                                                   first, current, address)));
    TEST_ASSERT_FALSE(ClosedBleSecurityGate::allow(true, true, true,
                                                   first, current, address));
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::ServiceUnavailable),
                      static_cast<int>(T::classify(false, true, true, true, true,
                                                   current, current, address)));
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::RegistryUnavailable),
                      static_cast<int>(T::classify(true, false, true, true, true,
                                                   current, current, address)));
    TEST_ASSERT_EQUAL(static_cast<int>(T::Reason::ChannelNotClosed),
                      static_cast<int>(T::classify(true, true, false, true, true,
                                                   current, current, address)));
    TrustedAuthority authority;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(authority));
    TEST_ASSERT_EQUAL(0, authority.principal);
    TEST_ASSERT_FALSE(authority.gameMaster);
}

void test_ble_late_start_ack_requires_new_stop_ack_after_failure() {
    ClosedBleRadioFence fence(false, false);
    // Stop ACK from an early attempt cannot certify a later successful start.
    fence.earlyStopPosted(true);
    fence.startComplete(true);
    TEST_ASSERT_FALSE(fence.needsStopAfterStart());
    fence.stopComplete(true);
    TEST_ASSERT_FALSE(fence.closed());
    TEST_ASSERT_TRUE(fence.needsStopAfterStart());
    TEST_ASSERT_FALSE(fence.closed());
    fence.stopPosted(true);
    TEST_ASSERT_FALSE(fence.closed());
    fence.stopComplete(false);
    TEST_ASSERT_TRUE(fence.needsStopAfterStart());
    fence.stopPosted(true);
    fence.stopComplete(true);
    TEST_ASSERT_TRUE(fence.closed());
    ClosedBleRadioFence rejected(false, false);
    rejected.startComplete(false);
    TEST_ASSERT_TRUE(rejected.closed());
}
