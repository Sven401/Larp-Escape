# Independent Effect v1 byte/header audit

Pure representation/header proof only: no actual Owner, journal write/reopen,
IO, rights, actuator handoff, sensor observation, D0 observed, or hardware result.
Handler Returned is not physical observation. Root own tests are separate.

`generate.py` constructs independent canonical CBOR directly, without importing
any product encoder. `vectors.json` preserves the entire original corpus and
is compared to regeneration before each run. Eight positive maps cover Claim
None and Result Returned/Failed/Unknown at indexes 0 and 31. Every Fence contains
all width boundaries in exact field order: 0,23,24,255,256,65535,65536,2^32-1,
2^32,2^64-1. Parent, catalog and boot bytes are checked independently, then the
ENTIRE product-encoded frame must equal the fixed byte oracle.

46 hostile maps include missing/duplicate/unknown/out-of-order keys, wrong
identity/digest widths and null IDs, exact32 and oversized intent indexes,
wrong domains, unknown/incompatible phases/results, wrong Fence length, tag,
indefinite/nonminimal encodings, negative/null/bool index, trailing bytes and
oversized raw source. All are rejected without partial decoded publication.

3160 C++11/-O2/-Wall/-Wextra/-Wpedantic/-Werror assertions additionally cover
every shorter prefix and every short output capacity of all eight maps; exact
capacity and reuse; all 30 phase/result combinations; source destruction;
full input/output/pool-wrapper aliases with preflight-preserved values; and
zero/null input clearing. A recognized domain-only prefix still fails complete
decode, so classification is never treated as semantic validation.

Actual ProgressRecord/IntentReservation checks exercise exact phase ID/kind,
index, zero/nonzero/equal/parent ID collisions and nonzero issue allocation.
Physical journal revision is deliberately UINT64_MAX while logical progress is
zero: these unrelated counters are not equated. This helper intentionally does
not establish original catalog/boot/Fence authenticity or phase sequencing;
those remain the actual Owner's additional obligation.

Reviewed frozen product SHA256:

- RulesProgressEffect.cpp: da108eee46c91194646520ddb2600c6882c0fe066fb5e2258196d53da82066bf
- RulesProgressEffect.h: 09f72004a76c116aba6f903dc9107b1b95242ca9fab8cc64b74d0401796ca975

```powershell
python -B test/fixtures/rules-effect-independent-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

Own separate build folder: private/rules-effect-independent-review.
`--binary /absolute/path/to/executable` skips compilation on either host.
Link main.cpp with the actual src/DeviceRuntime/RulesProgressEffect.cpp only.
No crypto backend or synthetic positive signature is used or required.

Owned files: main.cpp, generate.py, run.py, vectors.json, README.md.
No product or shared build-wiring changes by this independent reviewer.
