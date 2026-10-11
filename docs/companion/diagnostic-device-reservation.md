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
