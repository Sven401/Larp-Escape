# Application ownership of temporary profile IO

Status: implemented in the narrow host/tool and compile-only profile probe path.
No new wire kind, v2 field, callback IO, third Profile slot or game tick is added.

## Serialized actor and private fence

`integration::ApplicationProfileStaging` decorates one exclusively borrowed
`IProfileStagingStore` and the actual `devices::ProfileApplication`. Application
and backend outlive the bridge. All methods, GameControl and Application service
run in the same serialized application actor; this is not an SMP mutex. Transport
callbacks still copy bounded values to their static queues, never files here.
No other actor may use the backend directly.

The App stores a private original bridge pointer and cleanup quarantine. Before
any backend begin/discard, it claims this pointer and sets the quarantine. Only
confirmed backend discard clears both. Failure keeps them; bridge destruction
performs no IO, clears its pointer and retains quarantine. Reconstructing a
bridge at the same address cannot begin a new source before explicit successful
cleanup. Another live bridge cannot steal or release the source.

Collection begins only in confirmed Ready, stopped, non-operating application.
Trusted startup/Fault cleanup is permitted when stopped and non-busy; no file
operation is allowed during boot activation/save or a running game. Invalid
bounds, absent ownership and nested bridge calls cause no backend IO.

While claimed/quarantined, all game operations return existing execution Busy;
activateDraft/restoreStored/begin are rejected before their IO. The GameControl
v2 Start response remains execution_rejected with executionOutcome busy. It is
consumed/cached by the existing request history; after cleanup the GM sends a
fresh request. No implicit stop, restart, automatic retry or authority change.
The existing Ready phase and storedMatchesActive facts remain unchanged; neither
claims that a Start must execute. No new App contract fields are introduced.

`ProfileApplication::busy()` deliberately remains its short operation/activation
fact. A sealed source must still be imported through the existing private draft
inside App/Controller operating guards. Read/discard of the owning sealed source
remain permitted in those guards, including source revocation/expiry cleanup.
Nested file calls are separately fenced. Existing revision/receipt checks remain
responsible for intervening draft edits; no raw/private slot is exposed.

## Bounded source work

Begin accepts1..131072 bytes, append1..120 and exact remaining length, seal only
at the exact advertised count. Append freezes a local120-byte copy before the
external backend. The backend retains the complete immutable length/EOF policy.
Reader failure or unknown status preserves caller byte and poisons the bridge;
End preserves it. Failed begin/write/seal/read/discard keeps the Start fence.
The only recovery operation is explicit confirmed discard. No retry loops,
heap allocation, document/Profile copy or new timeout policy in this bridge.
Original source120000ms and control/reply10000ms policies stay unchanged.
Synchronous backend/NVS/stdio/FS latency remains a separate bounded-IO assumption;
the bridge does not preempt file IO or prove its wall-clock duration.

## Vertical uses and evidence

Actual Raw/ProfileFrameQueue -> bridge -> actual ESP temporary adapter under
Host VFS -> source -> private import -> receipt -> Apply -> owners/journal model
-> FreeRTOS response queue/sender still reaches DraftReady/ApplyQueued/Saved.
Confirmed cleanup precedes Apply. Finish cleanup failure may leave a local
Ready draft, but clears receipt, rejects Apply and keeps Start blocked.
Metadata during a running game causes source StorageFailure without any VFS IO
or stopping the game. Rights loss/expiry closes the source; failed cleanup keeps
the local quarantine even when no authority is available.

The compile-only ESP profile probe now runs its one-byte actual temporary-file
roundtrip after confirmed Ready, before the first GM Start, through this bridge.
It checks direct Start refusal while holding the source; no upload/device run.
The SDK ABI probe remains isolated, with no Application/file/radio owner added.

`profile-tools --file-owner-probe JOURNAL_DIR` requires an existing explicit host
directory. It uses the existing native journal and synthetic owners, an actual
anonymous tmpfile and one deliberately injected cleanup failure. It prints one
host observation only after blocked Start, failed-cleanup quarantine, destructor
quarantine, denied new begin, confirmed cleanup, successful Start and zero file
IO during running are observed. It does not import a profile or measure physical
flash durability, peer delivery, RF, SMP, stack, heap, time or energy. Failed setup
or missing directory emits no success observation. Host journal records may be
created/loaded in the explicitly supplied directory; ESP storage is unaffected.

Native fixtures and `scripts/check_application_file_owner.py` independently
check observation types, quarantine scope and13 wrong observations. Existing
v2 snapshots/schemas remain the App contract. Final test/build counts and static
compiler budgets are recorded separately after execution.

## Compiler budget, 7 October 2026

Pinned ESP32 compiler: bridge24Byte, ProfileApplication11224Byte (+8 from the
prior definition). No Profile/document allocation in the bridge. Pure individual
entry frames: bridge begin/seal/discard/destructor32, read48, append160Byte;
App claim/owns/release/abandon32, gameAllowed64, Start48, begin208,
activate/restore128Byte. These exclude full virtual/reader/parser/backend/
filesystem/journal/RTOS/caller chains and do not prove task stack reserve.
Actual final ELF/build totals and host/device limitations are recorded separately.

Final local verification:289/289 native functions (18 new), without fixture
export; five final builds successful. Architecture124 files/267 direct includes,
75 independent C++11 headers,231 format files. Independent counterpart rejects13
wrong observations; three negative compiles reject private claim/release/fence
writes. Built CLI checks fresh/restarted actual native journal, six failing
argument/backend paths without success output and54 unchanged legacy owner traces.
Twelve App-v2 snapshot pairs freshly byte/SHA256 equal; no App execution.

Actual final profile ELF confirms bridge24/App11224/sourceChecked1Byte, own entry
frames as above, setup224/loop864Byte plus all call chains. Total build RAM/Flash:
production55148/1229077 (93.8%), oldUSB21480/291881, profile107560/406613 plus57272
startup Profile heap/allocator/FS, SDK26128/286349. Net from362b: production+228
Flash, profile+40RAM/+1300Flash; oldUSB/SDK unchanged. Symbols are not summed to
infer net totals. RemoteCI/Cppcheck/board/upload/radio/SMP/time/heap/stack/energy
and power-failure measurements were not performed.
