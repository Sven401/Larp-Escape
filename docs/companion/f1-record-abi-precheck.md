# F1 record ABI and binary-path critical precheck (2026-10-11)

The requested `grilling` tool is absent after reading the local `grill-me`
skill. This critical precheck precedes the static ABI work.

**Goal:** determine the size and relevant offsets of the original
`tBTM_SEC_DEV_REC` under the product's ESP32 SDK configuration, then compare
them with the linked `libbt.a` key-selection instructions where possible.

**Assumptions to challenge:** upstream ESP-IDF v4.4.6 declarations may differ
from Arduino's compiled configuration; `libbt.a` may be built from other source
or flags; compiler-derived offsets alone do not identify the runtime-selected
record; map inclusion alone does not prove a key was used.

**Counterevidence:** the SDK callback truncates the stored 16-bit mode, and
source-level restoration, handle lookup and key selection are separate steps.
The product build has an ABI-probe map, not an instrumented live BLE link.

**Security boundary:** inspect only static SDK headers, build configuration,
map and archive instructions. No NVS, bonds, keys, device access or RF action.
No private field reader or positive F1 admission from inferred offsets.

**Acceptance gate:** pin the actual archive/toolchain and configuration, produce
reproducible offset checks, and identify instructions for both PENC and LENC
branches or document exactly why either cannot be established. Runtime-selected
key, same-birth correlation and current authorization remain independent gates.

## 2026-10-11: target configuration and competing sends

`python scripts/check_f1_key_send_exclusivity.py` passed. It pins the installed
Arduino ESP32 `dio_qspi/include/sdkconfig.h` (SHA-256
`5a87fa1f3311cb7c832dce58b9b842cb31642907d74ebf9e94fec4b502c07f3b`),
`libbt.a` (`1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`)
and the checked original `btm_ble.c`. The target enables Bluedroid, BLE, SMP
and GATTS. The original master function can send either STK or stored PENC;
the slave function can reply with either STK or stored LENC. Neither sender
checks `ble.auth_mode` or carries an application GATT-birth token. An identical
Encryption Change tuple can follow different modes, keys and recycled births.

The existing binary audit passed again: 20 disassembly/map checks verify the
328-byte archive allocation and relevant PENC/LENC, handle and full 16-bit
mode offsets. The source model and HCI declaration check also passed. These
tests do **not** compile the complete original `tBTM_SEC_DEV_REC` with all
Arduino private feature macros, and the Arduino package does not provide its
internal header. The target config pin narrows the configuration uncertainty;
it does not turn the partial offset audit into a complete private ABI proof.
No runtime-selected record, exclusive HCI command-to-result lifetime or
same-original-GATT-birth link was observed. F1 role admission stays closed.
