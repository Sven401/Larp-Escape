# Reichstagsbox diagnostic readiness — 2026-10-11

Status: **not ready for the complete user-run test**. This document records a
software build and the concrete closed gates. It is not a hardware acceptance,
upload approval, installation record or instruction to start the user run.

## Reproducible artifacts

- Product source branch/implementation commit:
  `plan/nutzergefuehrte-hardwarediagnose-startintegration` /
  `557a94b` (journal follow-up to `787d83b`).
  Firmware sources are unchanged from `77028cb74edbaa16ca33b4fcabc63a5c5cdbe94f`.
- Built firmware image:
  `.pio/build/az-delivery-devkit-v4-single-app-capacity/firmware.bin`,
  2,000,224 bytes, SHA-256
  `0964B7092F5ED4F2E6D038509D0D3776D196B2D87D38FB07AEFC212666BE9E67`.
  ELF use: 1,993,929/2,621,440 bytes flash and 80,484/532,480 bytes static RAM.
  This image was not uploaded.
- Built installable Debug APK:
  `companion/android/app/build/outputs/apk/debug/app-debug.apk`,
  23,977,655 bytes, SHA-256
  `D8F8FAE82E3E9ECBDD8D7424DE71ADCBED64A006A3BF07AD2FFEB8F8738875CF`.
  This revised build contains the report contract and journal core. It has not
  been installed because ADB no longer detected the phone at the update check.
  The APK still lists every required area with its concrete blockers and
  sends no request for the blocked areas.

The APK includes the readiness UI, report contract and journal core from the
product source commits above. No firmware source file changed in those commits.

## Connected phone state

The subsequently connected phone was identified read-only over ADB as a
Motorola moto g14 (`ZY22J7CSQD`) running Android 14 / API 34. It reports BLE,
NFC, NFC HCE and NFC UICC features. Both runtime Bluetooth permissions and NFC
are granted to the normal companion and the diagnostic preview.

Both packages were already installed and running before this inspection:

- `de.larpescape.companion`, version `0.1`, installed base APK 27,156,741
  bytes, SHA-256
  `61B584EFF9495D255D495B8F71FB44F668AFD6D8C79EE6BA9943784BECB57E63`.
- `de.larpescape.companion.diagnosticpreview`, version
  `0.1-diagnostic-preview`, installed base APK 23,458,554 bytes, SHA-256
  `BE2626C2519D15E189FDF947B23FE7B4014801D23784825777C132E34523DCC0`.

The initially installed diagnostic preview was therefore not the newly built
APK above. Both old and new diagnostic APKs have the same Android debug signing
certificate, SHA-256
`6B755048E55C8BE19EF3F73B1A015F96735F6E79148AC8960BD244E6F9BA4837`,
so an in-place update was technically possible. After explicit approval,
`adb install -r` completed successfully at 23:03:25 local phone time. A fresh
readback of the installed base APK was 23,959,403 bytes with SHA-256
`B1BF72BB6B46B165624B8C87B896F9B28FCBB68079C2EC16EE728E6D4E712DDC`,
an exact match to the build that was installed then. BLE and NFC permissions
remained granted.
The update stopped the previous process as expected; no activity was launched
afterward and no hardware run was started. The later corrected build above has
the same debug signing certificate, but a subsequent `adb devices -l` returned
no devices. Its `adb install -r` attempt returned `no devices/emulators found`;
the installed phone version is therefore still the `B1BF72...` build unless a
later phone-side action changes it.
The later journal build `D8F8FA...` has also not been installed. The latest
read-only ADB query again listed no attached device.

## Actual box state

The original box remains on the separately approved reply-transport probe from
firmware commit `56b63bd`. Its uploaded app image was previously verified as
1,982,784 bytes, SHA-256
`FADD7A7E13D9A4A96792A6762E87CC825CC3C7D6892B7D4B254290B3E9205644`.
A passive COM3 observation in this session reported BLE discovery phase 3,
no current original link, no missed LED frame deadline and no queue drop. This
identifies the running probe behavior; it is not a fresh flash readback.

A read-only `get_security_info` attempt identified the same ESP32-D0WD-V3,
revision 3, MAC `A8:42:E3:CD:4B:18`, but this ROM/stub returned “Command not
implemented”. No eFuse security conclusion is therefore claimed. The command
left the chip in the stub; `esptool run` only restarted the already loaded app.
The following passive observation confirmed normal runtime output again. No
flash, eFuse or restore operation occurred.

## Closed gates

1. **Authority:** Android OS bonding is Just Works and does not establish a
   Game Master role. `APP_MUTUAL_V1` and the selected NFC authentication path
   remain development contracts without production enrollment, protected box
   key storage, Finished exchange, role ACK or revocation/rollback proof.
   Control and Step writes must therefore remain `InsufficientAuthorization`.
2. **Ownership and LED off:** the closed arbiter fences software and has an
   independent ESP timer, but no electrical or visible all-off feedback. There
   is no productive component-owner handoff or positive diagnostic grant.
3. **LED profile:** only `GPIO15 / as-built-user-confirmed` may be displayed,
   still without type, level, supply or visible-function proof. GPIO4 conflicts
   with PN532 SS. No alternative profile is electrically verified.
4. **Matrix and Wake:** the current source profile maps matrix row 1 and Wake 1
   to MCP23017 GPB7 pins used as inputs. On an actual MCP23017 GPB7 is
   output-only. Startup directions also conflict with scan directions. A
   hardware correction and a confirmed pin/idle-level contract are required
   before an 81-point or four-input grant.
5. **Audio/SD:** DFPlayer UART/BUSY/Wake as-built wiring, SD inventory and
   audible output remain unconfirmed; no productive owner exists.
6. **NFC:** PN532 readiness was intermittent. A bounded production raw port,
   stable reader ownership, tag type/capacity protection and both independent
   byte-comparison paths are absent.
7. **ESP-NOW:** the product lacks a durable receiver report correlated by run
   and packet identifier; channel 0/coexistence and the receiver channel remain
   open.

## Action-by-action capability table

| Action | Firmware path at `77028cb` | Android path | Reply/evidence | Gate and targeted checks | Actual box |
| --- | --- | --- | --- | --- | --- |
| Diagnostic authority | `ProductGattsOwner` creates the closed boot identity and F0 graph; `DiagnosticGattsService` registers protected Control/Step/Reply values. No APP_MUTUAL/NFC owner publishes a current Game Master authority. | `AndroidDiagnosticGattShellV1` discovers and subscribes; its productive authority supplier remains null. | The loaded probe can emit one typed value on each Reply characteristic. Those notifications are transport evidence only. | Request writes remain `InsufficientAuthorization`. Configured-control/F0 host coverage and both Android notification probes pass, but no Finished, role ACK, diagnostic grant or revocation path exists. | `56b63bd` showed the two reply probes in the earlier user run. Current passive status has no link. |
| LED single | `ClosedDiagnosticArbiter`, `Esp32LedParkDeadline` and `GameLightEffects` fence the game, arm a 400 ms timer and submit black under the shared mutex. The v1 `diagnostic::Control` LED operation still has only an `IPort` fake; no productive request owner connects it to the arbiter. | The pilot generates a synthetic LED request/reply and keeps user observation separate. The new readiness view exposes only GPIO15 and no alternative profile. | Host reply fields include render calls and `restoredOff`; these are software observations. There is no electrical or visible all-off signal. | No role, positive grant or component handoff. The arbiter deliberately reports no grant. Application 10/10, regression 20/20 and diagnostic owner/step checks pass; physical off, WS2811/GRB, level and supply remain open. | The loaded probe runs the game/LED worker but exposes no LED diagnostic command. No LED action was attempted in this session. |
| LED selection, color and brightness | `GameLightEffects` owns one 23-pixel framebuffer and supports global brightness, but the diagnostic `IPort` does not expose a productive index/group renderer. | The readiness registry tracks index, group, RGB and brightness separately; all remain non-clickable. | No device reply names selected indices, applied RGB or effective brightness. | Define bounded 0–22 index/group payloads and an owner adapter behind the still-closed grant; host-test rejection and readback. Hardware profile remains GPIO15 only. | No request is loaded or executed. |
| LED effect, start, duration, stop and independent off | `GameLightEffects` implements Twinkle, Burst, Ripple, BrightnessRamp and palette primitives. `Esp32LedParkDeadline` supplies a software deadline, but a black render is not electrical off feedback. | The readiness registry tracks configuration, start, duration expiry, stop and independent off separately; no control is enabled. | Existing host metrics prove only calls, deadlines and queue behavior. No current/level/visible evidence is returned. | Add a bounded effect contract, productive owner, correlated terminal reply and a genuinely independent off proof before enabling. | No request is loaded or executed. |
| Audio clip/track, play, stop, repeat and volume | `DFMinniHandler` can initialize UART1 at 9600, wake and call `playTrack`; it has no stop, repeat, volume, bounded clip inventory or diagnostic owner/reply. Current source uses RX25/TX26, BUSY14 and Wake13. | The readiness registry tracks named Reichstag and Star-Wars clips, direct track, play, stop, repeat and volume separately; all remain non-clickable. | Only the local BUSY input is exposed to game logic. UART acknowledgement and typed error are not available. | Confirm the as-built wiring and SD inventory, define the bounded catalogue/direct range, extend the driver without conflating BUSY/UART/error, then bind it behind the productive grant. | No audio request is loaded; no sound or SD observation was made. |
| Audio BUSY, UART and error status | `DFMinniHandler::isBusy` reads BUSY LOW; the driver does not expose UART result or a typed failure channel. | Three distinct readiness actions prevent a single boolean from being presented as complete evidence. | No diagnostic reply can currently distinguish accepted command, UART acknowledgement, BUSY transition and failure. | Add independent fields and timeout/error semantics plus host/replay tests before a UI verdict exists. | No status was requested or measured. |
| NFC phone-write → box-read, reapply and byte comparison | `NFCReader` is a read-only UID adapter; bounded PN532 frame/adapter host tests exist, but no productive raw/tag-data diagnostic port or stable owner is installed. | The debug NFC capability host records phone-side development evidence only. It does not command the box or assert device correlation. | There is no correlated full-payload box read, removal/reapply event or byte comparison. | Stabilize PN532 startup, select supported tag/type/capacity and safe test payload, then add bounded reads behind the productive role. Keep authentication tags and test data isolated. | PN532 startup was previously intermittent; no tag was presented in this session. |
| NFC box-write → phone-read, explicit confirmation, reapply and byte comparison | No product write operation exists in `NFCReader`; no protect/verify sequence or productive owner is present. | Android can host local NFC development capability, but no UI sends a box write or treats a phone read as box proof. | No write acknowledgement, read-back, independent phone read or byte comparison exists. | Specify allowed test pages, tag protections, one-shot/unclear-outcome behavior and explicit device read-back before enabling. | No write was requested or attempted. |
| Matrix raw press/release, live highlight, scan error and 81 points | `KeyMatrix::readPressedPositions` returns current positions, while `MCPHandler` maps read failure to HIGH and therefore cannot surface a scan error. No diagnostic adapter emits press/release edges. | These five actions are individually registered but non-clickable. Raw coordinates must remain immutable evidence. | No typed raw event, release, scan-valid/error or complete 81-point ledger is available. | Correct/verify the hardware profile first: matrix row 1 currently maps to output-only GPB7 and boot directions conflict with scanning. Then add raw events and explicit scan validity. | No matrix command is loaded and no key was observed. |
| Matrix rotation, mirror, multi-point suggestion and confirmation | No firmware transform is needed or allowed; firmware must report raw coordinates only. | `MatrixOrientationV1` implements all eight display transforms separately from raw coordinates. `MatrixOrientationResolverV1` ignores unconfirmed references, requires at least two confirmed points, reports no-match/ambiguous/unique, and recomputes a unique suggestion from the confirmed points before allowing explicit confirmation. The readiness view labels this app-domain path as tested while the device path remains closed. | Six JVM tests cover all 81 cells under all eight transforms, multiple confirmed references, contradiction, confirmation gating and rejection of unconfirmed references. This is display-domain evidence, not matrix evidence. | Wire this domain to a future live raw-event UI and durable setting only after the raw adapter and hardware correction exist. More points may be required when a two-point set is symmetric. | No raw coordinate was received; no orientation was suggested or saved for the box. |
| Wake raw levels, edges, single inputs and combinations | `diagnostic::Control::edge` and `finishWake` plus `StepTransport` implement the bounded v1 owner against an `IPort`; only fake/host ports provide the component owner. No MCP/wake adapter is installed in product startup. | The pilot uses `FakeDiagnosticRepositoryV1` and a synthetic Step exchange. Four separate future actions are non-clickable. | The v1 Step reply preserves raw level, interpreted active state, channel, time and overflow; host reassembly is byte-tested. | Authority and component ownership are absent. Wake 1 maps to MCP23017 GPB7, which cannot be an input. HIGH-active interpretation with pull-up also requires raw and interpreted values to stay separate. | No executable Wake request is loaded; no live Wake observation was made. |
| ESP-NOW identified packet, short range, operational range and receiver report | `EspNowCommunicator` sends the game enum to fixed peer `B0:A7:32:F1:86:54`; `EspNowRuntime` counts local delivery callbacks. Neither carries a diagnostic run/packet ID or controls the receiver report. | Four readiness actions are registered but non-clickable; there is no receiver import/replay UI. | Local send callbacks cannot prove receiver processing. No durable receiver evidence correlates run, packet, MAC, channel and receipt. | Define a versioned test packet and receiver record, pin/observe the channel, preserve Wi-Fi/BLE coexistence evidence, and replay-test report reconciliation before enabling. | No packet was sent. Current passive counters remained 0/0. |
| Three cold starts and longer combined run | Normal boot and runtime loops exist, but no productive diagnostic orchestrator owns all components or resumes a correlated run. | The current pilot is only a two-step synthetic preview. Cold-start and endurance actions are non-clickable. | No complete run ID survives device/app restart and no combined component evidence exists. | All component paths, authority, ownership and restart semantics must be implemented first; the user then performs these actions in the app. | No restart/endurance test was started. |
| Open errors, user observations and complete report export | `DiagnosticGattsService` and the typed Step transport can carry bounded replies for the existing fake slice. There is no complete-device report producer. | `HardwareDiagnosticEvidenceV1` registers all 48 actions and holds multiple bounded attempts per action, each with a unique request identifier. It separates requests, explicitly **unverified local** acceptance/execution claims and user observations. Its canonical, checksum-protected report is immutable after finish. `DurableHardwareDiagnosticRunV1` writes each local intent to a journal before returning it for any caller to send, retires pending/accepted-but-unfinished attempts as unclear after restart, never resends them, and keeps a finalized run read-only. `AndroidHardwareDiagnosticJournalStoreV1` supplies private atomic files but is **not yet connected to the UI**. The existing `CreateDocument` UI still exports only the synthetic pilot; a complete hardware report export remains blocked. | Seven ledger and five journal JVM tests cover repeated matrix/cold-start attempts, correlation, unclear-write protection, journal failure, recovery, canonical replay, false provenance rejection and immutable finish. The checksum detects accidental changes but does not authenticate a device. | Add an authenticated reply type bound to device, boot, grant, connection and original request; connect the journal to the productive guided plan, Android recovery and export. An unclear write cannot be repeated in the same run; no protected device command is enabled by this journal. | Only JVM export/replay was tested; no complete phone report was produced in this session. |

The app registry gives every required action above a stable identifier and
state. The four display-orientation actions plus open-error retention and
separate user observations have tested, device-independent domain paths. The
report serializer is a draft with unverified local claims; the full export
action remains blocked. All device actions remain blocked rather than falling
through to the synthetic pilot.

Consequently there is no safe firmware candidate to upload and no valid
install/connect/test sequence for the complete user-run test. The APK must be
used only to inspect the blocked readiness view and the existing explicitly
synthetic two-step preview.

## Software verification

- Firmware application/regression: 10/10 and 20/20 host cases passed.
- Diagnostic owner, control transport, step transport and GATT route: 426,
  350, 968 and 26 assertions/checks passed with fake/host ports.
- Diagnostic JSON/replay contract: 22/22 tests; eight golden reports and the
  companion fixture suite passed.
- NFC capability, PN532 frame, PN532 adapter and APP_MUTUAL development
  transcript: 1, 4, 6 and 1 host cases passed.
- Android: 110 core protocol and 77 app unit tests passed; Debug APK build
  succeeded offline.
- Android lint did not complete because
  `androidx.collection:collection-jvm:1.4.0` was absent from the offline cache.
  Unit tests and APK assembly had already succeeded in that run.

All test results above remain software-only. The phone was connected and the
earlier `B1BF72...` diagnostic APK was installed after explicit approval. The
revised `986CF0...` and subsequent `D8F8FA...` builds were not installed because
the phone disconnected.
No app workflow was exercised, no firmware was uploaded and no user hardware
run was started.
