#include "../../lib/GattsWriteAdmission/Esp32GattsSource.h"
#include <cstdint>

// Isolated observation probe for the pinned IDF 4.4.6 Bluedroid symbol.
// Key refresh and command errors also call this symbol: the arguments alone
// do not attest an original Encryption Change event or the restored bond mode.
// Discard the observation and forward unchanged; never grant security here.
extern "C" void __real_btm_sec_encrypt_change(std::uint16_t, std::uint8_t, std::uint8_t);
extern "C" void __wrap_btm_sec_encrypt_change(std::uint16_t handle, std::uint8_t status,
                                              std::uint8_t enabled) {
    (void)larp::companion::observeGattsHciEncryptionNow(handle, status, enabled);
    __real_btm_sec_encrypt_change(handle, status, enabled);
}
