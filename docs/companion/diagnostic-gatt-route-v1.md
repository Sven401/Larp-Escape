# Diagnostic GATT route v1 — closed registration contract

Status 2026-10-11 on isolated branch
`plan/nutzergefuehrte-hardwarediagnose-gatt-registrar`: the byte codecs,
original-bound notification fence and a **closed diagnostic service registrar**
compile in the product. The registrar creates the service and four v1
characteristics on the same original SDK interface as the existing game
service, with two distinct 0x2902 CCCDs. It installs the complete handle set
on the original BTU actor before starting the diagnostic service. Product GAP
advertising of the existing game UUID waits for both services and the closed
registry ACK. No diagnostic owner, grant, reply sender or accepting write path
is installed. No firmware from this branch has been uploaded or observed over
radio.

All UUIDs below are canonical text order. SDK BLE UUID arrays use the reverse
128-bit byte order. The service is distinct from the existing game/profile
service. `DG` wire version remains exactly 1.

| Role | UUID | Properties | Attribute permission |
| --- | --- | --- | --- |
| Diagnostic service | `407e6a1d-1cc2-43fd-856b-4aff85a20010` | primary | — |
| Control request | `407e6a1d-1cc2-43fd-856b-4aff85a20011` | WRITE, response required | encrypted + MITM write |
| Control reply | `407e6a1d-1cc2-43fd-856b-4aff85a20012` | NOTIFY | encrypted + MITM CCCD write |
| Step request | `407e6a1d-1cc2-43fd-856b-4aff85a20013` | WRITE, response required | encrypted + MITM write |
| Step reply | `407e6a1d-1cc2-43fd-856b-4aff85a20014` | NOTIFY | encrypted + MITM CCCD write |

The two reply characteristics each need a distinct 0x2902 CCCD. No read,
write-without-response or prepare/execute write is part of v1. SDK encryption,
MITM, bond state, CCCD subscription and successful ATT write response confer no
`IdentityAccess` authority. An ATT response confirms only local write handling.
NOTIFY does not confirm delivery; a lost reply or segment stays unclear in the
app, and no execution retry is implicit.

`DG` Control requests are exactly 69 bytes, Step requests 80 bytes, Control
replies 88 bytes, and Step reply frames at most 128 bytes. Require effective
ATT value capacity >=128 bytes on both sides, hence negotiated MTU >=131
(ATT value capacity = MTU - 3). Reject a smaller MTU before the first write or
notification. No silent fragmentation, truncation, prepared write or change
to the existing game-frame attributes. The codec validates exact lengths,
version, correlation and timing as described in
`diagnostic-transport-host.md` and `diagnostic-step-transport-host.md`.

For each accepted input, the SDK actor must capture its original interface,
registration generation, peer address, connection birth token, complete current
trusted authority and callback time before any queue copy. The selected
attribute must match the original service registration. A single serialized
owner must recheck that full origin and `IdentityAccess.route` before decoding,
before touching the diagnostic owner, and at actual reply submission. A changed
birth, role epoch, principal, peer, service generation, subscription or MTU
closes the pending route. The notification bearer must independently check the
same origin at SDK posting; `DiagnosticGattsRoute` enforces this interface but
does not itself own the SDK actor. Never look up a new current connection by
recycled 16-bit handle while draining old work.

The product registrar reserves exactly 11 handles: service, four
declaration/value pairs, and one CCCD after each reply value. Both request
characteristics have only normal WRITE property and
`ESP_GATT_PERM_WRITE_ENC_MITM`; both reply characteristics have only NOTIFY
property and no read/write permission. Each CCCD uses
`ESP_GATT_PERM_WRITE_ENC_MITM` and application response mode. The original
BTU request callback intercepts all four writable diagnostic handles. If the
exact installed source registration and current connection birth remain live,
it returns `ESP_GATT_INSUF_AUTHORIZATION` for a write requiring an ATT response;
otherwise it consumes the request closed. It never copies diagnostic bytes to
the game/profile queues or even into their local write buffer. This refusal
does not depend on an authenticated IdentityAccess snapshot: an unassigned
client can receive the explicit closed error on its own live link. A write
without response is silently consumed, while a prepared write requiring a
response receives the same closed error. A stale link receives no reply and
cannot borrow a replacement birth. No notification API is called. SDK MITM
enforcement is an additional gate, not an authority or physical-off proof.

The registrar copies callback scalars and UUID bytes into a bounded queue;
the separate worker checks original interface/registration, expected
service/attribute handles and order, exact UUID, 30 s setup deadline and
positive SDK callback ACKs. A wrong/overflowed/late callback fails the
registration. A source-actor handle install may happen only once and before
any connection; its original ACK precedes `start_service`. A failed setup
prevents product advertising. This package does not claim measured callback
timing, radio discovery, pairing or a live security result.

The product still has no complete same-birth F1/F2 identity and Game Master
role ACK, no diagnostic receiver/owner, no proven diagnostic component
Park/Off acknowledgment, and no real diagnostic `IPort` for the arbiter. The
independent LED timer and black frame submission do not establish electrical
or visible LED off. The host `ControlTransport` uses a synthetic owner;
routing its Grant to real hardware would cross the closed gate. A later
package must add bounded callback-to-owner transport, recheck
ATT/CCCD/MTU/origin at both ends, and prove cancellation and stale replies
before enabling anything.

The configured-control host suite passed 533/533, including exact SDK shape,
source-actor install before service start, wrong callbacks/origin, all four
closed write handles and a connection preceding the install ACK. The ESP32
single-app build passed with the pinned SDK. These are software checks: no
radio behavior, runtime heap/latency, LED power, full-box function, or product
release is inferred. No upload, restore, physical test or user-led report was
performed for this registrar branch.

Follow-up closed-ingress check: 534/534 configured-control host cases pass
after the refusal was moved ahead of payload copying and decoupled from the
identity snapshot. The ESP32 single-app product build passed with 1,973,901
program bytes of 2,621,440 (75.3%); it was not uploaded. This is still
**not** a Control/Step bridge. The SDK source
does not yet supply a captured negotiated MTU, separately correlated CCCD
state, an admitted same-birth F1/GM authority or a serialized production
`Control`/`StepTransport` owner. Those are mandatory before any forwarding,
positive ATT write status or reply notification can be added.

## Encrypted CCCD transport preparation, 2026-10-11

The original registration now acknowledges a **normal, response-required**
`0x0001` subscription write or `0x0000` unsubscription write to either
diagnostic reply CCCD, only on the current original connection. The two CCCDs require an
encrypted link, but no MITM claim; the two Control/Step request attributes
retain their encrypted + MITM SDK permission and closed application gate.
Prepared writes, offsets, wrong lengths/values and writes without response
cannot receive a successful ATT response. A retired or mismatched link gets
no response. No CCCD bytes enter the game/profile command queues.

This initial change acknowledged **only transport preparation**. It did not
grant a role or run or prove that a notification could be delivered. The descriptor remains in
application-response mode with no read permission; no persistent CCCD state
is inferred from the local ATT response. In the pinned SDK,
`ESP_GATT_RSP_BY_APP` makes the application responsible for the response;
`esp_ble_gatts_set_attr_value` is a separate API and is not called here.
Thus the ACK does **not** establish that the CCCD database value was updated.
The Android UI must report this as an acknowledged descriptor write, never as
ready reply transport or diagnosis access. The earlier closed-registration
description and test counts above describe the prior state.

## Per-link notification transport probe, 2026-10-11

On the original BTU connection, a well-formed encrypted `0x0001` CCCD write
for ControlReply or StepReply receives its ATT response first. Only after an
SDK-successful response and an exact recheck of registration, peer address,
connection birth and lifecycle does the source enqueue one notification on
that channel's reply characteristic, only for that channel's first enabled
transition on the current birth. Its entire value is five octets:

| Bytes | Meaning |
| --- | --- |
| `44 47` | ASCII `DG`, reserved for transport probe |
| `01` | Probe wire version |
| `50` | Probe type, distinct from the normal reply codec |
| `01` or `02` | ControlReply or StepReply channel respectively |

The SDK call uses the original GATT interface, the public connection id from
the packed source id's upper octet, and the installed reply value handle.
Each CCCD has its own subscription flag tied to the exact original connection
birth. A valid `0x0000` write clears only its channel. Every connection or
disconnection callback clears both. Malformed, stale, response-free, and
request-characteristic writes enqueue no probe. An SDK notification enqueue
failure is not retried or treated as delivery. The Android side requires an
actual notification callback with these bytes on **each** expected
characteristic in the same GATT generation before marking reply transport
observed. ATT ACK and the firmware's successful enqueue remain insufficient.

This probes transport only. It never calls a diagnostic owner, accepts a
Control/Step request, verifies F1 identity or Game Master role, grants a run,
changes an LED, or proves physical behavior. Host tests exercise the SDK call
and stale-birth guards; an ESP32 build is compile evidence. Until a user-led
device run yields both notifications in an exported report, radio delivery is
unverified.

## Original-bound MTU observation, 2026-10-11

The selected private GATT source copies `GATTS_REQ_TYPE_MTU` (kind 4) on the
original BTU actor before forwarding that callback unchanged. Its full packed
connection ID, source registration and current connection birth are captured
with the SDK-reported MTU (23..517). A read requires the caller's entire
original `PeerTransportBinding` still to match the current link; disconnect
and a new connection clear the observation even if the SDK reuses the packed
ID. Invalid or missing MTU data leave capacity unknown. The later public
`ESP_GATTS_MTU_EVT` contains only a connection ID and MTU, so it is not used
to assign a value to a birth after a possible reconnect.

This is a transport-capacity observation only. The diagnostic request handles
still return `InsufficientAuthorization`, including at MTU 131 or above;
neither Control/Step bytes nor a copied authority are forwarded to an owner.
No role, grant, notification delivery, execution or physical result follows
from an MTU observation. A production request bridge still needs a single
serialized diagnostic owner, exact current authority, both subscribed reply
channels and a real component port before any positive ATT response.
