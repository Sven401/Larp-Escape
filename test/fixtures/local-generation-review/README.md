# Independent physical local-generation audit

Status 2026-11-01: actual strict C++11/O2/Wall/Wextra/Wpedantic/Werror build,
**561 fixed 64-byte codec vectors, 68 fresh processes, 7152 assertions**.
Only this own fixture area is written. The actual core codec, singleton owner,
private NVS store, BootClock, startup entropy adapter and Identity.cpp are linked.
Only narrow NVS/partition/mutex/entropy/timer SDK functions are stubbed.

This is physically partition-local allocator metadata. No device, station,
actor, D0, authorization, OriginalInput, sequence wrapper, debounce, Running or
Control producer is inferred. The software medium and exclusive single-worker
startup profile are explicit. No real NVS flash, power-cut, anti-rollback,
partition-wide recovery isolation, SMP, ISR, entropy-quality or hardware claim.
NVS initialization may itself recover/GC the shared partition; these tests do
not claim otherwise. Real pinned-SDK images remain a separate required gate.

## Fixed byte oracles

Independent Python generation preserves all 64 bytes, little-endian uint64,
LARPLG01/version/length, zero reservations and IEEE CRC32. CRC is corruption
checking, not authenticity. The 561 cases include eleven valid records (all
uint64 boundaries through maximum, and last-boot-byte-only), 512 independent
single-bit corruptions, 36 structural changes with repaired CRC, and zero
counter/zero boot with valid CRC. Actual decode compares full value/boot bytes;
actual re-encode must equal the complete independent record. Rejection clears
all destination fields and preserves source bytes; invalid encoding clears
all output bytes. records.json is preserved along with generate.py, and the
runner checks that regeneration equals the fixed corpus.

## Actual store/owner cases

The entire initial read, full-byte software CAS reread, set, commit, close,
NEW read-only open, exact-length readback, close and mutex release is checked
in order, under the same zero-wait software lock. The exact partition is
larpkeys at0x310000/0x6000, erase4096, unencrypted/writable; namespace
larp-local-v1, key boot-v1. Every partition field, missing partition, mutex
create/take, init, three opens and all six length/data read stages are covered.
Each read stage has error, missing, short and long outcomes. Missing never
creates/provisions a namespace/key. No erase/deinit/default-partition APIs are
available to these linked sources in the seam.

Set and commit failures model either no durable effect or a durable effect;
post-commit open/read/mismatch failures remain Uncertain. nvs_close is void:
there is no invented close-error status. Its actual ordering/handle release
and subsequent fallible reopen/readback are verified. Callback reentry at
all SDK stages cannot expose partial values or recursively reserve; all
returns release handles/lock. Repeated aliases of the singleton have no new
IO/entropy/reservation after success OR any failure. Public construction,
copy/move/assignment and independent store construction are compile-time denied.

Only the test harness pre-seeds existing records; the product has no initializer
or repair path. Full uint64 boundaries reserve max from max-1, then max refuses
Overflow without wrapping or writing. A corrupt/short existing record refuses
without repair. Identical random Boot128 labels across three ACTUAL NEW
processes still advance the persisted bytes41 ->42 ->43 ->44. Uncertain
processes cannot retry; a new process reads the exact software-medium bytes
that the explicit fault actually persisted, then reserves one higher. This
is a process/persistence SOFTWARE contract, not a power-loss simulation proof.

Clock loss deliberately injected after successful commit leaves that counter
advanced but exposes no reservation and permanently returns Unavailable for
this process, as specified by the actual owner post-clock check. It is not
misreported here as Reserved or as the store's Uncertain status. No caller
can reset/reconstruct either singleton to bypass the closed result.

## Reproduce

```powershell
python -B test/fixtures/local-generation-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

Own build private/local-generation-review creates missing parents. Portable
--binary <absolute executable> requires no compiler, runs all561 vectors and
all68 separate processes, and retains raw observations.txt. Six product-source
hashes are checked before/after each campaign. Product wire/record bytes are
never newline-normalized. No positive device provisioning or live authority.

Build main.cpp with actual:

- src/DeviceRuntime/LocalGeneration.cpp
- lib/Esp32LocalGeneration/Esp32LocalGeneration.cpp
- lib/Esp32LocalGeneration/Esp32LocalGenerationStore.cpp
- lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp
- lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp
- src/Companion/Identity.cpp

Include our stubs FIRST, include/ and the three corresponding lib directories.
No PSA macro/crypto adapter required. CTest uses Python -B run.py --binary
<target>; no shortened modes. Real SDK targets use their actual SDK headers,
not these deliberately limited stub headers.

Reviewed raw Windows source SHA256:

- LocalGeneration.h 218bac05b9288899bfc5e46c4681cf6ccd078a4beb8d3f284549d8c1b5c2d8a2
- LocalGeneration.cpp 63103dbddbf605de8ba3800596f89c18cc306714af851fe5364b589b7493797a
- Esp32LocalGeneration.h 6df67aa6303c92be6b5e4983bcb76a36baa1a84db60ce69cef2dbb25f633152b
- Esp32LocalGeneration.cpp d37a5b59fb8ab7c47aeeca0f523216dcf0e6978eeb8b99a213ec0492bad3c21b
- Esp32LocalGenerationStore.h fbdbbb91db1f0a51ceb8e54d901f6e9348211ef85e673c775e02a2b555ac00dd
- Esp32LocalGenerationStore.cpp bf79fb63ab28b5521fe2862ed7c0abb4b20cc2d4931a652786d15d7dc60b7219

Owned thirteen files: main.cpp, run.py, generate.py, records.json, README.md;
stubs/esp_partition.h, nvs.h, nvs_flash.h, esp_timer.h, esp_random.h,
bootloader_random.h, freertos/FreeRTOS.h, freertos/semphr.h. No product/wiring
edits, commit, push or device action by this reviewer.
