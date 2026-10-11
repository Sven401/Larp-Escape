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
the game/profile queues. A write without response or prepared write cannot
open a path. No notification API is called. SDK MITM enforcement is an
additional gate, not an authority or physical-off proof.

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
