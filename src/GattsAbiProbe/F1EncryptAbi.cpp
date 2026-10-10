#include "../../lib/GattsWriteAdmission/Esp32GattsSource.h"
#include <cstdint>

// Original outer dispatch distinguishes Encryption Change from key refresh and
// command errors. A bounded copy lives only on this synchronous BTU call stack.
extern "C" void __wrap_btu_hcif_process_event(std::uint8_t controller,
                                              larp::companion::sdk446::HciHeader *packet) {
    const bool owned = larp::companion::enterGattsHciEventNow(controller, packet);
    __real_btu_hcif_process_event(controller, packet);
    if (owned)
        larp::companion::leaveGattsHciEventNow();
}
// Observe only a matching original event, then forward unchanged. The selected
// restored bond mode is still unproven; this probe never grants security.
extern "C" void __real_btm_sec_encrypt_change(std::uint16_t, std::uint8_t, std::uint8_t);
extern "C" void __wrap_btm_sec_encrypt_change(std::uint16_t handle, std::uint8_t status,
                                              std::uint8_t enabled) {
    (void)larp::companion::observeGattsHciEncryptionNow(handle, status, enabled);
    __real_btm_sec_encrypt_change(handle, status, enabled);
}

// Observe pre-reset metadata only in the pinned original call. Never retain
// private SDK pointers or turn a stored callback mode into security/authority.
extern "C" void __wrap_smp_proc_pairing_cmpl(larp::companion::sdk446::SmpControlBlock *control) {
    const bool owned = larp::companion::enterGattsSmpCompletionNow(control);
    __real_smp_proc_pairing_cmpl(control);
    if (owned)
        larp::companion::leaveGattsSmpCompletionNow();
}
