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

## Recheck and next positive gate (2026-10-11)

The pinned source checks for both original key-send branches and the controller
result payload passed again. The 20-check `check_f1_record_binary_path.py`
audit also passed against the same `libbt.a` and linked ABI-probe map. Its
first two sandboxed invocations could not start the installed Xtensa archiver
(`WinError 623`, Windows DLL relocation); the same pinned command succeeded
when rerun outside that process sandbox. No binary input or expected offset
was changed to get a pass.

The source has two PENC/STK master sends and two LENC/STK slave replies. The
sender branches do not inspect the full stored `ble.auth_mode`. The pinned
controller Encryption Change handler reads only status, handle and enabled;
it receives neither the chosen key nor a command or GATT-birth identifier.
Therefore a matching result cannot identify a selected restored key. A
selected PENC/LENC bit plus the archive's 16-bit mode offset is still
insufficient to authorize a restored bond. The app's Android bond observation
cannot fill this gap.

At this recheck, the next isolated ABI probe still had to compile the **unchanged full**
`tBTM_SEC_DEV_REC` from the official v4.4.6 `btm_int.h`, with all dependent
original types and the feature switches actually used to build Arduino
2.0.14's pinned archive. At minimum compare whole size and the fields used
for address, BLE HCI handle, security state, BLE key mask, PENC/LENC key
location, key size and full 16-bit mode against archive instructions. The
currently installed public Arduino headers do not supply the private type
closure, and the selective 328-byte/offset audit is not a substitute.

After that ABI gate, a source-actor probe would still have to capture the actual selected
PENC or LENC branch and full mode for one frozen original GATT birth, without
retaining or exporting key bytes. It must reject STK, duplicate key sends,
re-keying, stale callbacks and recycled handles, correlate the command's
controller status and Encryption Change to that exclusive lifetime, and close
on disconnect. If the pinned SDK exposes no such exclusive observation,
restored-bond role admission remains unavailable in this build. Current
registry, challenge and credential checks remain independent requirements.

## Complete active record declaration and pinned archive (2026-10-11)

The isolated layout probe extracts the **unchanged complete**
`tBTM_SEC_DEV_REC` declaration from pinned v4.4.6 `btm_int.h`, together with
its unchanged `tBTM_SEC_BLE_KEYS`, `tBTM_SEC_BLE` and
`tBTM_LE_CONN_PRAMS` declarations. Its primitive-width definitions are checked
against pinned original `bt_common.h`, `bt_types.h` and `btm_api.h`. The pinned
Arduino 2.0.14 `dio_qspi` configuration enables BLE and SMP. Original
`bluedroid_user_config.h` and `bt_target.h` select privacy support but no
controller RPA list, BLE 5 extension or slave connection-parameter skip field;
`soc_caps.h` pins ESP32 device-privacy capability to zero. The original
`BTM_DISC_DURING_RS` field is present and L2CAP UCD field absent. The probe
compiles all active fields using the installed 32-bit Xtensa compiler. It
neither reads NVS nor emits any key bytes.

Run, from this firmware worktree:

```powershell
python scripts/check_f1_record_layout.py
python scripts/check_f1_record_binary_path.py
```

Both passed against pinned inputs: **17 compiler static assertions** for the
full active record and **23 archive/map disassembly checks**. The archive's
`BTM_SecAddBleDevice` allocates and clears 328 bytes. Its key selection and
restoration instructions agree with the compiled layout at these offsets:

| Original member | Xtensa byte offset | Independent archive check |
| --- | ---: | --- |
| `bd_addr` | 32 | restore path six-byte copy destination |
| `sec_state` | 150 | master byte load |
| `role_master` | 152 | connection byte store and encryption callback byte load |
| `ble_hci_handle` | 166 | master and slave 16-bit loads |
| `ble` | 176 | restoration BLE-subrecord base |
| `ble.key_type` | 199 | PENC and LENC mask tests |
| `ble.keys.pltk` | 216 | master PENC pointer |
| `ble.keys.lltk` | 248 | slave LENC pointer |
| `ble.keys.rand` / `ediv` | 280 / 288 | master command pointer / 16-bit load |
| `ble.keys.key_size` | 293 | slave byte load |
| `ble.auth_mode` | 304 | restore 16-bit store; compiler width assertion |

There is **no key union** in this original record: `tBTM_SEC_BLE_KEYS` holds
distinct peer and local key arrays. The compiler checks the entire active
record size, so the conditional tail is included even though only selected
instruction offsets are cross-checked. The archive source provenance is
bounded by its SHA-256 and these matching instructions; identical original
source and compile flags for every unobserved archive instruction are not
proved. This is a static layout match for the named fields, not a runtime-private-record reader
or an unrestricted claim about every archive access.

**F1 remains closed.** Neither test identifies a runtime-selected PENC/LENC
branch on one original GATT birth, excludes competing STK or duplicate sends,
nor associates a controller Encryption Change with exactly that key send.
No positive restored-bond origin, role grant or diagnostic release is inferred.
