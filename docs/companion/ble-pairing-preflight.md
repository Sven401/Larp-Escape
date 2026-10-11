# App–ESP BLE pairing preflight (2026-10-11)

Status: read-only chip and backup evidence; no pairing firmware has been
uploaded. A phone-side pairing report is implemented in the local Android
preview, but an actual app–ESP connection has not occurred. Do not treat an
Android OS bond, advertisement, GATT service discovery, or serial line as a
GM role or diagnostic grant.

## Current box and rollback material

`COM3` answered `esptool.py v4.5.1 flash_id` as ESP32-D0WD-V3, MAC
`a8:42:e3:cd:4b:18`, 4 MiB. This matches the private Originalbox manifest,
not the separate ESP-NOW receiver. A full read-only capture of the **current**
box flash was made before any BLE-pairing upload:

- Private directory outside the product repository and OneDrive:
  `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261011-PrePairing`.
- `before-pairing-flash.bin`: 4,194,304 bytes, SHA-256
  `170b9392dd1970ce53c8d2d16fbcc78c6e590b625576494375626193322075a9`.
  Its adjacent private manifest records the file, MAC, flash size and hash.
- The original pre-upload full backup from 2026-10-10 remains separate:
  4,194,304 bytes, SHA-256
  `a502ea122c9c81e02637f593a2c8c03ddf5c2963fa5f4b878eb291e271c3f020`.
  A restore to the current state must use the **2026-10-11** image; restoring
  the original is a distinct user decision.

Offline parsing of the current capture found the existing two-OTA partition
table (NVS `0x9000/0x5000`, otadata `0xe000/0x2000`, app0
`0x10000/0x140000`, app1 `0x150000/0x140000`, SPIFFS
`0x290000/0x160000`, coredump `0x3f0000/0x10000`). Current otadata copy 0
starts with sequence 1; copy 1 starts with zero. The development layout keeps
all listed data offsets but replaces both OTA apps with one factory app at
`0x10000/0x280000`.

ESP-IDF v4.4 documents erased (all `0xff`) otadata as the factory-boot
selection when a factory app exists. Consequently, the current nonempty
otadata must not be assumed to select the candidate factory app. A future
upload procedure must include a separately reviewed `0xe000/0x2000` erase or
equivalent verified image step, then verify the booted partition on the real
box. See the official [ESP-IDF v4.4 OTA guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/system/ota.html).

## Exact candidate and narrow write plan

The isolated product integration branch is at commit `14222a5`. Its ESP32
single-app build and the combined host checks passed. The immutable private
snapshot is in `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Candidates/
BLE-Pairing-20261011-14222a5`; its manifest says `NOT_UPLOADED`.

| Image | Offset | Bytes | SHA-256 | Write? |
| --- | ---: | ---: | --- | --- |
| `bootloader.bin` | `0x1000` | 17,536 | `0f65515a7e0dfe9cc4bc78c01bef79c941af1b2deb34d01ddd500ee3d465eedf` | No; byte-identical to current flash |
| `partitions.bin` | `0x8000` | 3,072 | `97545f3b8c6c47f11e9aeb3e56ea662d7f70c5f48497d234a10fa653380c7d9d` | Yes |
| `otadata-erased.bin` | `0xe000` | 8,192 | `7d2c7ac4888bfd75cd5f56e8d61f69595121183afc81556c876732fd3782c62f` | Yes; both sectors `0xff` |
| `firmware.bin` | `0x10000` | 1,974,176 | `375854ad53113a035e6853f9b14346892d95f40fa10445e77c213d1643d2b522` | Yes; ends below factory limit `0x290000` |

The candidate retains NVS, SPIFFS and coredump offsets. The three intended
writes are one esptool multi-file operation at `0x8000`, `0xe000`, `0x10000`,
with `--after no_reset`. This operation is not atomic; loss of power could
require a separately authorized restore of the full current backup. There is
no mass erase and no bootloader write. The full write has **not** run.

Before a separately authorized write, check all four private image hashes,
current backup hash/size, and live MAC/4-MiB chip identity again. The guarded
`scripts/verify_pairing_rollback.py` supports offline verification and live
`restore-current --dry-run`; both passed. Its real restore requires a distinct
`--confirm-restore-current` action and has **not** run. The first dry run with
system Python stopped for a missing esptool package; the PlatformIO Python
dry run then passed without writing flash.

After any authorized write, independently read back and compare the three
written regions, verify both otadata sectors are `0xff`, and compare the
unchanged bootloader, NVS and SPIFFS against the pre-pairing backup before
normal app startup. Then observe the actual factory boot and app start. Only
the user operates pairing in the installed app and exports its report; a
later session evaluates the exported result. No Android bond, advertisement,
GATT discovery or serial line alone establishes a firmware GM role or
diagnostic grant.

Separate authorization for the firmware write is still outstanding. No flash
write, firmware upload, restore, RF pairing run, device test, or product
release was performed for this preflight.

No flash write, firmware upload, restore, RF pairing run, or product release
was performed for this preflight.
