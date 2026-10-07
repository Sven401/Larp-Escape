#pragma once
// Narrow ABI from pinned ESP-IDF4.4.6 SMP/BTM/GATT sources. No SDK patches.
#include "SdkGattSourceAbi.h"
namespace larp {
namespace companion {
namespace sdk446 {
union SmpData; // All events pass through unchanged; only completion's 5 bytes copied.
using SmpCallback = std::uint8_t (*)(std::uint8_t, std::uint8_t *, SmpData *);
} // namespace sdk446
} // namespace companion
} // namespace larp
extern "C" {
bool __real_SMP_Register(larp::companion::sdk446::SmpCallback);
bool __wrap_SMP_Register(larp::companion::sdk446::SmpCallback);
bool GATT_GetConnectionInfor(std::uint16_t, std::uint8_t *, std::uint8_t *, std::uint8_t *);
bool BTM_GetSecurityFlagsByTransport(std::uint8_t *, std::uint8_t *, std::uint8_t);
bool BTM_GetLeSecurityState(std::uint8_t *, std::uint8_t *, std::uint8_t *);
}

extern "C" bool BTM_BleSecurityProcedureIsRunning(std::uint8_t *);
