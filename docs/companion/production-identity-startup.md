# Production companion identity startup

`src/main.cpp` owns one process-lifetime `Esp32CompanionIdentityOwner` and calls
`begin()` before Wire, device initialization and radio startup. The existing
startup entropy adapter temporarily uses the boot entropy resource; initialization
must stay before RF/ADC/I2S/device workers. A present, valid public identity is
loaded without entropy or writes. A missing record is created exclusively and
read back before it becomes visible.

`ReadyClosed` means only that the confirmed public identity has initialized the
private `IdentityAccess` policy. Authority remains unavailable, and there is no
connection, authenticated peer, game-master assignment or diagnostic session.
The owner exposes copied read-only observations and no mutable policy or channel.
All calls belong to the startup owner; the state and public-ID getters are not
cross-worker synchronization APIs.

Startup has one attempt per owner, including failures. Storage failure,
corruption, zero entropy and uncertain persistence never publish a candidate ID,
erase NVS or automatically retry. A companion startup failure is logged and the
existing standalone game startup continues. A new boot may load a previously
uncertain write after the complete record has become verifiable.

## Required before BLE and diagnostics can be enabled

This class does not start Bluetooth, advertise a service, register GATT callbacks
or create a boot identity for requests. It is the closed identity substrate for
the production owner, not a connected transport.

The next integration must provide a single actual SDK actor and worker ownership
for the existing GATT source, service, peer registry and proof paths. It must
establish callback provenance before SDK startup and transfer the initialized
identity policy to that actor without leaving concurrent mutable access from the
main loop. Persistent peer credentials and roles must be restored through the
confirmed registry path. Neither a public device label nor a BLE address grants
authority.

Bond reconnection additionally requires original connection birth, current
encryption state, and proven stored MITM/secure-connections/bond mode; a generic
SDK authentication-success callback is insufficient. The F1 SDK provenance work
alone does not supply that complete evidence. Production F2 owner wiring and
current role/authority binding remain prerequisites for diagnostic admission.

The diagnostic transport must use its own original-bound request path and revoke
the test session on disconnect, authority change, timeout and reboot. Only then
can the Android-owned wake/LED diagnostic step connect to real hardware owners.
No `GameCommand` extension, GPIO mapping, upload or physical acceptance is part of
this startup substrate.

The attempted closed registration and its measured product size gate are
documented in `production-ble-closed-owner.md`. Bluetooth remains unstarted.
