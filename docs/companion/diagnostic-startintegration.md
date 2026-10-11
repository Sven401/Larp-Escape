# Closed diagnostic startup integration (2026-10-11)

Status: local build and host checks only. Branch
`plan/nutzergefuehrte-hardwarediagnose-startintegration`, based on the
device-gate commit `59729500a276519e218a2c0d825849162e6e58bc`.
The five BLE/layout commits from `d677729` through
`e876dfbecc36b4a68de4cbff895deeda67d2e31a` were applied in order to
this isolated branch. Neither source branch was rewritten. The Android
transport gate `d80956d` and F1 negative check `0cfe160` remain separate.

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

Verification on this exact integration HEAD before this document:

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

Open gates: independent physical all-off evidence and LED type/level/supply;
actual owner Park acknowledgements; original current GATT/F1/GM authority;
diagnostic request and reply attributes with an effective ATT value capacity
of at least 128 bytes; Android transport and guided UI bound to those
attributes; real bootloader behavior of the single-app map with old OTA data;
runtime heap, stack and timing on the connected box. The test function has no
physical or product release. No upload, restore, RF run, device test, push,
PR or merge was performed. The next hardware run belongs to the user in the
app; a later session may evaluate only its exported report.
