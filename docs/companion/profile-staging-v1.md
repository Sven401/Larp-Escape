# Bounded temporary profile source, version 1

This slice supplies a sole synchronous receiver owner and an actual Host temporary-file backend. It does not initialize radio, add a SDK callback or supply an ESP32 storage partition. Future transport callbacks must copy bounded values into a FreeRTOS queue and preserve original trusted connection/route/time; they never write files or parse profiles directly.

## Sequence and ownership

ProfileStaging.start copies the fixed [kind5 source envelope](profile-source-v1.md), trusted original principal/epoch/GM and original admission time. It requires valid metadata, current original GM and unexpired time before opening the temporary store. It checks again after cleanup and after open. An existing receiving/complete transfer is Busy and cannot be silently replaced or have its deadline refreshed. Invalid input never opens a new source. This receiver collects only temporary source bytes; Ready/stopped/both Application revisions remain mandatory at the actual import and later apply boundaries.

For each original-route [kind6 data value](profile-frames-v1.md), data copies at most128bytes into owned local storage before clock/authority observations. Exact transfer id/32-bit offset/count bounds are checked; at most120raw payload bytes are appended, then time/rights are checked again. Missing/gapped/duplicate/extra/foreign data is not appended successfully. A value from another trusted route is WrongRoute and does not damage a still-valid original transfer. Malformed data from the original route fails/discards that transfer.

Exact byte count alone leaves phase Receiving. The trusted owner must call finish with the original route/id. Finish checks original rights/time, exact count and rawCRC32, seals/rewinds the temporary backend, checks rights/time again, then exposes phase Complete. finish is an INTERNAL owner operation: there is no new wire opcode or queue-empty-as-finish rule. Until Complete, readByte fails without interpreting pending bytes as final input.

The original120000ms budget now bounds this source collection/completion/import sequence as an explicit local policy. finish and reads retain the start timestamp; retry/data/waiting do not restart it. This is not measured RF/OS/parse latency or a promise that131072bytes fit over a default-MTU radio link. Future wire admission/queue deadlines need a separate explicit contract. uint32 time arithmetic assumes work/service within a complete clock cycle. service must be called by the owner while waiting to detect expiry/revocation and clean up.

Complete reads check original rights/time before and after backend reads, enforce exact length/final EOF and recheck CRC on actual stored bytes. Corrupt/short/long/failed sources become permanent Failure. The existing import guard independently checks CRC/length/rights/time again and parses into the existing private draft; its privately issued receipt enables the existing guarded apply/journal path. End is final, repeated End reads no backend, caller byte stays unchanged on failure/End. Revoke after End still closes source on service/read. There is no extra Profile, document RAM buffer, game tick, automatic activation/start or journal write in the receiver itself.

Any storage failure, revocation, expiry or explicit discard closes only this temporary transfer. No active journal path is passed to it. Cleanup errors are reported as StorageFailure; failed-phase service does not keep retrying cleanup. Reentrant backend/clock/authority calls cannot replace/finish/discard owner state: public mutations return Busy and nested reads fail. This fence is an owner/reentrancy guard, not a multi-core mutex or a proof of bounded filesystem latency. Each backend operation must supply bounded work; the owner checks elapsed time after a synchronous call and cannot preempt a blocked call. Caller must discard/close after consuming the source; backend lifetime outlives the owner.

## Actual Host backend and tool

TemporaryProfileFile uses std::tmpfile, bounded append <=120bytes, exact total before flush/rewind, immutable read mode and automatic close/removal. It is separate from the synthetic Application journal. stdio/OS may allocate internally; the portable owner has no heap allocation. This is not an embedded filesystem adapter, durable store, fsync/power-loss claim or partition recommendation.

profile-tools --receive-profile FRAMES_FILE

The tool reads one bounded whole uppercase hexadecimal value per line, rejects malformed/truncated/extra/embedded-NUL values, performs the actual temporary-source path and semantic profile parser, then outputs a complete Host observation. The file's checked EOF/close authorizes the TOOL's explicit finish call; it is not a rule for a future live queue. Fake principal7/epoch1/GM is visibly scoped verification=host_fake. Host monotonic clock supplies the deadline observations. Output persistentSave=false/radioMeasured=false, never claims the device received or stored a configuration. Invalid sequences/semantic profiles produce exit1 and no successful observation. A maximum source and both20/128byte values roundtrip through the authoring/receive tools.

## Current local evidence and remaining connection

Actual native path:20/128-byte data → actual temporary file → private Application import → internal receipt → controlled apply → synthetic owners → journal model → current bound mailbox/real v2 encoder. Original confirmed journal record is preserved; game stays stopped. Backend open/write/seal/discard/read/length/CRC failures, original rights/time/clock wrap, reentrancy/caller mutation/explicit finish and cleanup fences are separate checked cases. Counterpart validates an actual temporary-source observation and current v2 status, with seven invalid observations rejected.

The next vertical connection is copied bounded FreeRTOS profile input and dispatch by the sole owner, followed by explicit profile control/answer wire commands. Actual SDK lifecycle/verifier/response bearer, identity provisioning/role-key persistence, embedded temporary storage and production flash reserve remain open. Real device owners and RF/Android/SMP/time/stack/heap/energy/flash-failure acceptance remain separate. No new upload, formatting, radio initialization, push, PR or merge.


## Final local evidence, 7 October 2026

181/181 focused native functions passed/exit0, nineteen added in this slice. Actual running game rejects staged import without reading/editing draft; subsequent rights revocation closes only temporary source and preserves running game/configuration/confirmed journal. No newly failing production case preceded these green checks. Separate initial20/128byte actual-file paths and maximal131072byte actual-file import passed. Seven incorrect counterpart observations rejected; actual current v2 status validates.

Actual built tool roundtrips both golden frame files and131072bytes at both20/128value budgets, with explicit Host fake/no persistent save/radio labels.23malformed/truncated/extra/embedded-NUL/semantic-invalid frame files exit1 with no success output. Earlier framing/metadata CLI and twelve App v2 pairs remain unchanged; App byte/SHA comparison is read-only, no App tests.

Five final builds SUCCESS/exit0: production55,148RAM/1,228,729flash (93.7%), oldUSB21,480/291,881, profile107,328/395,201 plus57,272startup profile heap/allocator/filesystem, isolatedSDK25,176/275,189. Compared with prior framing slice all four embedded targets+56flashbytes and unchanged static RAM; production entry remains the previous controller, new source code compiles but there is no SDK profile dispatch. Last extra running-game case changes test files only; embedded sources are exactly those of the five successful builds. Architecture108files/237includes,67independent headers,189format files passed.

Native32bit/Xtensa size probe: portable receiver72bytes; Host temporary-file object24bytes, plus stdio/OS allocation. Fixed per data call: one <=128byte owned copy, one <=120byte append, at most960CRC bit updates, two original clock/authority observations; failure may add one discard. start has at most one begin and bounded cleanup (up to two discard calls in a failure path); finish at most one seal, reads at most one backend byte; no loops waiting on transport. Own temporary compiler and actual SDK entry frames: start64/data192/finish48/read48/current48/service32/discard32, plus authority/backend/parser/Application/caller chains. No physical time/SMP/stack/heap/filesystem bound is measured.

Evidence: %TEMP%/larp-profile-staging-host-final-20261007.log, larp-profile-staging-builds-final-20261007.log, larp-profile-staging-cli-final-20261007.log, larp-profile-staging-cli-20261007.py and larp-profile-staging-budget-20261007. No new upload, radio, Android, physical storage failure or remote operation.

[Copied FreeRTOS input](profile-frame-queue-v1.md) now reaches the actual source owner with original time/route and one-item dispatch. Explicit profile wire control and the real SDK profile callback remain open.
