# Bounded raw GATTS write admission

Status: implemented local admission/Host path and compile-only SDK ABI probe.
The actual registered radio callback, lifecycle attribution, security verifier
and SDK response bearer remain open. No RF/device/Android run is claimed.

## Original connection and task ownership

`GattsWriteBinding` is an immutable observation from the trusted SDK lifecycle
owner: interface, selected attribute, SDK handle, peer address, independently
minted connection generation, and original Principal/Epoch/GM. A raw SDK event
contains no application generation. The provider must synchronize acquisition
and prove event ordering; looking up the currently active connection while
draining old callbacks is insufficient. Address matching is routing, not a
cryptographic identity or GM assignment.

`SdkGattsWrite.h` uses the actual pinned Arduino2.0.14/IDF4.4.6 structures,
including `conn_id`, `trans_id`, `bda`, `handle`, `offset`, `need_rsp`, `is_prep`,
`len` and borrowed `value`. It never infers authority from these fields.
Execute events have no value/attribute; the original selected attribute is
supplied by the lifecycle owner, and both Execute and Cancel are explicitly
rejected. No prepared values are retained. ATT status constants are statically
compared with the pinned SDK enum. Primary ABI source:
[ESP-IDF4.4.6 GATTS header](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/api/include/api/esp_gatts_api.h).

The raw callback is task-context work: at most one 9..128-byte frame copy into
the actual four-place static queue and one optional scalar response copy.
No heap/profile/JSON/filesystem work, game tick, retry or nonzero queue wait.
The original callback timestamp is retained, including queue wait in the
existing absolute2000ms command deadline.

## Checks and results

Missing binding, wrong interface or wrong SDK handle/address cause no response
on any inferred current peer. For the selected original context, original GM
rights must still match actual channel authority. A wrong attribute, prepared
write, nonzero offset, null/short/oversized value, malformed header and wrong
frame kind receive explicit errors if a response was requested and can still
be copied to that exact original connection. The actual queue independently
checks authority before copying; the stream/control owner checks it again.

| Result | ATT status | Meaning |
|---|---|---|
| Copied |0x00|One fixed frame now belongs to the FIFO|
| QueueFull |0x11|No copied frame; caller may retry explicitly|
| Unauthorized |0x08|Original authority is absent/revoked/stale|
| WrongAttribute |0x01|Wrong selected attribute|
| PreparedUnsupported/UnsupportedFrame |0x06|Unsupported operation/frame kind|
| InvalidOffset |0x07|Only outer offset0 is accepted|
| InvalidFrame, outer length |0x0d|Value must contain header plus payload within128|
| InvalidFrame, header |0x04|Malformed document header/range|
| Unavailable |0x0e|Queue was not initialized|

A requested positive response is copied only after the frame has been copied.
A lost response neither removes that frame nor executes it again. A write
without response still gets only one bounded frame copy. The bearer must
recheck original SDK handle/generation/address/interface at actual submission;
handle reuse cannot redirect an old error response. Generic error responses
may use an original still-live connection after GM revocation; they contain
no private state. The real `esp_ble_gatts_send_response` path is not implemented
by this ABI probe.

ATT success means copied input, not complete CommandQueued, Executed, saved
profile or peer delivery. Existing type3 processing receipts and type2 game
replies represent the later distinct observations.

## Actual vertical checks and probe

Twelve new native cases run the actual admission/FreeRTOS adapter/CommandStream
and actual stored ProfileApplication/GameControl. The Host fixture contains
153 actual command bytes in13max20-byte values, lease2 after the real policy
admission. All13 input copies precede the owner service; running remains false
and request1 remains unconsumed until owner execution. The independent Python
counterpart reassembles these exact bytes, validates canonical JSONv2, checks
the scoped Host observation and rejects15 malformed sequences. That JSON
observation is not an ATT wire response.

Other cases cover four-place overflow, no response request, response loss,
revoke between guard/queue observations, revoke after copy, recycled handles,
original wrapping queue time and uninitialized queue. Recycled response
attempts retain the old epoch/generation/transaction and are rejected.

`esp32-gatts-abi-probe` compiles actual SDK write/execute fields and routes
synthetic values through the actual queue/stream. Its synthetic verifier and
response sink are explicit. It never initializes Bluetooth, advertises,
registers a server or starts an application/game owner. CommandQueued therefore
remains unexecuted in this probe. Production source excludes this entry point.

Envelope sizes measured with Xtensa and deterministic RTOS header stubs:
binding32/event32/response40/result4Bytes. Own `-Os/-fstack-usage` admission
frame128Bytes, excluding called queue/channel/bearer frames. One copied frame
and one optional response per call are fixed work bounds; microsecond, SMP,
RF and total stack/heap reserves require a device run.

## Local evidence

125/125 final native cases, exit0:
`%TEMP%/larp-gatts-admission-host-final-20261007.log`.
Architecture98files/222direct includes;62pure headers;169format files passed.
Schemas/generator and all prior Reply/Receipt/Status/publicIdentity counterpart
checks passed. Twelve App-v2 snapshot files freshly match bytes and SHA256;
no App code or App check was changed/run. Five targets now built successfully: four existing targets in
`%TEMP%/larp-gatts-admission-builds-final-20261007.log`; new SDK target in
`%TEMP%/larp-gatts-admission-sdk-green-20261007.log`, all exit0.
First SDK build exposed an ambiguous global `access` name; renamed before
commit, then the actual target build passed. This was a new probe compiler
error, not a failure of earlier committed firmware. A manual identity CLI
check also used an invalid31-character value; repeated with the actual golden
ID,187exact bytes and three invalid inputs confirmed.
RAM/Flash: production55,148/1,228,529Bytes(93.7%slot), oldUSB21,480/291,785,
profile107,328/395,001 plus57,272Profile heap/allocator/filesystem;
isolated ABI probe25,176/275,093Bytes. Production entry remains the old controller.
Actual isolated ELF: channel1264/queue672/stream1048/policy144,
originalBinding32/syntheticResponse48/document512/frame20/connection8Bytes.
Actual SDK object entry frames: admission128/setup224/loop192/syntheticCopy32,
plus called frames. No total measured reserve with radio or real owners.

Remaining: registered SDK lifecycle/generation attribution and response bearer,
secure stable peer credentials, provisioning/admin/recovery and persisted GM
roles/keys; profile transport; real device owners/production flash reserve;
physical RF/Android/SMP/timing/stack/heap/energy/flash-failure acceptance.
No new upload, erase/format, remote CI, push, PR or merge.
