# Closed production BLE owner: measured build gate

Status on 2026-10-11: `fec047e` remains the buildable production state.
`Esp32CompanionIdentityOwner` starts the confirmed public identity and a closed
policy before hardware or radio startup. The product still does not initialize
Bluetooth, register GATT, advertise, authenticate a peer or open diagnosis.

An isolated implementation attempt on branch
`plan/nutzergefuehrte-hardwarediagnose-ble-owner` connected the existing
`Esp32GattsSource` and `Esp32GattsService` to the actual product startup: one
process-lifetime owner, bounded queues, a separate service/ATT worker, the
`GATT_Register` and `SMP_Register` wrappers armed before controller/Bluedroid
initialization, and a selected GATTS callback. It did not start advertising or
install registry, credential, role or diagnosis workers. The build verified
the pinned ESP32 `libbt.a` SHA-256
`1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`
and linked an ELF, then failed the program-size gate. The attempt was removed
from the product files before commit. The ELF is a capacity measurement, not
a flashable firmware or a runtime test.

Measured with `platformio run -e az-delivery-devkit-v4`, PlatformIO
`espressif32@6.6.0`, Arduino ESP32 `2.0.14`, board
`az-delivery-devkit-v4`, 4 MiB flash, on the original product partition map:

| Build | App image | OTA app slot | Static RAM |
| --- | ---: | ---: | ---: |
| Identity-only baseline `fec047e` (fresh counter-build passed) | 1,236,489 B | 1,310,720 B | 56,692 B |
| Closed BT/GATT integration attempt (linked, size check failed) | 1,949,801 B | 1,310,720 B | 73,212 B |

The attempt exceeds the current OTA app slot by **639,081 B**. The SDK's
`tools/partitions/default.csv` assigns `app0` and `app1` 0x140000 bytes each,
with NVS, SPIFFS and coredump partitions. Changing that layout, removing OTA
or reducing other product functions is a separate architecture decision and
requires a rollback and storage assessment. No partition or firmware upload
was changed in this cut.

Even after a size decision, product admission requires evidence that the
selected original SDK actor and service callbacks retain their registration
and connection birth in the actual build. The current isolated ABI probe
shows source mechanics, not a live product registration. F1 full-width
restored bond mode and selected-key provenance, the current registry/P256/
credential and same-birth role ACK, and an original-bound diagnostic transport
remain open. Generic bond/authentication success cannot grant authority.

## Single-app development layout: isolated build check, 2026-10-11

The development candidate is `partitions/development-single-app.csv`, selected
only by `az-delivery-devkit-v4-single-app-capacity`. The standard product build
continues to use its two-slot map. The candidate replaces `app0` and `app1`
with one `factory` app from `0x10000` to `0x290000` (0x280000 = 2,621,440 B).
The generated partition binary was decoded with the pinned framework's
`gen_esp32part.py`: NVS remains at `0x9000`/`0x5000`, otadata at
`0xe000`/`0x2000`, SPIFFS at `0x290000`/`0x160000`, and coredump at
`0x3f0000`/`0x10000`. The parser verified the table without overlap.

Build command: `platformio run -e az-delivery-devkit-v4-single-app-capacity`.
PlatformIO `espressif32@6.6.0`, Arduino ESP32 `2.0.14`, ESP32 4 MiB board;
result **SUCCESS**. The size gate reported 1,236,553 B used and 56,692 B
static RAM. The generated `firmware.bin` is 1,242,848 B, leaving 1,378,592 B
against the 2,621,440 B app partition by binary length. These figures describe
the current identity-only product. The previous closed BLE integration was
removed before commit `d677729`; its exact integration source is not present
on this branch. Its reported 1,949,801 B linked size would be 671,639 B below
the candidate's limit only as an arithmetic comparison across distinct builds,
not a reproduced BLE build or a bound on future F1, role, and diagnosis code.

Reconstruction audit on 2026-10-11: `git log --all` found no committed
`src/main.cpp` change containing `esp_bt_controller_init` or
`Esp32GattsService`. `git fsck --full --no-reflogs --unreachable` found six
unreachable commits, none carrying that production startup, and a scan of
5,441 unreachable blobs found no `esp_bt_controller_init` or the attempted
`Esp32GattsService` construction. The removed working-tree experiment was
therefore not recoverable as a defined Git revision. Recreating a new BLE
startup from the ABI probe would be new integration work, not reproduction of
the 1,949,801 B build; it could silently change callback, worker, or authority
semantics. The next gate is to implement and review a fresh, narrowly closed
production owner from the pinned SDK and current contracts, then build that
exact source against this layout and report its actual image length and RAM.
It must keep advertising, registry/credential/role and diagnostic admission
disabled until their separate proofs exist.

This check did not execute the ESP32 bootloader. A future device procedure must
verify how the actual bootloader chooses the sole `factory` app when existing
otadata still contains a prior OTA selection, and must specify how the old
otadata is handled during a USB layout migration. The app's runtime, GATT
registration, security/role admission, and reconnect behavior are unverified.
No upload, restore, flash write, or hardware test occurred. The accepted
development fallback is another USB flash; two-slot OTA remains a later
product-level feasibility and rollback target.
