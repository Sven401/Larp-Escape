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
