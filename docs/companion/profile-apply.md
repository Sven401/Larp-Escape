# Authorized imported-profile activation

Status: actual internal receipt/import/apply/Application/journal/Host path and
independent current-status counterpart implemented. Network profile data/spool,
wire apply request and registered SDK profile endpoint remain open. App code
is owned by the App session and was not changed or run here.

## Internal evidence and application ownership

`ProfileImportReceipt` now has private issuance/route/source/revision/application
fields. Only the successful source importer is a friend permitted to issue it.
Read-only public observations and bounded server copies remain available; no
client JSON/binary deserializer or public setter exists. Three actual negative
compiler cases reject ordinary issuance, application-binding and revision
mutation. This is an internal process object, not a cryptographic credential.
Never reconstruct it from remote metadata or persist its application pointer.

The receipt is bound to the exact process-lifetime ProfileApplication which
issued it, as well as original Principal/Epoch/GM and validated source/revisions.
A different Application with identical draft/configuration revisions cannot use
it. Source revocation/expiry after parsing can leave a local ready draft with
no issued receipt; a new GM cannot turn that missing receipt into remote apply.
Local admin authoring APIs remain explicit separate operations.

`applyImportedProfile` copies the internal receipt before external observation.
It requires issued/sane matching source receipt, exact issuing Application,
current original GM, Ready/stopped/initialized/active Application, current
accepted draft and expected configuration revisions. Authority is observed again
immediately before the real `activateDraft` call; admission/controller checks
remain in the actual Application. It performs at most one activation request,
no game tick, wait, retry, heap allocation or implicit start.

Results: Queued, InvalidReceipt, WrongApplication, Unauthorized, NotReady, Busy,
GameRunning, StaleRevision, Rejected. Rejected preserves the actual Application
result. Duplicate during the transaction is Busy; after completion, consumed
revision/configuration makes the same receipt stale. No repeated save.

This is observation-based authorization selection followed by one owner request,
not an authority lock around hardware validation. A revoke before the last
observation rejects. Once the original operation was selected/queued, a later
revoke does not retroactively cancel/rollback the accepted hardware transaction.
The existing controller finishes/rolls back its bounded owner protocol and
journal policy; it never starts the game automatically. Replies/status still
require current exact producer routing and cannot follow a new connection.
Do not claim atomic hardware execution versus an asynchronous revoke.

## Queued, applied and confirmed storage

Queued means the actual Application entered Applying; it is not durable storage.
The sole application owner services existing controller/owner acknowledgment,
then Saving/journal. Only Ready with storedMatchesActive is the confirmed model
result. WriteUncertain becomes Fault with storedMatchesActive=false and protects
the previous confirmed record. Start is denied until storage again matches the
active RAM profile. No receipt text may be displayed as already saved.

Current JSONv2 status already expresses these states. Two new actual native
encoder fixtures, outside the unchanged eight-file v2 snapshot, describe:

|Observation|Configuration|Profile|Stored generation|Stored matches|Game|
|---|---|---|---|---|---|
|queued Applying|1|previous active|1|previous active|stopped|
|confirmed Ready|2|4|2|true|stopped|
|WriteUncertain Fault|2|4|1|false|stopped|

`fixtures/profile-source/apply-observation.json` is explicitly Host storage
model observation, not an outgoing wire receipt or physical durability test.
`applied-status-v2.json` and `uncertain-status-v2.json` are actual canonical
encoder output. Independent Python checks the existing v2 schema, matches
application/game fields and rejects nine incorrect operation observations.
The real public transport must separately provide bounded operation admission/
result correlation and boot/replay control before exposing remote apply.

## Actual vertical checks and bounds

150/150final native cases/Exit0;12new apply functions plus existing source/core:
`%TEMP%/larp-profile-apply-host-final-20261007.log`.
Complete source→private receipt→actual activation→actual fake owner service→
model-confirmed journal→current mailbox/real v2 encoder. Original confirmed
record remains byte-exact. Checks cover default/foreign receipts, old epoch,
local draft edits, running game, revoke between two observations, busy/stale
replay, later revoke, storage uncertainty, receipt mutation during observation,
and locally parsed but unaccepted draft. No physical device/Funk/Android run.

Target compiler sizes: private receipt44/importResult128/applyResult88Bytes;
previous source-only receipt40/result124 is historical. Own -Os frames:
import320/guardRead48/guardCurrent48; apply272/authoritycheck48Bytes, plus callers,
Application/validation/channel/source/device chains. Three negative private
access compilations used actual pure public header and MinGW C++11 compiler.
Architecture104files/230direct includes;65pure headers;179format files pass.
Five final builds SUCCESS/exit0:
`%TEMP%/larp-profile-apply-builds-final-20261007.log`.
Production55,148RAM/1,228,529Flash(93.7%),oldUSB21,480/291,785,
profile107,328/395,105 plus57,272Profile heap/allocator/filesystem,
isolated GATTS25,176/275,093. Profile image40Flash bytes above129d2d5.
Production entry remains old; SDK objects compile import/apply, with actual
entry320/272 and authority helpers48, but do not dispatch these APIs via radio.
Actual source-envelope/command/GATTS/capabilities/identity CLI bytes unchanged;
12App-v2 schema/fixture pairs freshly byte/SHA256 equal, no App tests.
Independent source/frame/schema/generator/identity checks passed.
Initial new Host compile exposed sorted test-helper include order and a wrong
status field name; explicit dependency barrier/actual connectionEpoch fixed
before final150green. No failure of previously committed129d2d5.

## Next slice

Bounded profile data framing and an exclusive complete temporary source, with
original authority/clock, limits and partial-write/abort model; then actual
operation wire admission/result and SDK profile dispatch. Keep all existing
private slots, protected journal records and owner acknowledgment. Production
entry/flash reserve, actual credential/SMP verifier/lifecycle/response bearer,
provisioning/role-key persistence, real device owners and physical RF/Android/
time/stack/heap/energy/flash acceptance remain open. No new upload, format,
remote CI, push, PR or merge.
