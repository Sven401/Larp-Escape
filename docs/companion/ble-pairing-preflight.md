# App–ESP BLE pairing preflight (2026-10-11)

Status: initial pairing firmware uploaded with separate user approval; full
flash readback passed, but BLE stack startup failed on the real box. A
phone-side pairing report is implemented in the Android preview, but an
actual app–ESP connection has not occurred. Do not treat an
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

The initially uploaded product integration candidate was commit `14222a5`. Its ESP32
single-app build and the combined host checks passed. The immutable private
snapshot is in `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Candidates/
BLE-Pairing-20261011-14222a5`. Its original manifest was made before upload.

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
no mass erase and no bootloader write. This was the scope of the separately
approved 2026-10-11 upload.

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

## First upload result, 2026-10-11

The user separately approved this exact upload. The live preflight again
matched MAC `a8:42:e3:cd:4b:18`, 4 MiB, and the current-backup hash. All
private candidate hashes matched the table above. Esptool wrote only the
three selected regions and verified each written image hash. A full 4-MiB
readback was then made while the chip remained in the bootloader:
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/
COM3-20261011-PostPairing/after-flash.bin`, SHA-256
`38682f775602b128c1e706d3d3b60d60d237596cdcbbdfd8cd10f2a6c0f2ecae`.
Each candidate image matched its readback bytes, both otadata sectors were
`0xff`, and all ranges outside the erased write sectors matched the current
backup, including bootloader, NVS, SPIFFS and coredump. The app sector tail
through `0x1f1fff` was erased. A private `postflash-verification.json` beside
the readback records these checks.

`esptool run` reset the box. Serial observation showed `Runtime ready`, two
MCP expanders, NFC firmware, and game phase 0. It also showed `Closed GATT
registration state: 4`, which maps to `ProductGattsOwner::StackFailed`, and
`BLE discovery phase: 0` (`Closed`). Therefore the advertised BLE service is
**not running**; no app–ESP pairing was attempted or observed. A corrected
firmware candidate needs its own build, preflight and separate upload
approval. No restore, user-led RF pairing, physical diagnostic acceptance or
product release occurred.

## Closed BLE startup telemetry candidate, 2026-10-11

The isolated BLE fix-investigation package `3c0fe4e` is integrated at
`2028896`. It does not retry a failed stack start. On failure, it prints the
exact ESP-IDF step and error code, controller/Bluedroid states, and internal
free/largest heap blocks before and after the attempt. It grants no role or
diagnosis. Integrated configured-control host checks passed 529/529; the
ESP32 single-app build passed with ELF flash 1,968,189/2,621,440 bytes and
static RAM 76,044/532,480 bytes.

The private candidate, recorded before its separate upload, is
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Candidates/
BLE-Stack-Diagnostic-20261011-95acedf/firmware.bin`, 1,974,480 bytes,
SHA-256 `16a828694ba4d4a0c6705c15df154f8fa9cf58838ae2215f70566bb06738673b`.
Bootloader and partition-table image hashes equal the first upload; otadata
also remains unchanged. The separately approved second upload wrote only
this app image at `0x10000`, without mass erase.

Because the first image booted, the present 4-MiB flash was captured again
read-only in `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/
COM3-20261011-PreBleStackTelemetry/before-ble-telemetry.bin`, SHA-256
`155c425805863d9d0a4b9464116c17962ad04ffc6fb8956768c2be0411fce1ec`.
It differs from the immediate first-upload readback by 113 NVS bytes; the
app, layout, otadata and SPIFFS matched. The distinct chip-bound rollback
tool `scripts/verify_ble_telemetry_rollback.py` and private manifest passed
seven fake-esptool tests, offline verification and a live COM3 dry run with
the Originalbox MAC and 4-MiB size. No restore has run.

Esptool verified the app-only second write hash. Full 4-MiB readback while
in the bootloader is stored privately at `C:/Users/seide/AppData/Local/
LarpEscape/ESP32-Backups/COM3-20261011-PostBleStackTelemetry/
after-ble-telemetry.bin`, SHA-256
`ff1ac0bbd32c299006aae73045560e0a7db89a80344a6540ac9c8670fd31edb8`.
The app image matched exactly, its final sector padding was erased, and every
other byte matched the immediately preceding full backup, including NVS,
otadata, SPIFFS, bootloader and partition table. The adjacent private
`postflash-verification.json` records the comparison and serial observation.

After reset, the game reached `Runtime ready`, but closed GATT registration
again reported state 4. The added telemetry identified failing step 1,
`esp_bt_controller_init`, with SDK error 259 (`0x103`). Controller and
Bluedroid status stayed `0 -> 0`; internal free/largest 8-bit heap blocks
stayed `213576/110580` bytes. Discovery phase remained 0 (`Closed`). These
observations locate the failed SDK call; they do not yet establish why the
controller rejected initialization. No app–ESP pairing, restore or product
acceptance occurred. Any corrected image needs its own build, current-state
backup check and separate upload approval.

## BLE controller startup fix candidate, 2026-10-11

The pinned Arduino 2.0.14 startup calls its weak `btInUse()` before product
`setup()` and otherwise releases all BT controller memory. This explains the
observed idle controller plus SDK error 259 on init. The isolated BLE fix
`6407199`, integrated at `671be0b`, supplies a product-only strong answer
and uses BLE mode for both controller init and enable. The integrated ELF map
selects `btInUse` from `libClosedBleStackOwner.a` and discards the weak
Arduino definition. Host checks passed 529/529; integrated ESP32 build
passed, ELF flash 1,968,193/2,621,440 bytes and RAM 76,044/532,480 bytes.

The private candidate, recorded before its separate upload, is
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Candidates/
BLE-Startup-Fix-20261011-671be0b/firmware.bin`, 1,974,496 bytes,
SHA-256 `e3d81c8173e03e7e77e72d75669aa84226b399ad6911c6ec98961cdba7b48973`.
Bootloader and partition-table hashes are unchanged. The separately approved
third write touched only the app at `0x10000`.

An attempted new full capture after the second image booted proved byte-for-
byte identical to the existing second-upload full readback, including NVS.
The redundant new file was removed. The exact current-state rollback source
is therefore the already verified
`COM3-20261011-PostBleStackTelemetry/after-ble-telemetry.bin`, SHA-256
`ff1ac0bbd32c299006aae73045560e0a7db89a80344a6540ac9c8670fd31edb8`.
The separate chip-bound `scripts/verify_ble_startfix_rollback.py` and
private manifest passed seven fake-esptool tests, offline verification and a
live COM3 dry run (MAC `a8:42:e3:cd:4b:18`, 4 MiB); no restore.
No further full capture is needed unless a targeted state check shows this
source no longer matches the current chip. A 28-KiB NVS/otadata read before
the third write matched this source exactly.

Esptool verified the third app-only write hash. To respect the user's limit
on fresh full captures, the independent readback covered only the entire
written app sector (`0x10000` through `0x1f2fff`) and NVS/otadata. The app
bytes matched the candidate exactly and the sector tail was `0xff`;
app-sector readback SHA-256 is
`6cf1cc8c9283b74d5b644cb2deccea00df3ef251c0f9bd25a90ed6597271a026`.
NVS and otadata matched the existing full rollback source. Bootloader,
partition table, SPIFFS and coredump were outside esptool's reported erase
range; they were not reread after this third write. Private targeted checks
and a `startup-verification.json` are under
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Checks/
COM3-20261011-PostBleStartupFix`.

After reset, serial observation showed `Runtime ready`, closed GATT
registration state 7 (`RegistrationPending`) and BLE discovery phase 3
(`Visible`) on the actual box. This confirms that the previous Bluetooth
stack failure was cleared and connectable advertising reached its SDK
completion. It is **not** an App–ESP pairing, OS bond, F1/GM role or
diagnostic grant. The user must perform the pairing flow in the installed
Android app and export its report. No Codex session drives that flow. No
restore or product acceptance has occurred.

## Android report durability update, 2026-10-11

The Android report follow-up `f80440c` plus `08e1f61` is integrated at
`8eb1489`. A versioned app-private journal bounds the observation history
to 256 events and 128 KiB, saves it atomically, and retains a pending export
snapshot across process restart. Recovery adds `connection_closed`; stored
events remain historical evidence and do not revive the live BLE connection
or authority. An already bonded or bonding Android device does not receive
another `createBond()` request. Ten targeted JVM tests and the integrated
offline Debug build passed.

The updated preview APK was installed on Moto G14 `ZY22J7CSQD` while no
pairing attempt was running. SHA-256 of the built APK and the APK read back
from the phone both equal
`79bd30ead3751f8c1d904ef533a374e595535b87d5f63dd648ab71519794ca37`.
Codex did not open the app or conduct a pairing attempt. No exported user
report is available yet, so App–ESP pairing and physical diagnosis remain
unverified.

## User scan reports and scan-outcome update, 2026-10-11

The user exported three pairing reports after the BLE startup fix. Their
SHA-256 values, in filename order `ble-kopplungsbeobachtung-v1.json`, `(1)`,
`(2)`, are `a5c289951674bf229a71d53479c79f5aaae7138738213ad25c3642a924a11b9f`,
`617d5ee22aaff2b9a0271fac041c91bf3cb564b497fccbe44606d93161beb7bc`,
and `9d99103b564a71462631f6f58cbef2608ffa9880220c1c9ca85254454d9bb3b8`.
The first two contain only `report_started`; the third also contains two
`scan_started` events. None contains a product candidate, GATT connection
attempt, service discovery or OS bond. The app log said `Suche beendet: 0
Geräte`. Android's system pairing list showed the user only `[TV]
UE55JU6870`, not the box. Since the product advertisement omits a device
name, the system list alone cannot establish whether its BLE advertisement
was currently on air. The earlier firmware `Visible` log is a point-in-time
startup observation, not a continuous radio measurement.

Commit `9733c08` adds bounded `scan_end` evidence: callback count, filtered
count, canonical-service candidate count and a distinct end reason. Its
targeted JVM test and offline Debug build passed. After the user confirmed
no attempt was running, the APK was installed on Moto G14 `ZY22J7CSQD`;
the built and pulled installed APK share SHA-256
`87bc6f6b26ea2478a96f0f832500a518aa27e163414844697f75c6fe82c28cd6`.
The user has been asked to start a fresh pairing report, run the scan in the
app, and export the result. That new report is pending. The device still
runs the separately approved `671be0b` image. No additional firmware upload,
restore, full backup, physical diagnosis or product acceptance followed.

## Filtered callbacks and unverified diagnostic scan, 2026-10-11

The next user export, `ble-kopplungsbeobachtung-v1 (4).json`, is 1,566 bytes,
SHA-256 `ff5dda2e86c466dd4b3c622e5833408188e5dd67518fa7acd6fa6b3ed89638eb`.
Two complete app scans received 135 and 92 BLE callbacks respectively;
all were rejected by the product/name filter and neither scan saw a
canonical-service candidate. There was no connection attempt or OS bond.
This proves Android delivered other BLE advertisements to the app, but does
not distinguish absent box radio packets from packets without a parsed
canonical service UUID. A subsequent passive, command-free 12-second COM3
read saw the box's periodic `BLE discovery phase (no role): 3` and
`Original BLE link current (no bond claim): 0`. The firmware phase is an SDK
state, not an independent over-the-air capture.

The loaded `671be0b` source keeps the 16-byte little-endian UUID in stable
storage and configures connectable public-address advertising. Its UUID
matches Android's canonical service UUID; no unconditional advertising
timeout was found. The [ESP-IDF v4.4 system API](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/system.html)'s four-universal-MAC configuration derives the
Bluetooth public address from the chip base MAC plus two in the last octet.
For chip MAC `A8:42:E3:CD:4B:18`, `A8:42:E3:CD:4B:1A` is therefore the
expected Bluetooth address, still unconfirmed by a received packet. Searching
for `...:18` in Android's pairing settings was an invalid identity check.
Android's `BLUETOOTH_SCAN` manifest uses `neverForLocation`, which the
[Android documentation](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions) says may suppress some BLE beacon results; whether it
suppresses this box remains unmeasured.

Commit `47f74d4` adds a second, explicitly user-started app scan showing up
to 32 otherwise filtered advertisements as **unverified** candidates. It
records mode and aggregate scan outcome; unrelated device addresses enter
the report only if the user selects one. No candidate automatically connects,
bonds, acquires a role or grants diagnosis. The selected candidate must pass
canonical GATT service discovery even if a lab service is present. App unit
tests passed 58/58 and the offline Debug build succeeded. The APK built and
pulled back from Moto G14 `ZY22J7CSQD` matches SHA-256
`d0bb1fdd37359d8c447c338ae07dc0f4d809bf85f11b31231a46a9e2253f3425`.
The user has been asked to run this scan and export its report independently.
No new firmware image was uploaded, restored or accepted.
