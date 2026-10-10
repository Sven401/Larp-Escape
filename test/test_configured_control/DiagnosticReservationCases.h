#pragma once
#include "DeviceRuntime/Runtime.h"

class DiagnosticGateOwners : public Owners {
  public:
    bool reserving = false, dropReplies = false, barrierFailure = false;
    ControlOutcome result = ControlOutcome::Applied;
    std::vector<ControlTicket> parks;
    Fifo<ControlReply, 8> injected;
    void (*onPark)(void *) = nullptr;
    void *hookContext = nullptr;
    bool sendControl(const Control &value) noexcept override {
        if (reserving) {
            TEST_ASSERT_TRUE(value.ticket.kind == ControlKind::Park);
            parks.push_back(value.ticket);
            if (onPark)
                onPark(hookContext);
        }
        return Owners::sendControl(value);
    }
    bool receiveControlReply(ControlReply &value) noexcept override {
        if (injected.pop(value))
            return true;
        if (reserving && dropReplies)
            return false;
        if (!Owners::receiveControlReply(value))
            return false;
        if (reserving)
            value.outcome = result;
        return true;
    }
    bool quiescent() const noexcept override {
        return !barrierFailure && Owners::quiescent();
    }
};
struct DiagnosticGateFixture {
    DiagnosticGateOwners owners;
    larp::devices::Runtime runtime{owners, owners};
    std::unique_ptr<Profile> profile{new Profile};
    std::uint32_t now = 0;
    DiagnosticGateFixture() {
        TEST_ASSERT_TRUE(makeStandardProfile(*profile));
        TEST_ASSERT_TRUE(runtime.activate(*profile, 0, now));
        for (unsigned i = 0; i < 200 && !runtime.activation().hardwareReady; ++i)
            pump();
        TEST_ASSERT_TRUE(runtime.activation().hardwareReady);
    }
    void pump() {
        runtime.service(++now);
        owners.pump(now);
    }
    void reserve() {
        owners.reserving = true;
        TEST_ASSERT_TRUE(runtime.reserveDiagnostics(1, now) ==
                         DiagnosticReservationOutcome::Reserved);
    }
    void closed() {
        TEST_ASSERT_TRUE(runtime.busy());
        TEST_ASSERT_FALSE(runtime.activation().hardwareReady);
        TEST_ASSERT_TRUE(runtime.startGame(1, now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.tickGame(now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.stopGame(0, now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.pauseGame(0, now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.resumeGame(0, now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.changeState({}, 0, now).outcome == larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.takeTransition({}, 0, now).outcome ==
                         larp::execution::Outcome::Busy);
        TEST_ASSERT_TRUE(runtime.activate(*profile, 1, now).outcome == ActivationOutcome::Busy);
        TEST_ASSERT_TRUE(runtime.reserveDiagnostics(1, now) == DiagnosticReservationOutcome::Busy);
        const auto game = runtime.game();
        for (unsigned i = 0; i < game.deviceCount; ++i)
            TEST_ASSERT_FALSE(game.devices[i].available);
    }
};
void test_diagnostic_reservation_closes_before_parking_and_stays_closed_after_all_acks() {
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    const auto generation = gate->runtime.activation().generation;
    gate->reserve();
    gate->closed();
    TEST_ASSERT_TRUE(gate->owners.parks.empty());
    for (unsigned i = 0; i < 30; ++i)
        gate->pump();
    const auto snapshot = gate->runtime.diagnosticReservation();
    TEST_ASSERT_TRUE(snapshot.phase == DiagnosticReservationPhase::ParkedClosed);
    TEST_ASSERT_EQUAL_UINT32(ChannelLimits::Owners, snapshot.parkedOwners);
    TEST_ASSERT_EQUAL_UINT32(ChannelLimits::Owners, gate->owners.parks.size());
    std::uint32_t sequence = 0;
    for (unsigned i = 0; i < gate->owners.parks.size(); ++i) {
        const auto &ticket = gate->owners.parks[i];
        TEST_ASSERT_EQUAL_UINT32(generation, ticket.generation);
        TEST_ASSERT_EQUAL_UINT32(i, static_cast<unsigned>(ticket.owner));
        TEST_ASSERT_TRUE(ticket.sequence > sequence);
        sequence = ticket.sequence;
    }
    gate->closed();
}
void test_diagnostic_reservation_rejects_unready_stale_running_and_paused_games() {
    DiagnosticGateOwners owners;
    larp::devices::Runtime empty(owners, owners);
    TEST_ASSERT_TRUE(empty.reserveDiagnostics(0, 0) == DiagnosticReservationOutcome::NotReady);
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    TEST_ASSERT_TRUE(gate->runtime.reserveDiagnostics(2, gate->now) ==
                     DiagnosticReservationOutcome::StaleRevision);
    TEST_ASSERT_TRUE(gate->runtime.startGame(1, gate->now));
    TEST_ASSERT_TRUE(gate->runtime.reserveDiagnostics(1, gate->now) ==
                     DiagnosticReservationOutcome::GameRunning);
    TEST_ASSERT_TRUE(gate->runtime.pauseGame(gate->runtime.execution().stateRevision, gate->now));
    TEST_ASSERT_TRUE(gate->runtime.reserveDiagnostics(1, gate->now) ==
                     DiagnosticReservationOutcome::GameRunning);
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().phase ==
                     DiagnosticReservationPhase::Idle);
}
void diagnosticGateReentry(void *context) {
    auto &gate = *static_cast<DiagnosticGateFixture *>(context);
    gate.closed();
    gate.runtime.service(gate.now); // Reentry cannot send another Park.
}
void test_diagnostic_reservation_reentry_during_owner_post_cannot_start_or_reconfigure() {
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    gate->reserve();
    gate->owners.onPark = diagnosticGateReentry;
    gate->owners.hookContext = gate.get();
    for (unsigned i = 0; i < 30; ++i)
        gate->pump();
    TEST_ASSERT_EQUAL_UINT32(ChannelLimits::Owners, gate->owners.parks.size());
    gate->closed();
}
void test_diagnostic_reservation_stale_replies_and_deadline_never_release_gate() {
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    gate->reserve();
    gate->owners.dropReplies = true;
    gate->pump();
    ControlReply stale;
    stale.ticket = gate->runtime.diagnosticReservation().operation;
    stale.outcome = ControlOutcome::Applied;
    stale.completedMilliseconds = gate->now;
    ++stale.ticket.generation;
    TEST_ASSERT_TRUE(gate->owners.injected.push(stale));
    --stale.ticket.generation;
    ++stale.ticket.sequence;
    TEST_ASSERT_TRUE(gate->owners.injected.push(stale));
    gate->pump();
    TEST_ASSERT_EQUAL_UINT32(2, gate->runtime.diagnosticReservation().staleReplies);
    TEST_ASSERT_EQUAL_UINT32(0, gate->runtime.diagnosticReservation().parkedOwners);
    gate->now += ActivationLimits::OperationMilliseconds;
    gate->pump();
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().phase ==
                     DiagnosticReservationPhase::Fault);
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().error ==
                     DiagnosticReservationError::Timeout);
    gate->owners.dropReplies = false;
    for (unsigned i = 0; i < 10; ++i)
        gate->pump();
    gate->closed();
    TEST_ASSERT_EQUAL_UINT32(1, gate->owners.parks.size());
}
void test_diagnostic_reservation_failed_owners_and_false_barrier_stay_fenced() {
    for (auto result : {ControlOutcome::Rejected, ControlOutcome::Failed}) {
        std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
        gate->reserve();
        gate->owners.result = result;
        for (unsigned i = 0; i < 10; ++i)
            gate->pump();
        TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().phase ==
                         DiagnosticReservationPhase::Fault);
        TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().error ==
                         (result == ControlOutcome::Rejected
                              ? DiagnosticReservationError::OwnerRejected
                              : DiagnosticReservationError::OwnerFailed));
        gate->closed();
    }
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    gate->reserve();
    gate->owners.barrierFailure = true;
    for (unsigned i = 0; i < 30; ++i)
        gate->pump();
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().error ==
                     DiagnosticReservationError::BarrierFailure);
    gate->owners.barrierFailure = false;
    gate->pump();
    gate->closed();
}
void test_diagnostic_reservation_deadlines_work_across_clock_wrap() {
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    gate->now = UINT32_MAX - 2;
    gate->reserve();
    for (unsigned i = 0; i < 30; ++i)
        gate->pump();
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().phase ==
                     DiagnosticReservationPhase::ParkedClosed);
    gate->closed();
}
void test_diagnostic_reservation_after_stop_drains_workers_without_restarting_game() {
    std::unique_ptr<DiagnosticGateFixture> gate(new DiagnosticGateFixture);
    TEST_ASSERT_TRUE(gate->runtime.startGame(1, gate->now));
    TEST_ASSERT_TRUE(gate->runtime.stopGame(gate->runtime.execution().stateRevision, gate->now));
    TEST_ASSERT_FALSE(gate->owners.quiescent()); // Worker execution is distinct from game state.
    gate->reserve();
    for (unsigned i = 0; i < 30; ++i)
        gate->pump();
    TEST_ASSERT_TRUE(gate->owners.quiescent());
    TEST_ASSERT_FALSE(gate->runtime.gameRunning());
    TEST_ASSERT_TRUE(gate->runtime.diagnosticReservation().phase ==
                     DiagnosticReservationPhase::ParkedClosed);
    gate->closed();
}
