# APP_MUTUAL_V1 — contract for USB-enrolled app and box identities

**Decision:** On 2026-10-11 the user selected individually USB-enrolled app
and box keys, with USB revocation of a lost phone, for the Originalbox. This
document is a **design contract**, not an implemented protocol or a grant.
The product remains on its current closed BLE/GATT permissions and has no
enrolled APP_MUTUAL_V1 keys. No flash, eFuse, Android Keystore, role, or
diagnostic permission is changed by this document. See the [decision and
evidence](diagnostic-auth-options-v1.md).

## Trust and scope

`APP_MUTUAL_V1` is a separate, typed security class over an encrypted BLE
link. Just Works encryption may carry it, but the firmware must never set or
report the BLE SDK's `mitm` flag from an application proof. A Bluetooth bond,
Android `OS_BONDED`, a service discovery, CCCD acknowledgement, transport
probe, or notification is neither this proof nor a Game Master (GM) grant.
The class proves possession of both *enrolled* private keys for one current
session; it does not establish physical proximity against a live relay.

Each box has one device-specific signing identity; each phone/app installation
has its own non-exportable signing identity. Enrollment pins the corresponding
public keys and key IDs to the exact physical box and phone, never to an
advertising name, BLE address, bond record, display `deviceId`, or fleet-wide
secret. The existing 16-byte device and peer labels remain labels, not keys.
The box accepts a phone key only from a currently valid, explicitly scoped
enrollment record. A valid key without an explicit GM assignment remains
unprivileged. The phone likewise accepts the box only if the locally enrolled
box key and chip-bound identity match; no trust-on-first-use over BLE.

The enrollment ceremony must be defined and tested **before** key creation:
the operator physically connects the exact box and phone to a trusted USB
station, checks chip identity and intended pairing of devices, authorizes the
scope, receives both public-key confirmations, and commits a matching record
atomically on each side. The station must report partial commits as failed and
leave both sides closed. A computer that merely sees a USB serial port or ADB
device is not automatically a trusted enrollment station. A phone reinstall
or replacement creates a new identity; copying an old app data backup must not
recreate rights. Android should generate its private key in Keystore and
report actual hardware/attestation capabilities instead of assuming them.
The current ESP32 has no verified protected private-key storage or antirollback
path. Its key protection, provisioning mode, factory ownership, and recovery
must be decided before any real key is installed. No production key belongs in
a repository, log, report, fixture, firmware binary, or flash backup shared
as a diagnostic artifact.

## Identity and session binding

Every authentication attempt begins closed and binds all of these values:

- Protocol and cipher-suite version, device role (`box` or `app`), device
  model/protocol compatibility, both enrolled key IDs and public-key hashes.
- Box chip identity and stable `deviceId`, app installation identity, current
  enrollment generation and revocation generation on both sides.
- Fresh box and app random challenges; fresh ephemeral ECDH public keys; the
  complete ordered request/response transcript and its cryptographic hash.
- The original product source registration, GATT interface, peer identity,
  connection birth token, boot identity, current encrypted-link observation,
  and the exact authentication attempt number. A reused 16-bit handle or MAC
  never substitutes for that birth.

The signed transcript names both participants and their roles in a canonical
order with domain separation from existing `PeerProof` signatures. Both sides
verify the other enrolled signature before deriving directional session keys
from ECDH with a standard KDF. Independent key-confirmation/Finished messages
must authenticate the *full* transcript before application-authenticated data
is admitted. Session keys, challenges, and attempt state are volatile and are
never restored after disconnect, process/box restart, key rotation, or link
security loss. No locally inferred timeout, BLE `AUTH_CMPL`, or peer-provided
timestamp substitutes for verified Finished. An unknown suite or message
version fails closed, with no downgrade to the current MITM proof path.

This is a semantic contract, **not a byte format or selected cryptographic
suite**. An independently reviewed exact encoding, algorithms, random-source
requirements, test vectors, and Android/ESP32 golden fixtures are needed before
implementation. In particular, a signed ephemeral exchange must be bound to
the original SDK birth without trusting a value supplied by the remote peer.

## State machine and admission

| State | Entry evidence | Allowed output |
| --- | --- | --- |
| `NOT_ENROLLED` | No valid local key/peer record or unverifiable chip binding | Public discovery and explicit closed error only |
| `ENROLLED_CLOSED` | Valid local record and current encrypted original connection | Bounded authentication exchange, no game/diagnostic request |
| `VERIFYING` | Both fresh challenges and signed transcript in progress | Bounded auth response only; no role or grant |
| `MUTUAL_CONFIRMED` | Both enrolled signatures and Finished verified on the same birth | A typed, short-lived proof may be handed to the original authority actor |
| `ROLE_CONFIRMED` | Original actor rechecks proof, current enrollment and explicit stored GM role, then publishes exact authority and ACK | Game commands admitted only on that authority and protected session |
| `DIAGNOSTIC_GRANTED` | Fresh GM/role ACK plus deliberate diagnostic start, all component park ACKs and an independent confirmed off gate | Correlated Control/Step requests within one bounded grant |
| `CLOSED` | Disconnect, link loss, stale birth, timeout, replay, revocation, conflicting role, reboot, owner failure, or unknown outcome | No new privileged operation; preserve/report uncertainty |

Transitions only advance on original-owner observations and explicit ACKs.
`MUTUAL_CONFIRMED` alone is not a GM role. `ROLE_CONFIRMED` alone is not a
diagnostic grant. The current `IdentityAccess` and F0/F1 proof path require
BLE MITM; a later implementation must add an explicit `APP_MUTUAL_V1` variant
and recheck it at the grant and every privileged route. It must not synthesize
`encrypted=true, mitm=true, bonded=true` to satisfy the old boolean policy.
An F1 reconnect starts a new birth, challenge, ECDH exchange, Finished, role
lookup and ACK; a saved bond or old grant cannot shortcut that sequence.

Only the original serialized owner may publish the proof into the authority
channel. Worker queues carry copied immutable evidence, not a mutable proof
reference. At posting, grant, command admission, actual execution, and reply
submission, recheck registration, birth, enrollment/revocation generation,
role epoch, session ID, sequence and expiry. A closure must reach the original
owner even if a data FIFO is full. Closing an old publisher must never revoke
a newer birth; conversely a stale callback must never reopen one.

## Protected messages and replay

All privileged application messages, including F0 challenge/issue/proof/result,
game/profile commands, diagnostic Control/Step and their replies, require
authenticated encryption or a separately reviewed equivalent integrity
envelope under the *current* session. Distinct keys and nonce spaces are
required for each direction. Additional authenticated data binds protocol
version, session/transcript hash, direction, message type, complete original
birth, role and revocation generation, request/run/step IDs, and exact length.
Maintain a monotonic receive sequence per direction; reject duplicate,
out-of-order, wrong-session, wrong-type, truncated, oversized, and late
frames. Sequence exhaustion or uncertain counter state closes the session
before another message. Reconnect never resumes a previous sequence space.
Retransmission of an action after a lost reply needs an independently specified
idempotency/lookup rule; a GATT write ACK or missing notification says nothing
about whether the action ran. The app records `unknown_outcome` where needed.

The existing diagnostic wire `DG` v1 has fixed 69/80-byte requests and up to
128-byte replies. An added authenticated envelope does **not** fit the
current MTU >=131 assumption by declaration alone. A versioned framing and
capacity plan must be agreed across SDK, Android, codec, report and tests,
including fragmentation/reassembly if chosen. Until then Control/Step stay
closed. Neither a five-byte transport probe nor its CCCD ACK carries an
APP_MUTUAL_V1 proof.

## Enrollment, revocation and recovery

An enrollment record needs key ID and public-key digest, exact box chip ID,
app installation ID, scope, issuing authority, monotonically protected
generation, creation evidence, and revocation status. The trusted USB station
is the only path to add/replace a trusted key or explicitly assign/revoke GM.
BLE cannot enroll an unknown key, self-assign a role, undo a revocation, or
rotate to a new key merely because an old session is open.

On loss of a phone, the operator connects the box by USB and revokes that
phone's key. The box atomically persists the new generation and closes any
live session, role authority, queued request and diagnostic grant derived
from the old generation. The station reports completion only after a durable
device acknowledgement and a readback that detects stale state. Every other
paired box needs its own revocation, unless a future independently trusted
fleet mechanism is explicitly designed. A new phone must enroll afresh.

A full flash restore or NVS rollback may resurrect old keys and roles. Before
real enrollment, provide a monotonic, independently protected rollback barrier
or a documented factory recovery that resets trust and requires physical USB
reenrollment. A normal backup or successful flash hash is **not** an authority
restore. Secure Boot, flash encryption, eFuse changes, and their development
and disaster-recovery effects require a separate reviewed plan and user
authorization. If rollback protection is unavailable on this Originalbox,
APP_MUTUAL_V1 remains a development design and cannot be called a durable
revocation or production trust path.

## Migration and acceptance gates

The `ESP_GATT_PERM_*_ENC_MITM` game/profile and diagnostic attributes currently
prevent the selected Just Works transport from reaching the application
proof. Any change to their ATT permissions is one coordinated, versioned
migration of `SdkGattsService`, `DiagnosticGattsService`, original GATT source,
F0/IdentityAccess, F1 reconnect, game/profile admission, Android and reports.
The ingress must reject unencrypted links and all non-auth protocol writes
*before* payload copy or command decoding; prepare/execute writes,
write-without-response, Read Blob, CCCD and malformed frames get explicit
fail-closed treatment. A per-attribute matrix and no-downgrade migration plan
must be reviewed before any permission relaxes. The present closed GATT gate
remains in place meanwhile.

Review and test, at minimum: swapped box/phone, unknown and revoked key,
wrong chip/installation, BLE MITM flag spoofing, cut/reordered transcript,
nonce reuse, replay in either direction, concurrent attempts, recycled handle,
F1 delayed SDK events, lost Finished/role ACK/reply, rekey/reboot, role revoke
while queued, full-flash/NVS rollback, all GATT operation forms, and no grant
from a black LED frame. Host and Android golden fixtures and full firmware/app
builds precede a chip-bound upload. Only a later user-operated app run with a
correlated firmware result and observation report can assess physical function.

**Open design choices:** exact suite/encoding and authentication GATT channel;
trusted USB station and operator check; actual ESP32 key storage and rollback
barrier; Android hardware-key support on target phones; per-attribute migration;
envelope/MTU capacity; external security review. This document settles none
of those details by implication and grants no product or hardware approval.
