# Closed diagnostic startup integration (2026-10-11)

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
