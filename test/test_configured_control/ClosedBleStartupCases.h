#pragma once

#include "../../lib/ClosedBleStackOwner/ClosedBleStartupAttempt.h"

struct BleStartupFakePort {
    unsigned failingCall = 0, calls = 0;
    std::int32_t failure = 0x103;
    std::uint8_t controller = 0, bluedroid = 0;
    std::uint8_t controllerStatus() noexcept { return controller; }
    std::uint8_t bluedroidStatus() noexcept { return bluedroid; }
    std::uint32_t internalFree() noexcept { return 40000U - calls * 100U; }
    std::uint32_t internalLargest() noexcept { return 20000U - calls * 100U; }
    std::int32_t controllerInit() noexcept {
        ++calls;
        if (calls == failingCall) return failure;
        controller = 1;
        return 0;
    }
    std::int32_t controllerEnable() noexcept {
        ++calls;
        if (calls == failingCall) return failure;
        controller = 2;
        return 0;
    }
    std::int32_t bluedroidInit() noexcept {
        ++calls;
        if (calls == failingCall) return failure;
        bluedroid = 1;
        return 0;
    }
    std::int32_t bluedroidEnable() noexcept {
        ++calls;
        if (calls == failingCall) return failure;
        bluedroid = 2;
        return 0;
    }
};

void test_ble_stack_start_reports_exact_first_sdk_failure_and_never_retries() {
    using State = ClosedBleStartupAttempt::State;
    const State expected[] = {State::ControllerInitFailed, State::ControllerEnableFailed,
                              State::BluedroidInitFailed, State::BluedroidEnableFailed};
    for (unsigned failed = 1; failed <= 4; ++failed) {
        ClosedBleStartupAttempt attempt;
        BleStartupFakePort port;
        port.failingCall = failed;
        TEST_ASSERT_EQUAL(static_cast<int>(expected[failed - 1]),
                          static_cast<int>(attempt.begin(port)));
        const auto report = attempt.report();
        TEST_ASSERT_EQUAL_INT32(port.failure, report.sdkError);
        TEST_ASSERT_EQUAL_UINT8(0, report.controllerBefore);
        TEST_ASSERT_EQUAL_UINT8(0, report.bluedroidBefore);
        TEST_ASSERT_EQUAL_UINT8(failed >= 2 ? (failed == 2 ? 1 : 2) : 0,
                                report.controllerAfter);
        TEST_ASSERT_EQUAL_UINT8(failed == 4 ? 1 : 0, report.bluedroidAfter);
        TEST_ASSERT_EQUAL_UINT32(40000U, report.internalFreeBefore);
        TEST_ASSERT_EQUAL_UINT32(40000U - failed * 100U, report.internalFreeAfter);
        port.failingCall = 0;
        TEST_ASSERT_EQUAL(static_cast<int>(expected[failed - 1]),
                          static_cast<int>(attempt.begin(port)));
        TEST_ASSERT_EQUAL_UINT(failed, port.calls);
        TEST_ASSERT_EQUAL_INT32(report.sdkError, attempt.report().sdkError);
    }
}

void test_ble_stack_start_success_stays_closed_ready_without_second_sdk_call() {
    ClosedBleStartupAttempt attempt;
    BleStartupFakePort port;
    TEST_ASSERT_EQUAL(static_cast<int>(ClosedBleStartupAttempt::State::ReadyClosed),
                      static_cast<int>(attempt.begin(port)));
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().controllerAfter);
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().bluedroidAfter);
    TEST_ASSERT_EQUAL_INT32(0, attempt.report().sdkError);
    TEST_ASSERT_EQUAL(static_cast<int>(ClosedBleStartupAttempt::State::ReadyClosed),
                      static_cast<int>(attempt.begin(port)));
    TEST_ASSERT_EQUAL_UINT(4, port.calls);
}

void test_ble_stack_preexisting_controller_state_is_reported_without_adoption() {
    ClosedBleStartupAttempt attempt;
    BleStartupFakePort port;
    port.controller = 2;
    port.bluedroid = 2;
    port.failingCall = 1;
    port.failure = 0x103; // ESP_ERR_INVALID_STATE in the pinned ESP-IDF.
    TEST_ASSERT_EQUAL(static_cast<int>(ClosedBleStartupAttempt::State::ControllerInitFailed),
                      static_cast<int>(attempt.begin(port)));
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().controllerBefore);
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().controllerAfter);
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().bluedroidBefore);
    TEST_ASSERT_EQUAL_UINT8(2, attempt.report().bluedroidAfter);
    TEST_ASSERT_EQUAL_INT32(0x103, attempt.report().sdkError);
    TEST_ASSERT_EQUAL_UINT(1, port.calls);
}
