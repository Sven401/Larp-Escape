# Complete profile source import v1

Status: bounded source envelope, private-draft importer, actual Application Host
path and independent tool counterpart implemented. Network data framing/spool,
registered SDK endpoint and remote apply are still open. This endpoint is not
routed through the current game-command GATTS queue.

## Contract

A profile import requires an exclusively owned complete bounded source whose
End is final and whose reads return Byte/End/Failure within a bounded time.
The synchronous profile parser cannot interpret an empty live network FIFO as
End. Transport must finish/seal a bounded source first, or implement a separate
incremental parser. This slice does not allocate a document spool or third
Profile; the source is imported into the existing inactive private RAM slot.

The source envelope is exactly20bytes, little endian:

|Offset|Width|Value|
|---|---|---|
|0|1|Version1|
|1|1|Kind5, profile source metadata; not a data fragment|
|2|2|Nonzero transfer correlation ID|
|4|4|Exact source byte count1..131072|
|8|4|CRC32/ISO-HDLC of every raw UTF8 byte, including whitespace|
|12|4|Expected draft revision0..4294967294|
|16|4|Expected configuration revision1..4294967295|

CRC uses reflected polynomial0xedb88320, initial/final XOR0xffffffff.
It detects accidental corruption and is not a signature/authentication proof.
The correlation ID is not an authorization/replay mechanism. Current original
route plus revision checks control acceptance; failed imports also consume a
draft revision, so a retry must use freshly observed state.

Decoder rejects any other size/version/kind or invalid bound and preserves its
destination on failure. Encoder leaves written0 and the output untouched on
failure. Kind5 is explicitly rejected by the game-command frame queue; it
must not be confused with the existing type1..4 document/receipt contracts.
JSONv2 and the App snapshot are unchanged.

## Original authority and stopped application

`importProfileSource` runs in the same sole synchronous Application owner.
Envelope and original Principal/Epoch/GM are copied before any source/authority
call; client metadata supplies no authority. receivedAt and the injected clock
must use the same trusted uint32 millisecond clock as the producer. Original
callback/transfer start time is retained. The source allowance is120000ms
absolute, including any prior wait; the2000ms game-command deadline is not
reused. This is a policy bound, not measured RF throughput or parse time.
Future timestamps fail closed; wrap arithmetic assumes regular work within
one full clock turn.

Before consuming input, require exact current original GM, available authority,
unexpired source, Ready/initialized/active Application, stopped game and both
matching revisions. Busy/running/stale/invalid/unauthorized/expired input is
rejected without source reads or draft edit. The actual Controller rechecks
its own stopped/busy/draft admission.

Each bounded underlying read is surrounded by clock/authority checks. Extra,
short, failed or CRC-mismatched input never becomes parser End. Valid End is
published only after exact length/digest/original authority/deadline pass.
The real parser/validator then determines whether the private draft is valid.
No game tick, activation, hardware switch or journal save occurs here.
Active configuration and both durable records remain unchanged on every case.

After parsing/validation, original authority/deadline are checked again before
issuing an internal receipt. Closure between checked EOF and this last check
can leave a parsed local draft, but produces NO accepted-route receipt and no
apply/save. A later local admin can deliberately replace/use that local draft;
remote apply must retain and recheck the internal original route AND accepted
draft/current configuration revisions. Remote apply is not implemented by
this function. Never accept a client-deserialized receipt as server proof.

## Resources and actual vertical evidence

For declared N<=131072, at mostN+1underlying reads, including final EOF or first
extra byte. At most2(N+1)+2clock/authority checks on the successful path;
maximum262148. Each authority read uses the existing channel's short lock;
there is no blocking FIFO read, retry, heap allocation or document buffer in
the importer. Source implementation and parser validation must still have
bounded cost. Actual scheduling/microsecond/stack reserve needs measurement.
The existing two Profile allocations remain57,272Bytes plus allocator.

Thirteen new native functions cover real private draft import with a decoded
20-byte envelope, CRC known vector, exact bytes, active/stored immutability,
codec boundaries/output preservation, wrong rights/revisions/busy/running,
short/extra/CRC/IO/malformed/schema-invalid sources, revoke during byte/EOF,
caller metadata mutation, late revoke and expiry, maximum128KiB and wrapping
clock, and rejected reentrant start/edit/tick. Host source fixtures are built
with a temporary authoring Profile which is destroyed before import; the
firmware importer owns no third Profile. Only the Application owns its two.

Golden hint source20220raw UTF8 bytes, CRC0x73999d9b, transfer1, expectedDraft2,
expectedConfiguration1; successful receipt carries draft3 and original route.
The independent Python counterpart uses zlib CRC and canonical profile schema,
rejects13 malformed variants. CLI `--profile-envelope FILE DRAFT_REV CONFIG_REV`
validates the actual file in one Host-only Profile, checks close before output,
then emits the exact20-byte uppercase hex envelope, correlation1. It never
activates/saves or prints metadata for invalid input/revisions. App tools must
supply fresh revisions. Source golden and envelope have explicit LF attributes
because Windows core.autocrlf must not change checksum-covered bytes.

138/138final native functions/Exit0:
`%TEMP%/larp-profile-source-host-final-20261007.log`.
Architecture102files/228includes,64pure headers,176format files passed.
Actual target compiler sizes: envelope20/receipt40/result124Bytes.
Own -Os stack frames decode64/encode32/CRC32/import320/guardRead48/guardCurrent48,
excluding parser/Application/channel/source/caller chains.
Five final builds SUCCESS/exit0:
`%TEMP%/larp-profile-source-builds-final-20261007.log`.
Production55,148RAM/1,228,529Flash(93.7%),oldUSB21,480/291,785,
profile107,328/395,065 plus57,272Profile heap/allocator/filesystem,
isolated GATTS25,176/275,093. Profile image64Flash bytes above d97cf42;
production entry is still the old controller. Import bridge is compiled in
SDK objects; the radio/production entry does not dispatch this API yet.
Actual SDK object entry frames import320/guardRead48/guardCurrent48Bytes,
plus parser/Application/channel/source/caller chains. Previous quick regex
initially misread hexadecimal entry0x140 as0; direct disassembly confirmed320.
Actual CLI matches all20golden bytes; six invalid revision inputs exit2/no
output, three invalid sources exit1/no output. Prior CLI fixtures unchanged.
All existing frame/schema/generator checks remain green;12App-v2 schema/fixture
pairs freshly byte/SHA256 equal; no App checks. First new host compile used a
nonexistent guessed channel header; corrected to actual ConfiguredProtocol
before passing138cases. No failure of the previously committed d97cf42.

## Next connection and open acceptance

Implement bounded profile data framing and an exclusive temporary source with
exact original route/clock, then controlled authorized apply and journal save.
A client needs distinct imported/activation-pending/saved observations; an
import receipt cannot be displayed as a persisted running configuration.
The current source gate supplies that import boundary, not a radio transport.
SDK lifecycle/credential verifier/response bearer, provisioning/role-key
persistence, real device owners/production reserve and physical RF/Android/SMP/
time/stack/heap/energy/flash-failure acceptance remain open. No new upload,
format/erase, remote CI, push, PR or merge.

## Following internal apply slice

The source-only138-case/receipt40/result124 measurements above describe129d2d5.
The current receipt is privately issued and bound to the exact Application;
receipt44/importResult128Bytes. [Authorized apply](profile-apply.md) records
the actual guarded activation/journal/current-status path,150cases and
remaining wire/network acceptance. Source envelope20Bytes is unchanged.
