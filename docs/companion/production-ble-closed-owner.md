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
