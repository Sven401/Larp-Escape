# Closed diagnostic startup integration (2026-10-11)

## Current integrated gate, 2026-10-11

The closed diagnostic GATT registrar `b0e4913` is integrated at `4968e38`.
It installs the versioned Control, Step, two Reply and two CCCD attributes
on the original GATT registration. Control, Step and CCCD writes remain
closed with no diagnostic owner or grant. Advertising waits for both services
and the closed registry acknowledgement. On the combined tree, configured
control host checks passed 533/533; source and service model checks rejected
17 and 58 negative cases; the route contract passed 26 checks. The ESP32
single-app build succeeded with 1,974,053/2,621,440 B ELF flash and
76,572/532,480 B static RAM. This image has **not** been uploaded.

The Android report durability commits `f80440c` and `08e1f61` are also
integrated. Ten targeted JVM tests and the offline Debug build passed. The
resulting APK was installed on Moto G14 `ZY22J7CSQD`; its built and pulled
SHA-256 both equal
`79bd30ead3751f8c1d904ef533a374e595535b87d5f63dd648ab71519794ca37`.
Codex did not open the app or run pairing. The box still runs the previously
approved BLE startup image, not this new registrar build. A user-exported
pairing report, F1 role proof, confirmed physical off state and productive
diagnostic Control/Step/Reply admission remain open. F1 negative follow-up
`57c7dec` stays on its separate branch and grants no role.

Status: local build and host checks only. Branch
`plan/nutzergefuehrte-hardwarediagnose-startintegration`, based on the
device-gate commit `59729500a276519e218a2c0d825849162e6e58bc`.
The five BLE/layout commits from `d677729` through
`e876dfbecc36b4a68de4cbff895deeda67d2e31a` were applied in order to
this isolated branch. The Android transport branch through `268f008` and
the diagnostic GATT route branch through `c57951a` were merged locally after
their own tests. None of the source branches was rewritten. The F1 negative
and ABI probes remain separate at `c7843bf`; they confer no role authority.

The product `main.cpp` now owns one `ClosedDiagnosticArbiter` for the actual
legacy game loop and one process-lifetime `Esp32LedParkDeadline`. An internal
one-way handoff first fences game updates, then arms an ESP timer and requests
the light worker to park under the same ticket. At 400 ms the timer wakes a
separate higher-priority off task, which attempts a black FastLED submission
under the shared driver mutex. Completion after 500 ms, mutex failure, timer
failure or task failure latches a closed fault. The existing light worker's
black submission and the separate timed submission are distinct software
observations. Neither confirms electrical or visible LED off. No production
caller requests the handoff; the arbiter has no grant or release path.

The single-app development variant also constructs the product GATT owner
against the closed identity policy. SDK binary pins, callback hook ordering,
service worker, GATT registration and the final image are compiled. It starts
no advertising, verified connection, saved role, F1 reconnect admission or
diagnostic endpoint. `diagnostic::Control` and the configured device owner
still use fake/host ports; a black submission must not become an `Applied`
Park reply, `restoredOff`, a step result or a permission to run a test.

Verification before the Android and diagnostic GATT package merges:

- `pio test -e host-regression -f test_application_loop`: 10/10 passed.
- `pio test -e host-regression -f test_regression`: 20/20 passed.
- `pio run -e az-delivery-devkit-v4-single-app-capacity`: SUCCESS, pinned
  GATT and peer-crypto binaries verified; ELF flash 1,952,485/2,621,440 B,
  static RAM 73,300/532,480 B. `firmware.bin` 1,958,784 B, leaving
  662,656 B by binary length, SHA-256
  `2272461C87F7DD74DA77146AF1CBDADCE5406156FBC425C891499295969876A7`.
  The first build attempt stopped during local FastLED installation in the
  long OneDrive path; the successful run reused the already installed pinned
  3.10.5 library from the BLE source worktree. No source workaround was made.

Combined verification after merging Android and the diagnostic GATT route:

- Diagnostic GATT route 26/26, control transport 350/350 and step transport
  955/955 host assertions passed. The first attempts under the restricted
  Windows process sandbox failed to start the local C++ compiler; all three
  passed when executed with the normal local compiler permissions.
- `pio test -e host-regression -f test_application_loop`: 10/10 passed;
  `pio test -e host-regression -f test_regression`: 20/20 passed.
- `pio run -e az-delivery-devkit-v4-single-app-capacity`: SUCCESS, pinned
  GATT and peer-crypto binaries verified; ELF flash 1,952,485/2,621,440 B,
  static RAM 73,300/532,480 B. `firmware.bin` 1,958,784 B, leaving
  662,656 B by binary length; SHA-256 of this combined build:
  `5A1F535D524A1960A07DC72A82FFB02B438637D24CEE86C8CCFE4EBA5362A939`.
- Android targeted `DiagnosticGattLinkV1Test`, `DiagnosticGattPreflightV1Test`,
  `DiagnosticControlV1Test` and `DiagnosticStepV1Test`, plus
  `:app:assembleDebug --offline`: SUCCESS on the combined tree.

The versioned diagnostic GATT UUIDs, MTU >=131 and full-origin reply fence
are now shared by product code and Android. The firmware deliberately does
not register those diagnostic attributes: no production same-birth F1/GM
role owner, serialized diagnostic component owner, confirmed LED off state or
SDK request/reply path exists. Android discovery therefore remains blocked;
its production authority supplier is null, so no diagnostic write can occur.

Open gates: independent physical all-off evidence and LED type/level/supply;
actual owner Park acknowledgements; original current GATT/F1/GM authority;
diagnostic request and reply attributes with an effective ATT value capacity
of at least 128 bytes; trusted Android authority and guided UI bound to those
attributes; real bootloader behavior of the single-app map with old OTA data;
runtime heap, stack and timing on the connected box. The test function has no
physical or product release. No upload, restore, RF run, device test, push,
PR or integration into `main` was performed. The next hardware run belongs
to the user in the app; a later session may evaluate only its exported report.

## Closed follow-up, 2026-10-11

Commit `db814e4` makes the fake diagnostic step owner demand an independent
`timedOffConfirmed()` observation before an `Applied` park reply. A missed timer
and a mere black submission stay closed. Host checks passed: diagnostic control
16 scenarios/426 checks, control transport 350 assertions, step transport 968
assertions. The real ESP32 timer still observes only its own software off
submission; there is no electrical or visual off feedback on the current board.
The firmware therefore has no production diagnostic grant.

Commit `c9f21bd` generates a fresh, process-local `BootIdentity` during closed
identity startup. Failure leaves the owner unavailable, and it never rewrites
the persisted device identity for a failed boot nonce. Configured-control host
tests passed 521/521. The single-app ESP32 build passed with 1,952,569 B ELF
flash and 73,308 B static RAM against 2,621,440 B and 532,480 B respectively.
This boot identity is not yet bound to peer proof, a registry or GATT. The
original-bound startup prerequisites are recorded in
`production-ble-closed-owner.md`.

Android commit `8c8bd9f` adds a visible 11 October preview label and an
explicit message that the box test is still blocked and responses are
simulated. Its offline Debug build passed. The APK was installed on the
connected Moto G14 after the previous same-signature APK was backed up; the
installed APK hash matched the new build. The app was not opened or operated
by Codex, and the user has not yet confirmed the screen or run a box test.
This APK install was not a firmware upload. No firmware upload or restore has
occurred.

The separate F1 package `d3319f7` pins the original controller-result gap:
Encryption Change carries status, handle and enabled, with no command, key or
GATT-birth identity. A reused handle or second key can produce identical event
bytes. The private security-record ABI remains incomplete. Its source checks
and ESP32 ABI-probe build passed, but it grants no restored role.

Android availability package `e7af181` was merged locally after its targeted
JVM tests and offline Debug build passed. A separate `ESP · BLE` view now shows
the actual connection/discovery/MTU/subscription block reason; the simulation
has its own view. The merged tree's targeted availability test and offline
Debug build also passed. The resulting APK kept package
`de.larpescape.companion.diagnosticpreview`, versionCode 1 and the same debug
signing certificate. Before installation on Moto G14 `ZY22J7CSQD`, the
previous installed APK was backed up and verified as SHA-256
`2703E2BD15405E4DEF5920E2683AFE5B7DC757E746F19D29913371E75CFD5D22`.
The new APK installed successfully; the pulled installed APK matched the
built SHA-256
`6A926F339DD8F96730331F03ABC08CA42EE4E48E7D91D2775BC0BB7651301EB7`.
The app was not opened or operated by Codex. Its device screen and any box
behavior still require the user's own observation and later exported report.

## BLE pairing candidate and installed app, 2026-10-11

The closed pairing firmware package `8347d7c` is integrated at `14222a5`.
It exposes the original service and connectable advertising only after the
closed registry owner acknowledges readiness, and requests an Android OS bond
with `IO_CAP_NONE`. It does not claim MITM protection, GM/F1 role, or a
diagnostic grant; sensitive GATT writes remain closed. The integrated
configured-control checks passed 526/526, application and regression checks
30/30, and the ESP32 single-app build succeeded. The candidate app image is
1,974,176 bytes with SHA-256
`375854ad53113a035e6853f9b14346892d95f40fa10445e77c213d1643d2b522`.
It remains **not uploaded**. The chip-bound current-flash capture and exact
three-region upload/readback plan are in `ble-pairing-preflight.md`.

The Android pairing-report package (`6d06149`, `b676bc0`, `ce1ffec`) lets the
user scan, connect, request OS bonding and export observed results on `ESP ·
BLE`. An `ANDROID_PAIRING_OBSERVED` label requires same-generation connected
GATT, the product service and an OS bond for the same address. Missing
firmware-side security evidence remains explicit. The targeted pairing test
and offline Debug build passed. The resulting APK was installed on Moto G14
`ZY22J7CSQD`; the pulled installed APK matched the built SHA-256
`1260c588c8b8d01afe4c579c62ea1fa38d66dc98f3ac534e3e8f7c6b38623c36`.
Codex did not open the app or run the pairing flow. The user has not yet
observed a live app–ESP link or exported a report.

The user separately approved the exact first firmware candidate after the
chip-bound current backup and rollback dry run were checked. Commit `14222a5`
was written in three narrow regions; a full 4-MiB readback matched all images
and preserved bootloader, NVS, SPIFFS and coredump. The postflash SHA-256 is
`38682f775602b128c1e706d3d3b60d60d237596cdcbbdfd8cd10f2a6c0f2ecae`.
The box started its game (`Runtime ready`), but reported `Closed GATT
registration state: 4` (`StackFailed`) and discovery phase 0 (`Closed`). BLE
pairing is therefore blocked on the real box. See `ble-pairing-preflight.md`
for the exact write/readback evidence. No restore or user-led pairing run
occurred; a corrected image requires a new preflight and upload approval.

The separate F1 package documents why the controller Encryption Change event
cannot bind a selected key to a fresh GATT/command birth. Its report-gap
follow-up `c323f90` explicitly distinguishes the original link, Android OS
bond, fresh SMP completion, and live-link security. The latter two are not
available in the present product report. No restored role is asserted.

## Real BLE startup follow-up, 2026-10-11

The first uploaded image booted the game but failed before BLE registration.
A separately approved telemetry image, with full readback, narrowed this to
`esp_bt_controller_init` error 259. The pinned Arduino startup had released
BT memory before `setup()`. The BLE package `6407199` retains that memory
through a product-specific `btInUse()` implementation and aligns controller
init/enable to BLE mode. Host checks 529/529 and the integrated ESP32 build
passed; the ELF map selects the product implementation.

The user separately approved this third app-only upload after the existing
current full readback and chip-bound rollback dry run were checked. A newly
attempted full capture had proved byte-identical to that readback and its
redundant file was removed. The third upload's app sector was read back
exactly, including erased padding, and NVS/otadata matched the rollback
source; no new full postflash capture was made. Serial now shows game
`Runtime ready`, GATT registration pending and BLE advertising `Visible` on
the box. No pairing was run by Codex or observed yet. Role, diagnostic grant,
physical LED-off evidence and product acceptance remain closed. The user
operates the installed app's pairing flow and exports its report before any
actual pairing claim.

## Product status after the user-run BLE return-path check, 2026-10-11

The user subsequently paired the Android app to the original box, and a
separate user-run app attempt received both ControlReply and StepReply
transport probes on one GATT generation. Direct USB reads of the app's
persisted journals are preserved and interpreted in
`ble-pairing-preflight.md`. The loaded box firmware contains the narrow probe
sender from `56b63bd`; the Android probe client was installed and its APK
hash checked. This establishes the BLE notification return path only. It
does not establish any authenticated diagnostic command or physical result.

Commit `1410938` is a **local, not uploaded** product-start package. It
constructs the existing F0 PeerAuth graph after a valid boot identity,
starts its worker after original GATT source registration, and gates
advertising on the worker's closed readiness. The empty registry, current
`ESP_IO_CAP_NONE` bond policy, MITM-protected writes and identity gates remain
unchanged. The configured-control host suite passed 541/541 and the ESP32
single-app build passed (1,993,693/2,621,440 B flash;
80,436/532,480 B static RAM). These are build and host findings, not a role
or device proof.

Hardware review found no verified input, display or OOB path on the original
box for BLE MITM. Android `OS_BONDED` and a working notification transport
therefore cannot satisfy F1 or Game Master authority. Protected diagnostic
writes and the independent LED-off gate stay closed. A security contract
decision and its separate implementation/tests are required before any
positive role or diagnostic access can be claimed. Product hardware testing
and release remain open for the user-led app flow and a later evaluable
end report.
