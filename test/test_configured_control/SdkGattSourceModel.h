#pragma once
// Deterministic model of only the pinned C APIs; actual adapter is compiled.
#include "../../lib/GattsWriteAdmission/GattsWriteAdmission.h"
#include "../../lib/GattsWriteAdmission/SdkGattSourceAbi.h"
#include <freertos/task.h>
#include <vector>
namespace host_sdk_source {
struct Work {
    void (*function)(void *);
    void *context;
};
TaskHandle_t currentTask = reinterpret_cast<TaskHandle_t>(1);
std::vector<Work> work;
Work replayWork = {nullptr, nullptr};
std::vector<larp::companion::RoutedWriteResponse> sent;
struct ReadResponse {
    std::uint16_t connection = 0;
    std::uint32_t transaction = 0;
    std::uint8_t status = 0;
    larp::companion::sdk446::Response response = {};
};
std::vector<ReadResponse> readSent;
larp::companion::sdk446::Callbacks registeredCallbacks = {};
bool rejectPost = false, synchronous = false;
unsigned posts = 0, registrations = 0, passedConnections = 0, passedRequests = 0;
std::uint8_t registerResult = 1, sendResult = 0;
void (*beforePost)() = nullptr;
void (*beforeSend)() = nullptr;
void (*beforeForward)() = nullptr;
void drainOne() {
    if (work.empty())
        return;
    const auto item = work.front();
    work.erase(work.begin());
    item.function(item.context);
}
void drain() {
    while (!work.empty())
        drainOne();
}
void reset() {
    drain();
    work.clear();
    sent.clear();
    readSent.clear();
    posts = registrations = passedConnections = passedRequests = 0;
    rejectPost = synchronous = false;
    registerResult = 1;
    sendResult = 0;
    beforePost = beforeSend = beforeForward = nullptr;
    currentTask = reinterpret_cast<TaskHandle_t>(1);
}
void passConnection(std::uint8_t, std::uint8_t *, std::uint16_t, bool, std::uint16_t,
                    std::uint8_t) {
    ++passedConnections;
    if (beforeForward) {
        auto hook = beforeForward;
        beforeForward = nullptr;
        hook();
    }
}
void passComplete(std::uint16_t, std::uint8_t, std::uint8_t, larp::companion::sdk446::Complete *) {}
void passDiscovery(std::uint16_t, std::uint8_t, larp::companion::sdk446::Discovery *) {}
void passDiscoveryComplete(std::uint16_t, std::uint8_t, std::uint8_t) {}
void passEncrypted(std::uint8_t, std::uint8_t *) {}
void passCongestion(std::uint16_t, bool) {}
void passRequest(std::uint16_t, std::uint32_t, std::uint8_t, larp::companion::sdk446::Request *) {
    ++passedRequests;
}
} // namespace host_sdk_source
extern "C" {
void *btu_thread = reinterpret_cast<void *>(2);
bool osi_thread_post(osi_thread *thread, void (*function)(void *), void *context, int queue,
                     std::uint32_t timeout) {
    using namespace host_sdk_source;
    if (host_rtos::depth || timeout || queue || static_cast<void *>(thread) != btu_thread)
        host_rtos::nonzeroWait = true;
    ++posts;
    if (beforePost) {
        auto hook = beforePost;
        beforePost = nullptr;
        hook();
    }
    if (rejectPost)
        return false;
    if (synchronous)
        function(context);
    else
        work.push_back({function, context});
    return true;
}
std::uint8_t GATTS_SendRsp(std::uint16_t packed, std::uint32_t transaction, std::uint8_t status,
                           larp::companion::sdk446::Response *response) {
    using namespace host_sdk_source;
    if (host_rtos::depth)
        host_rtos::nonzeroWait = true;
    if (beforeSend) {
        auto hook = beforeSend;
        beforeSend = nullptr;
        hook();
    }
    larp::companion::RoutedWriteResponse value;
    value.original.connection.handle = packed;
    value.transaction = transaction;
    value.status = static_cast<larp::companion::AttWriteStatus>(status);
    if (response) {
        ReadResponse copied;
        copied.connection = packed;
        copied.transaction = transaction;
        copied.status = status;
        copied.response = *response;
        readSent.push_back(copied);
    } else
        sent.push_back(value);
    return sendResult;
}
std::uint8_t __real_GATT_Register(larp::companion::sdk446::Uuid *,
                                  const larp::companion::sdk446::Callbacks *callbacks) {
    using namespace host_sdk_source;
    ++registrations;
    if (callbacks)
        registeredCallbacks = *callbacks;
    return registerResult;
}
}
