# A new authenticated profile attempt invalidates the prior receipt

The actual host Raw/queue/ESP temporary-source/Application path exposed a bug in
1d0d114: after DraftReady, a fresh followup with the same transfer ID failed to
open its temporary file; later Apply still returned ApplyQueued for the old
import. The regression test failed before this change (expected7, got2).

The internal ProfileFramePump now calls sourceAttempted(original) for a valid,
queue-fresh metadata value before source.start and any backend IO. Control first
requires ready (no pending response/selected Apply), freezes the original route,
uses its own reentrancy guard, and confirms current GM before dropping the
private receipt. Storage failure, confirmed cleanup and unchanged local draft
cannot revive the old receipt. Later Apply returns the existing20-byte
ApplyRejected/NoReceipt (7/10), same request/revisions/original route. No new wire
field, request reset, profile copy, retry, file operation or tick is added.

An unrelated/unauthorized route cannot invalidate current proof. Queue-expired
metadata is discarded before this call. Pending output or a selected Apply
cannot lose its receipt/Saved correlation; pump backpressure remains unchanged.
Historical duplicate response caching still refers to the old consumed request;
it never reconstructs a new private receipt. Application Ready draft may remain,
but only another actual successful import can reissue remote Apply permission.

Native regression uses actual ESP adapter under Host VFS and current App, plus
wrong route, expired metadata and selected-Apply cases. Independent struct
counterpart reconstructs actual response and rejects five altered correlations.
These are host models; no radio, cryptography, physical flash or peer delivery
claim. Final checks/build budgets are recorded after execution.

Final verification,7 October2026:293/293 native functions (four new), without
fixture export, five final builds successful. Architecture124/267,75 isolated
headers,231 format files. Actual fixture counterpart rejects five incorrect
correlations; all older counterpart checks remain valid. Twelve App-v2 files
freshly byte/SHAequal; no App code/validation/remote chat changes. Initial added
expiry-case used two inconsistent fake clocks, corrected before final run.

Build RAM/Flash unchanged from1d0d114:production55148/1229077(93.8%),oldUSB21480/
291881,profile107560/406613 plus57272StartupProfileheap/allocator/FS,SDK26128/
286349. Pure ESP32 sourceAttempted own compiler entry96Byte plus virtual/channel/
caller chains; no full stack reserve or timing measurement. No new object fields,
heap/document/Profile allocation, upload, physical flash/RF/SMP/energy proof,
Cppcheck, RemoteCI, push, PR or merge.
