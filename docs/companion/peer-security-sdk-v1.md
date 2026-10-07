# Original SDK security evidence (ESP-IDF4.4.6)

Status: narrow firmware/tool implementation, host checked; production pairing,
wire authentication attributes and device/RF acceptance remain open. Base9df1d36.
No configuration/proof wire format or Appcode changed in this cut.

## Origin and grant

One process-lifetime selected source is armed before Bluetooth initialization.
Only the isolated SDK target wraps SMP_Register, retaining the existing core
callback. Every event, address/data pointer and result is forwarded unchanged.
The original BTU/source actor captures start events1/2 and the first5 bytes of
terminal event7 **before** BTM/BTA/BTC forwarding. It never signs, reads NVS,
allocates an input buffer or grants rights in the callback. Registration once;
failure/retry/SDK instance replacement stays closed. No SDK patch or private
security-record prefix cast. No late joining of queued GAP events by MAC/handle.

PeerLinkSecurity owns one fixed pairing attempt: device, exact connection token
and packed handle, six original address bytes, registration, interface and
lifecycle. An original start is mandatory. Completion success0/security level4,
no cancel/BR, resolved SC8+MITM4+bond1 (optional keypress16) are required. Requested
SC or the encrypted/authenticated flags alone cannot prove the selected mode.
Original SMP code computes the selected association model; terminal assembly may
also use stored ble.auth_mode. The reader does not independently validate bond
persistence. Missing original start/reencryption-only restoration stays closed.

A repeated start cannot extend the 30000-ms absolute attempt deadline. Re-pairing
invalidates prior mode. Disconnect or expiry while an attempt is pending retains
an abandoned attempt until its original-address terminal callback is drained.
No new attempt can overwrite it, even on a recycled handle/address. If that
terminal never arrives, admission remains closed for this source lifetime;
restarting a live SDK is not a supported recovery. This conservatively handles
the fact that SMP callbacks expose no application attempt token.

The sole source actor calls readOriginalPeerSecurityNow at grant on a frozen
trusted proof binding. It checks actual GATT connection/interface/LE transport
and address, no current security procedure, BTM flags authenticated/encrypted/key
known, LE state encrypted+MITM and 16-byte key, then no security procedure again.
Every port is outside the mux; original lifetime, policy, published authority and
mutable address bytes are rechecked between ports. No publication on false;
output remains unchanged. A failed/re-pair original SMP result rejects the exact
owned IdentityAccess link before forwarding and preserves another publisher.

The reader binds only transport evidence. Cryptographic key possession, captured
boot/proof freshness and current protected credential must be checked separately
before/after this read. The native tool case copies a real verified proof from
the separate crypto model worker; the source actor rechecks original context,
credential and proof deadline, combines evidence and authenticates. A reader
success alone grants no GM role. Stored explicit role assignment is mandatory.
A production asynchronous verified-proof handoff and captured boot context are
still open; the mutable crypto owner is never read across workers in this tool.

## Independent SDK evidence

scripts/check_peer_security_abi.py pins11 official ESP-IDFv4.4.6 sources and
extracts the actual complete scalar types,5 field offsets, callback and5 C API
signatures. An independent compiler executes original smp_get_auth_mode for10
association models (legacy/SC JustWorks/SC MITM/out of range). Completion size5,
alignment1; opaque union pointer only, no whole-union allocation/layout claim.
Resolved-to-terminal dataflow is checked in the pinned original source. Installed
SDK libbt.a and libmbedcrypto.a remain guarded by the existing build pins.

Inputs from official https://github.com/espressif/esp-idf/tree/v4.4.6/components/bt
are separately downloaded, checked against the official recursive Git tree blob
SHA1 and pinned SHA256. They are read-only external inputs, not edited SDK files.
Run:

```text
python scripts/check_peer_security_abi.py --source-dir <official-security-source>
  --gatt-source-dir <official-lifecycle-source> --compiler <independent-g++>
python scripts/check_peer_security.py
pio test -e host-configured-control
pio test -e host-peer-proof
pio run -e esp32-gatts-abi-probe
```

Native environment: audited official MbedTLS2.28.4 include/library sources and
pinned archive via LARP_PEER_MBEDTLS_DIR/LARP_PEER_MBEDTLS_ARCHIVE. Independent
record/P-256 counterpart uses cryptography45.0.7. No fake crypto fallback.

## Public vertical observation

fixtures/peer-security/observation.json is emitted by exactly
`test_security_actual_crypto_original_smp_metadata_frames_game_and_revoke` with
LARP_EXPORT_PEER_SECURITY_FIXTURE=1; normal native tests reread it byte-for-byte.
One original registration, three SMP callbacks, five same-actor SDK metadata
reads, six actual MbedTLS proof frames, thirteen original command/ATT frames,
stored GM restore and confirmed revoke. Actual game runs and remains running
after old proof/route rejection. SDK callbacks, tasks and NVS are explicit host
models. Independent Python counterpart correlates the observation with three
public registry records/proof metadata and rejects53 malformed correlations.

Public generator key is a test fixture, never commission it on a real device.
RF/SMP timing, actual pairing UI, entropy, physical bond/NVS durability, mutually
authenticated server identity and production startup remain open. Existing bond
reencryption, pre-GM challenge/proof/result attributes, original output/CCC,
live security loss on all authenticated traffic and application adapters must
be added in small vertical paths. No board upload or COM access in this cut.

## Resource scope

The kernel and callback use fixed byte copies. No own heap or NVS in SMP/source
reads; SDK internals are separate costs. Source actor exclusively owns the kernel.
Mux sections only copy the fixed policy snapshot; SDK calls occur outside.
30000 ms is a cancellation policy, not measured pairing latency. Final compiler
sizes/frames/builds are recorded separately in the project Werkbank. The closed
SDK probe links the reader and wrappers but starts no SDK, pairing, radio or NVS.
Separate target build sizes do not prove a combined production reserve.
