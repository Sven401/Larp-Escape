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

## Gates before any firmware write

1. Finish and build the exact closed pairing firmware. Record its commit,
   bootloader, partition binary, app image hash/size, write offsets and
   unchanged game/authority/diagnosis constraints.
2. Recheck the current backup size/hash and live chip MAC/4-MiB identity.
   Keep the current and original captures outside Git. Verify the restore
   command's checks against the selected current manifest without running a
   restore. The existing `restore-original` command is named and documented
   for the 2026-10-10 original image; its confirmation flag must not be used
   to imply approval to restore the 2026-10-11 current image. Define a
   distinct, reviewed current-state rollback procedure first.
3. Define and review the ordered partition/app/otadata write and readback
   procedure, including loss-of-power recovery and the full-current-image
   rollback path. The only intended writes are bootloader at `0x1000`, table
   at `0x8000`, both otadata sectors at `0xe000/0x2000`, and app at `0x10000`;
   no mass erase. Independently read back these areas, confirm all otadata
   bytes are `0xff`, and compare retained NVS/SPIFFS with the current backup.
   Confirm the factory selection and app start on the real device after any
   separately authorized upload.
4. Obtain separate authorization for the actual firmware write. The user then
   operates pairing in the app and exports the report. A later session may
   evaluate it. No Codex session drives the user-led pairing or box test.

No flash write, firmware upload, restore, RF pairing run, or product release
was performed for this preflight.
