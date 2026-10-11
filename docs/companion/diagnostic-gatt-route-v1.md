# Diagnostic GATT route v1 — closed registration contract

Status 2026-10-11: the byte codecs and a fail-closed, original-bound notification
fence compile in the product branch. **No diagnostic attribute is registered, no
diagnostic write is routed, and no radio advertising is started.** The current
product `ProductGattsOwner` registers only its existing game/profile service and
keeps `IdentityAccess` closed. This document fixes the route expected by a future
Android transport; discovery of these UUIDs must remain unavailable today.

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

The current product cannot safely register these attributes. It has no
production same-birth F1/F2 identity and Game Master role ACK, no diagnostic
receiver on the serialized original SDK actor, no proven diagnostic component
Park/Off acknowledgment, and no real `IPort` for the arbiter. Its independent
timer and black frame submission do not establish electrical or visible LED
off. The host `ControlTransport` uses a synthetic owner; routing its Grant to
real hardware would cross the closed gate. A future attribute implementation
must add a bounded callback-to-owner queue, check ATT/CCCD/MTU/origin at both
ends, and prove cancellation and stale-reply behavior before enabling it.

Software checks here make no claim about radio behavior, actual ESP32 stack
latency, LED power, full-box function, or product release. No upload, restore,
advertising, physical test or user-led report was performed.
