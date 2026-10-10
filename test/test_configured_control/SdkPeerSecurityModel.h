#pragma once
// Test-only SDK metadata and actors. No SMP/RF/NVS durability claim.
#include "../../lib/GattsWriteAdmission/SdkPeerSecurityAbi.h"
#include <array>
namespace larp {
namespace companion {
namespace sdk446 {
union SmpData {
    std::uint8_t complete[5];
    std::uint32_t passkey;
};
} // namespace sdk446
} // namespace companion
} // namespace larp
namespace host_sdk_security {
using larp::companion::sdk446::SmpCallback;
SmpCallback callback = nullptr;
std::array<std::uint8_t, 6> address = {{1, 2, 3, 4, 5, 6}};
std::uint16_t packed = 257;
std::uint16_t hci = 0x123;
std::uint8_t interface = 1, transport = 2, flags = 0x16, state = 5, keyBytes = 16;
bool registerResult = true, connectionResult = true, flagsResult = true, stateResult = true,
     running = false;
unsigned registrations = 0, forwarded = 0, connections = 0, flagReads = 0, stateReads = 0,
         runningReads = 0;
std::uint8_t result = 23, lastEvent = 0;
std::uint8_t *lastAddress = nullptr;
larp::companion::sdk446::SmpData *lastData = nullptr;
void (*beforeForward)() = nullptr;
void (*beforeConnection)() = nullptr;
void (*beforeFlags)(std::uint8_t *) = nullptr;
void (*beforeState)(std::uint8_t *) = nullptr;
void (*beforeHci)(std::uint8_t *) = nullptr;
unsigned hciReads = 0;
void reset() {
    callback = nullptr;
    address = {{1, 2, 3, 4, 5, 6}};
    packed = 257;
    hci = 0x123;
    interface = 1;
    transport = 2;
    flags = 0x16;
    state = 5;
    keyBytes = 16;
    registerResult = connectionResult = flagsResult = stateResult = true;
    running = false;
    registrations = forwarded = connections = flagReads = stateReads = runningReads = 0;
    result = 23;
    lastEvent = 0;
    lastAddress = nullptr;
    lastData = nullptr;
    beforeForward = beforeConnection = nullptr;
    beforeFlags = beforeState = beforeHci = nullptr;
    hciReads = 0;
}
std::uint8_t original(std::uint8_t event, std::uint8_t *addr,
                      larp::companion::sdk446::SmpData *data) {
    ++forwarded;
    lastEvent = event;
    lastAddress = addr;
    lastData = data;
    if (host_rtos::depth)
        host_rtos::nonzeroWait = true;
    if (beforeForward) {
        auto hook = beforeForward;
        beforeForward = nullptr;
        hook();
    }
    return result;
}
void query() {
    if (host_rtos::depth || host_sdk_source::currentTask != reinterpret_cast<TaskHandle_t>(1))
        host_rtos::nonzeroWait = true;
}
} // namespace host_sdk_security
extern "C" {
bool __real_SMP_Register(larp::companion::sdk446::SmpCallback callback) {
    ++host_sdk_security::registrations;
    host_sdk_security::callback = callback;
    return host_sdk_security::registerResult;
}
bool GATT_GetConnectionInfor(std::uint16_t packed, std::uint8_t *interface, std::uint8_t *address,
                             std::uint8_t *transport) {
    using namespace host_sdk_security;
    query();
    ++connections;
    if (beforeConnection) {
        auto hook = beforeConnection;
        beforeConnection = nullptr;
        hook();
    }
    if (!connectionResult || packed != host_sdk_security::packed)
        return false;
    *interface = host_sdk_security::interface;
    *transport = host_sdk_security::transport;
    std::memcpy(address, host_sdk_security::address.data(), 6);
    return true;
}
bool BTM_GetSecurityFlagsByTransport(std::uint8_t *address, std::uint8_t *flags,
                                     std::uint8_t transport) {
    using namespace host_sdk_security;
    query();
    ++flagReads;
    if (beforeFlags) {
        auto hook = beforeFlags;
        beforeFlags = nullptr;
        hook(address);
    }
    if (!flagsResult || transport != 2 ||
        std::memcmp(address, host_sdk_security::address.data(), 6))
        return false;
    *flags = host_sdk_security::flags;
    return true;
}
bool BTM_GetLeSecurityState(std::uint8_t *address, std::uint8_t *state, std::uint8_t *keyBytes) {
    using namespace host_sdk_security;
    query();
    ++stateReads;
    if (beforeState) {
        auto hook = beforeState;
        beforeState = nullptr;
        hook(address);
    }
    if (!stateResult || std::memcmp(address, host_sdk_security::address.data(), 6))
        return false;
    *state = host_sdk_security::state;
    *keyBytes = host_sdk_security::keyBytes;
    return true;
}
std::uint16_t BTM_GetHCIConnHandle(std::uint8_t *address, std::uint8_t transport) {
    using namespace host_sdk_security;
    query();
    ++hciReads;
    if (beforeHci) {
        auto hook = beforeHci;
        beforeHci = nullptr;
        hook(address);
    }
    return transport == 2 && !std::memcmp(address, host_sdk_security::address.data(), 6) ? hci
                                                                                         : 0xffff;
}
bool BTM_BleSecurityProcedureIsRunning(std::uint8_t *address) {
    using namespace host_sdk_security;
    query();
    ++runningReads;
    return running || std::memcmp(address, host_sdk_security::address.data(), 6);
}
}
