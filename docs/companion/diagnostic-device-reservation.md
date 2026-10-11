# Closed device reservation for diagnostic integration

The sole application owner may call `devices::Runtime::reserveDiagnostics`
only while the configured game is stopped and the configuration is ready.
The expected configuration revision must match. A paused game is still running
and cannot reserve devices. This is a trusted internal API, not a BLE operation
or a permission check.

Reservation immediately blocks game start, ticks, state changes and profile
activation, before any owner callback or asynchronous Park operation. It hides
device availability and captures the original configuration revision and device
generation. The existing copied control channel then receives one correlated
Park request per owner from `DeviceRuntime/Plan.h`: NFC, I2C (including matrix
and wake inputs), audio, LED and peer. Each reply must match the full ticket,
including generation, sequence, owner and operation. At most one control post
and a bounded reply drain occur per service call.

Only five successful replies followed by `IChannel::quiescent()` reach
`ParkedClosed`. Empty queues alone do not satisfy that barrier: no executing
job may remain. An owner rejection/failure, expired operation/transaction or
failed barrier latches `Fault`. Stale replies cannot finish another operation;
late replies cannot clear a fault. Unsigned elapsed-time checks cover the
32-bit clock wrap. The configured runtime's existing 5-second per-operation
and 60-second transaction bounds apply to parking, not to LED output duration.

Both `ParkedClosed` and `Fault` permanently block this runtime instance. There
is deliberately no grant, render, resume, release or fault-reset API in this
substrate. `diagnostic::IPort` is not connected to it. Thus even
successful parking does not claim a safe LED state, suppressed physical inputs,
a working off timer, a visible test indication or diagnostic authority.

## Missing production contracts

| Concern | Existing code path | Contract still required |
| --- | --- | --- |
| Actual game ownership | `src/main.cpp` calls `LarpEscapeApp::update`, which runs the legacy `ReichstagGame` directly. It does not own a configured `DeviceRuntime::Runtime`. | One application arbiter must control both the actual game start/tick path and diagnostic reservation. A configured Runtime fence alone does not stop the legacy machine. |
| Asynchronous acquisition | `diagnostic::IPort::acquire` returns one synchronous boolean and a grant identifier, whereas owner parking takes several service cycles. | Define pending acquisition and cancellation bound to the original run, connection and authority, or a separately prepared handoff. A successful reservation or posted Park is never a successful acquire. No adapter may leave a half-acquired grant after authority changes. |
| Owner mapping | `DeviceRuntime/Plan.cpp` maps Matrix and IoExpander to I2C and LedStrip to LED; this compiles a declaration. | Actual workers must implement the correlated Park/Resume contract. `OwnerConfiguration` and synthetic host owners do not prove driver completion. |
| Wake input | `KeyMatrix::readWakeSignals` returns a four-boolean snapshot directly to the legacy game. | A sole I2C owner must publish timestamped raw edges with confirmed polarity, retain all channels, swallow test input from the game, flush stale events and require a fresh neutral baseline before returning ownership. No pin or polarity is inferred here. |
| LEDs | `GameLightEffects::renderLoop` continuously drains effect commands and calls `renderFrame`/`FastLED.show`. | Correlated park acknowledgement after old effects, brightness ramps and queued commands are stopped; safe output ownership and a confirmed all-off result. A queue admission acknowledgement cannot prove off. |
| 500-ms off bound | `diagnostic::Control::tick` calls off on its first invocation at or after `startedDeviceMs + 500`. `IPort::render` has no deadline/completion acknowledgement. | The LED owner must arm and verify a local bounded off mechanism before illumination, independent of BLE/App/Control tick progress, and define timer failure, render latency and completion evidence. The current cooperative tick supplies no hard 500-ms bound. |
| Off failure | `Control::off` latches cleanup failure; `close` retains ownership after failed off. | A product release protocol must require correlated confirmed safe-off and drained inputs, preserve the fence on unknown outcome, and specify explicit recovery. The closed reservation provides no recovery shortcut. |
| Visible test state | Neither `IPort::acquire` nor current production owners acknowledge an indicator. | Device-confirmed test mode with run/grant/connection correlation must be visible in the Android flow before a step. Define indication loss and teardown; do not repurpose an unverified LED or GPIO. |
| Permission | `Esp32CompanionIdentityOwner` initializes a public identity with unavailable authority. | Original SDK connection, current verified role/authority, and revocation serialized with the arbiter; no permission from the reservation ticket. |

The next vertical integration must supply those contracts and actual worker
acknowledgements before adding any transition out of the closed states. Resetting
software state alone is not evidence that output became safe. No upload, physical
acceptance or 500-ms timing measurement is provided by this package.

## Light-worker handoff slice (11 October 2026)

`GameLightEffects::requestPark(ticket)` is a one-way, internal fence for the
actual FreeRTOS light worker. It rejects zero and duplicate tickets, closes
game-command admission, ignores commands already queued or racing with the
fence, and stops effect drawing. On its next worker cycle it fills the existing
LED buffer with black and calls `FastLED.show()` once. The worker exposes only
`offFrameSubmittedFor(ticket)`, correlated to the original request. This is
evidence of a software frame submission, **not** an owner Park success or a
confirmed physical all-off state. There is no release or diagnostic render API.

This slice is not yet called by the application arbiter or mapped to the
`DeviceRuntime` LED owner. The running render call can finish after the request;
there is no measured bound on when the black frame reaches the LEDs. FastLED's
return does not prove the LED controller accepted it, and no independent off
timer, supply/level check or user observation exists. Consequently no caller
may turn `offFrameSubmittedFor` into `ControlOutcome::Completed`, grant
diagnostic LED ownership or advertise a 500-ms safety guarantee. GPIO15 is the
user-confirmed data connection of the real box; LED type, level and supply
remain unverified. The next gate is an owner-specific, independently bounded
and physically checkable off mechanism plus a correlated Park reply.

### Integration stop gate checked on 11 October 2026

The separate fake-owner commit `ecec6c3` adds `IPort::armLedOffTimer()` and
`timedOffConfirmed()` to `CompanionIntegration/DiagnosticControl`. Its host
tests prove that a fake independent timer must expire and confirm off before a
LED step can finish. It adds no ESP32 timer, LED driver acknowledgement or
production `IPort` implementation. The `GameLightEffects` worker in this
branch supplies only the weaker, sequence-correlated black-frame submission.

In the actual product start, `src/main.cpp` constructs `ReichstagGame`,
`LarpEscapeApp` and `GameLightEffects`. It constructs neither
`DeviceRuntime::Runtime`/`FreeRtosProfileChannel` nor `diagnostic::Control`.
The application preflight/Standby fence only stops future game-loop calls.
Consequently there is no common production arbiter to serialize that fence,
the worker Park ticket, the timer and the original BLE authority. Splicing a
synthetic `ControlReply{Applied}` into the unused reservation queue would
misrepresent physical ownership and must not be done.

**Required before implementation of an `Applied` LED Park reply:** choose and
verify a real off mechanism that can expire without progress in the app,
BLE callback or light render loop; define what driver or electrical evidence
confirms the output is off and how failure remains latched; specify the LED
type, supply, level and switching path of this box. The production arbiter must
then atomically fence the actual app loop, request Park on the sole light
worker with its full generation/sequence/owner ticket, await the matching
worker submission and independently confirmed off state, and only then send a
matching `Applied` reply. A timeout, stale ticket, timer failure, unknown off
state or authority change must leave the game and diagnostics fenced. The
timer evidence must be bound to the same owner generation and must not be
reused by a later request. This is an implementation gate, not a successful
hardware test or a grant. No source in this branch satisfies it yet.

## Closed legacy game preflight

`LarpEscapeApp::fenceUnstartedGameForDiagnostics()` now gives the actual
`src/main.cpp` application loop a one-way preflight fence. It can succeed only
before the first `update()` and must run on that loop's sole owner. Once set,
`update()` performs no audio service,
communication service, game machine tick or snapshot publish. A request after
the first update fails and does not change the running game. Host tests exercise
this exact application class and the real `update()` call order.

This fence is currently an internal seam with no production caller. It does not
park the already initialized LED, audio, matrix, NFC or radio workers, and it
does not connect to `Runtime::reserveDiagnostics` or grant diagnostic ownership.
The production arbiter must coordinate this fence with actual worker park
acknowledgements and the original connection/authority before any test step.

The same loop owner can also call
`LarpEscapeApp::fenceObservedStandbyAppLoopForDiagnostics()` after two consecutive
published Standby snapshots without a retained role or reported busy audio.
The second tick lets the selected Standby state perform its pending entry action.
This permanently stops subsequent `update()` IO; a game in another phase is
rejected without changing its operation. Standby and `audioBusy == false` are
only game observations. Neither means that the LED render task, queued light
effects, DFPlayer hardware, matrix reader or radio callback is parked. There is
no correlated worker Park/Ack API on this legacy path, so no physical quiescent
result or transition to diagnostic ownership can follow this loop fence alone.
