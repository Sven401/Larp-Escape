# Own partition-local generation tests

Actual fixed record codec, actual Esp32LocalGeneration singleton/NVS backend and
unchanged actual native boot-clock/entropy sources. SDK operations are explicit
fault seams, **not** hardware or power-loss evidence. Strict C++11/O2/Werror:
16 separate processes / 1424 assertions.

All fixtures start from a pre-seeded positive record. The product contains NO
first initializer, seed, erase or repair path. Missing/Corrupt/Overflow close
reservation. Tests include full UINT64_MAX result, same Boot128 still allocating
old+1, private singleton construction, no retry after success/failure, exact
partition/namespace/key, complete exclusive CAS, close/new-handle readback,
prewrite failures, ambiguous set/commit/readback and reentry with no early output.
Every byte of a valid 64-byte record is independently flipped and rejected.

`run.py --compiler <g++>` or `--binary <target>` runs all cases, each in its own
OS process. No in-process reset seam is introduced to the product. The separate
independent campaign adds retained-medium multi-process and hostile SDK cases.

Record layout v1: magic LARPLG01[8], LE version1[2], LE length64[2], zero[4],
positive LE highwater[8], original boot128[16], zero[20], LE CRC32[4]. CRC covers
the first60 bytes; it detects corruption and proves NO authority/anti-rollback.
No device/station ID is inferred from these physically partition-local bytes.

Actual SDK calls use ONLY existing larpkeys at0x310000/0x6000 and namespace
larp-local-v1/key boot-v1, never default nvs. Existing access-v1 is never named,
read or modified by wrapper code. NVS initialization/GC can internally mutate
shared partition pages: namespace separation is NOT physical fault isolation.
Commit+new-handle readback is a software result, not a cold/powerloss proof.
Missing/corrupt basis closes reservation. A valid older whole-flash snapshot
can be accepted and incremented: this code does NOT detect rollback and makes
no anti-rollback claim. Such a lifecycle supplies no authority for automatic
continuation of the same event/run; compatible recovery remains a separate
requirement, as does qualification of firmware changes.

Both native app_main paths attempt reservation after startup entropy is off.
Missing reservation logs only a Local-generation warning; existing communication
continues. Numeric reservation creates NO Local input/sequence, station binding,
Control, GM/Running authority or setup capability. Original producer/first
provision remain open. No upload/device operation is performed by these tests.

The native entry points now execute real larpkeys initialization and, when a
valid record is already present, a real counter update. They are no longer the
previous clock/queue-only startup with respect to flash. Even a missing COUNTER
RECORD does not establish write-free NVS init. No upload permission follows
from earlier probe assumptions or these source/host checks.
