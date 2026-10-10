#pragma once
#include "SdkPeerSecurityAbi.h"
#include <array>
#include <cstring>

namespace larp {
namespace companion {
namespace sdk446 {
struct SmpControlBlock;
// Metadata only. No SMP private keys, nonce or OOB material is read or copied.
// Offsets require both the original complete type compilation and pinned libbt
// instruction check in check_f1_smp_prereset_abi.py. No private record cast.
struct SmpPreReset {
    SmpCallback callback = nullptr;
    std::array<std::uint8_t, 6> address = {};
    std::uint8_t status = 0xff, overBr = 0xff, securityLevel = 0;
    std::uint8_t pairingMode = 0xff, cancelled = 0xff;
};
inline bool readSmpPreReset(const SmpControlBlock *control, SmpPreReset &out) noexcept {
    if (!control)
        return false;
    static_assert(sizeof(SmpCallback) == 4, "pinned 32-bit SMP callback pointer");
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(control);
    SmpPreReset value;
    std::memcpy(&value.callback, bytes, sizeof(value.callback));
    std::memcpy(value.address.data(), bytes + 37, value.address.size());
    value.overBr = bytes[53];
    value.status = bytes[56];
    value.securityLevel = bytes[61];
    value.pairingMode = bytes[598];
    value.cancelled = bytes[702];
    out = value;
    return true;
}
} // namespace sdk446
} // namespace companion
} // namespace larp

extern "C" void __real_smp_proc_pairing_cmpl(larp::companion::sdk446::SmpControlBlock *);
extern "C" void __wrap_smp_proc_pairing_cmpl(larp::companion::sdk446::SmpControlBlock *);
