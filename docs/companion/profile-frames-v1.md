# Complete profile data frames, version 1

A narrow firmware/host/tool path. This contract represents a **complete, exclusively owned, immutable source**. It does not implement a live callback collector, temporary storage, radio transport or a client apply command. The four-place command queue is not storage for a complete profile. Kinds5/6 are rejected by that command path.

## Wire values

Begin with the exact20-byte kind5 [source envelope](profile-source-v1.md): version1/kind5/transfer u16/total u32/rawCRC32 u32/expectedDraft u32/expectedConfiguration u32, all little endian. Then emit kind6 data values:

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 1 | version=1 |
| 1 | 1 | kind=6 |
| 2 | 2 | positive transfer id, little endian |
| 4 | 4 | raw source byte offset, little endian |
| 8 | 1..120 | unmodified UTF8 source bytes, including whitespace/newline |

Data values are9..128bytes. Offsets are32-bit and must be below131072; payload must remain within that global limit. Sequence begins at0, every next offset equals the previous offset plus payload size, every id equals the original metadata id, and exact final size equals the original metadata total. No gaps, duplicate retry, reordered values, foreign ids or extra values. Metadata fixes the immutable length, CRC and revisions; there is no repeated total in data headers. Transfer ids correlate data; they are not authentication, replay protection or a GM assignment. Transport callbacks must capture and retain the original trusted connection/route.

Codec failure preserves the destination and outputs written=0. Successful decode borrows caller payload; only the complete reader owns a fixed copy. Encoder supports overlapping payload/output by moving payload before writing the header. Caller pointers must refer to valid owned readable/writable ranges.

## Actual complete reader and import

ISealedProfileFrames copies one whole value into fixed128-byte storage. This interface is an explicit complete-source precondition, never an empty live FIFO. End is final with written=0; backend failure is Failure. FramedProfileReader copies the metadata, is noncopyable and exposes bytes from one owned value. It copies at most one new value per readByte; a final extra source read must prove End. Early End, malformed count/header, wrong transfer/position, excess data and backend failure become permanent Failure. Repeated terminal reads leave the caller byte unchanged and do not read more source values.

This reader is fed to the actual importProfileSource. Its existing guards check original GM rights and absolute120000ms before/after source reads, exact raw length/CRC and post-parser rights/time. It imports only into the existing inactive private slot, issues the existing Application-bound receipt and can use the existing controlled apply/journal path. No third Profile, full document buffer, reader heap, added tick or automatic game start. Host Fake frame vectors are test-source allocations; firmware reader storage is fixed.

The120000ms import policy remains unchanged for this complete-source path. It is not measured transfer/parse time or a promise that the largest profile fits over radio. A future live transfer must specify its own admission/completion timing and retain original connection/deadlines without silently restarting them.

For N raw bytes and negotiated data value V, payload is V-8 and data frame count is ceil(N/(V-8)); add one20-byte metadata value. Tool supports V=20..128, default20; metadata itself needs20bytes. Golden20220-byte source requires1685data values at20bytes or169at128, plus one metadata value each. Maximum131072bytes requires10923or1093data values respectively. These are size calculations, not measured RF throughput. Reader calls: at most N+1 readByte calls for complete sequential consumption and frameCount+1 backend calls; the outer import guard has its existing fixed byte/auth/clock bounds. A synchronous backend must itself have bounded execution.

## Tool and independent counterpart

profile-tools --frame-profile FILE DRAFT_REV CONFIG_REV [VALUE_BYTES]

The tool validates the exact complete file first, emits kind5 then kind6 uppercase hexadecimal lines, and checks length/CRC/EOF/close again after a second file pass. Caller requires exit0 and exact complete reconstruction/CRC. A concurrent file change or output failure may leave a partial stdout sequence with nonzero exit; it is never a successful transfer. Invalid arguments/revisions/value budget exit2 without output; invalid original profiles exit1 without output. This is an authoring tool, not an authenticated phone or network receiver.

Actual native fixtures [20-byte values](fixtures/profile-source/frames-20-v1.txt) and [128-byte values](fixtures/profile-source/frames-128-v1.txt) are encoded from two independent initial Application fixtures. Both import through the actual reader/private slot and complete actual controlled apply with synthetic owners and journal model, stopped game. Python independently unpacks32-bit fields, reconstructs exact source bytes and checks zlib CRC/schema; malformed sequences are rejected.

## Remaining acceptance

Live raw callback/FreeRTOS transfer owner, bounded temporary source storage and recovery/cleanup, original lifecycle attribution, profile operation/answer wire protocol, trusted identity provisioning/verifier and real bearer. Production connection, real devices, flash reserve, RF/Android/SMP/stack/heap/timing/energy/storage failures are separate. No new device upload or radio run is implied.


## Measured local result, 7 October 2026

162/162 focused native functions passed, twelve added here. Complete framed128KiB source imported into the actual private slot,1093data frames plus one source EOF call. Separate20/128-byte initial fixtures both applied and reached confirmed journal model with stopped game. Initial new revoke/expiry loop incorrectly tried to reopen the already suspended epoch; corrected the test to a fresh epoch before final success. No fault demonstrated in the prior committed firmware.

Independent counterpart:1854data values plus two metadata values, exact20220bytes each,20malformed sequences rejected. Actual built CLI matches all golden encoded values, default20 matches, maximum131072bytes gives10923/1093data frames. Ten invalid argument/revision/value budgets exit2 and four invalid original profiles exit1, all without output. Existing metadata-only CLI values unchanged. Hex line endings are presentation; raw source bytes/CRC are preserved.

All five final builds SUCCESS/exit0: production RAM55,148/flash1,228,673 (93.7%); old USB21,480/291,825; profile107,328/395,145 plus57,272startup profile heap/allocator/filesystem; isolated SDK25,176/275,133. Compared with previous apply slice: flash+144/+40/+40/+40 in the four embedded targets, RAM unchanged; production entry still uses the previous controller. No RF or new production dispatch is claimed. Architecture106files/233includes,66independent C++11headers,182format files passed. All previous fixture/schema/generator checks passed; twelve App v2 pairs freshly byte/SHA256 equal, App code/checks untouched.

Native32-bit and temporary Xtensa object sizes agree: ProfileDataFrame16/FramedProfileReader172bytes. Own temporary Xtensa compiler and actual pinned SDK object entry frames: decode48/readByte64/encode32, plus callees/parser/Application/authority/backend/caller chains. This is not physical stack/time/heap reserve. Evidence: %TEMP%/larp-profile-frames-host-final-20261007.log, larp-profile-frames-builds-final-20261007.log, larp-profile-frames-cli-20261007.py and larp-profile-frames-budget-20261007.
