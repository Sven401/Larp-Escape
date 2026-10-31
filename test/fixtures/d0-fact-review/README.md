# Independent D0 Fact / COSE signature audit

The generator imports no product codec, serializer or cryptographic adapter. It
encodes deterministic CBOR and COSE_Sign1 independently and signs using the real
`cryptography` P-256 implementation (50.0.2 in the bundled runtime). Public,
synthetic private scalars 1 and 2 are test data, never deployment credentials.
Deterministic signing preserves fixed vectors. An additional randomized valid
signature is preserved explicitly in the generator and independently verified
before regeneration. High-S and low-S variants are both accepted; these and the
additional signature have the same original payload and payload digest.

Actual local evidence: strict C++11 warnings-as-errors build, real Windows CNG
SHA-256/P-256 verification, **225 fixed COSE byte cases, 22 positive**, 1107
shorter-prefix/reuse pairs, 1109 every-byte-source-failure/reuse pairs, 15
hash-failure/reuse pairs, seven crypto/reset-failure/reuse pairs and three
2048/2049-limit-EOF/reuse pairs; zero mismatches. The two audit C++ files pass
repository clang-format 19.1.7. Mandatory native production-PSA CI qualification
remains pending and must not be inferred from the CNG result.

The accepted cases cover all three Fact types, full positive uint64 generations
and sequences, eight unsorted distinct dependency references, eight ordered
nonoverlapping ranges (adjacency allowed), and widest fixed Fact shapes. All ten
payload labels have individual missing, duplicate, unknown replacement and
nonminimal-key cases. Other negatives cover COSE tags/arrays/bstr widths, map
order, integer/length minimality, unknown fields, null/bool/floats, identities,
header algorithm/kid, source failure, dependency/range shape and limits, DER
signature rejection, wrong AAD and invalid P-256 points/scalars/signatures.

Each successful call checks exact original bytes, protected/payload/signature
slices, every copied Fact field, canonical 77-byte COSE_Key kid, independent
payload SHA-256 and the exact Sig_structure hash passed to the real checker.
The AAD is exactly 13 bytes `larp.event.v1`. Source and candidate-key bytes are
overwritten after return; FrozenFact must retain owned copies. Every failure
clears the previous output, all published byte slices, digests, fields and counts.
The same output, hash wrapper and crypto wrapper are reused for healthy calls.

Hash and crypto failure decorators are negative-only seams around real backends.
They do not count as positive signature evidence. PSA handle/import/destroy
lifecycle faults are a separate reviewer-owned suite in `../d0-crypto-faults`.
An invalid curve point may be diagnosed by the backend during import or during
verification; the fixture accepts InvalidKey/InvalidSignature for that distinction,
never signature success. The candidate is not a trusted identity or origin grant.

The fixed Fact grammar cannot make a valid padded 2048-byte message. The exact
2048-byte source contains a shorter real message plus invalid trailing bytes and
must reach EOF then reject Cbor; 2049 rejects LengthLimit. Source failure at byte
2048 wins before a possible 2049th byte. Once actual overlength is established,
reading stops instead of draining an unbounded source. These are read-limit
checks, not positive maximum-size packet or ESP transport/stack claims.

Run from repository root:

```powershell
& C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe test/fixtures/d0-fact-review/generate.py
python test/fixtures/d0-fact-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

`generate.py` requires the real cryptography package; `run.py` reads preserved
vectors and uses only Python's standard library. It also supports
`--binary <externally-built-executable>` without compiler discovery, and
`--no-faults` for the fixed corpus only. `cli --layout` reports host object sizes.
For the mandatory production target define `D0_FACT_REVIEW_PSA`, include the
PsaPackageDigest and PsaD0FactCrypto library directories, and link both actual
adapter .cpp files plus the actual PSA library. The default Windows review path
uses BCrypt; the optional non-PSA Linux path uses OpenSSL 3 (`-lcrypto`). The
OpenSSL fallback is provided, not independently run in this Windows audit.

Result success is only `SignatureCheckedUnauthenticated`. No installed trust,
replay handling, event/run/package admission, GM/override authorization, receipt
handling, durable acceptance, transport Verified or planner CurrentSolved claim
is made. Unsupported message kinds remain explicit rejections. Existing D0
model fixtures are read-only references and are not renamed crypto evidence.

Own files: README.md, generate.py, vectors.json, Support.h, cli.cpp, run.py.
No product files, build wiring, original fixtures, commits or pushes are owned by
this audit. Host32 GCC 5.1/O0 sizes: FrozenFact 2888 bytes, Fact 760, Result 12;
caller-owned storage is not a total stack/ESP budget qualification.

Final frozen source audit, 2026-10-31 (Windows working-tree raw SHA-256):

| File | SHA-256 |
| --- | --- |
| D0Fact.h | `6166707435763174fa9cdf53ae40a8823616e03dd5cf65a0ca95c39f644d38ef` |
| D0Fact.cpp | `c259fceffb730d5e392c88f38564805d7d1906e3052937a04e423d2373f94403` |
| PsaD0FactCrypto.h | `ca7230c0e6c009c1b9ce68370462c671ce3b55030617b0b967bb867c3b0c6d97` |
| PsaD0FactCrypto.cpp | `3026da90503e5db2ff2cd0e3b4218f26c412966a68f5669d8a768fa0c3e98356` |

Fresh full audit after freeze: all counts above pass. GCC 5.1/O0
`-fstack-usage` reports a largest static core frame of 240 bytes in checkFact;
this excludes caller-owned FrozenFact, callback frames and total call stack.
