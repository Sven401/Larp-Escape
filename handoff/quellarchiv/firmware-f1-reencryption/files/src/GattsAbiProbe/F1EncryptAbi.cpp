#include <cstdint>
#include "../../lib/GattsWriteAdmission/Esp32GattsSource.h"

// Link-only probe for the pinned IDF 4.4.6 Bluedroid symbol. This wrapper
// forwards without changing security state; the production source does not use it.
extern "C" void __real_btm_sec_encrypt_change(std::uint16_t, std::uint8_t, std::uint8_t);
extern "C" void __wrap_btm_sec_encrypt_change(std::uint16_t handle, std::uint8_t status,
                                                std::uint8_t enabled) {
    (void)larp::companion::observeGattsHciEncryptionNow(handle, status, enabled);
    __real_btm_sec_encrypt_change(handle, status, enabled);
}
