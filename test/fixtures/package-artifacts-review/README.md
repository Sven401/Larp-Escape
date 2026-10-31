# Independent artifact validation audit

These fixtures and drivers are audit-owned. `generate.py` contains independent
canonical JSON and minimal deterministic CBOR encoders; it imports no product
codec or compiler. `vectors.json` preserves 87 complete raw bundle inputs,
including 31 positive sets, independently written complete canonical station IR
bytes, and actual legacy profile bytes. The full actual artifact validator passes
all 87 vectors with real BCrypt SHA-256, plus 33 source-failure/reuse pairs,
55 injected crypto-failure/reuse pairs, 55 combined exact-EOF source-priority/reuse
pairs, 314 shorter-CBOR-prefix/reuse pairs, 632 every-byte-CBOR-failure and
paired hash-begin-failure/reuse pairs, and three hash-alias/failed-open/reuse seams. Rejected results have no ready
Program and zero published deployment/content digests. Error source, module,
artifact/content-resource identity and exact offset are checked for source faults.
Native production-PSA qualification remains a separate gate.

The fixtures cover two/eight stations, distinct and repeated box IDs, authoritative
nonfirst epoch selection, full uint64 binding generations, repinned malformed
CBOR, source/digest/length mismatches, exact regenerated IR tampering, paths,
permission/manifest closure, lexical requirement majors, compatible multi-role
paths, compatible/incompatible physical locator aliases, station-local stable rule IDs,
T2-only reads without source emission, and value/add-old/timer-arm static dependencies.
Actual legacy extras and legacy/resource aliases use `readDraftProfile`; LF,
whitespace and Unicode-escape bytes valid in the old grammar are accepted. The
legacy reference derives from `docs/configuration/hint-profile.json` without
changing that source. Its LF raw SHA-256 is
`3cfbcd25be2bc42d64b9a602498b2a1ee141ab72a0e542f668396f843d3f5042`. A 262145-byte actual audio stream is accepted. uint32-max
media length is tested by parsing followed by immediate failure/short source;
this is no claim about transferring or performance at 4 GiB.
No device capability, authentication, inventory, fallback authorization, or real
hardware behavior is claimed.

`metadata-vectors.json` preserves nine exact independently counted source-profile
cases. The approved MetadataSourceProfile-v1 counts raw canonical content and
distinct pinned modules once, excluding only complete array values (brackets
included) at content.stations[].rules, module.rules and module.constraints.
Instances/maps, unused definitions, repeated pin text, escaped strings and UTF-8
bytes count. This is a maintainer operating profile, separate from P0 validity.
The exact 32768/+1 cases test the observer flag, not rejection by the existing
module compiler. The outer artifact validator must enforce its profile result.
The metadata runner also checks six content/module source-failure/reuse pairs.

Run from the repository root on Windows:

```powershell
python test/fixtures/package-artifacts-review/generate.py
python test/fixtures/package-artifacts-review/run_metadata.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
python test/fixtures/package-artifacts-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

An existing native build can use `run_metadata.py --binary <metadata-executable>`
and `run.py --binary <artifact-executable>` without a compiler argument. Define
`PACKAGE_ARTIFACT_REVIEW_PSA` when compiling the audit C++ sources and link the
actual production `PsaPackageDigest` implementation and PSA library. The default
Windows backend uses actual BCrypt SHA-256. Positive checks never substitute a
fake digest. `Support.h` copies caller-pool setup from the prior independent
planner audit and reuses its real-hash/source helpers; it contains no precomputed
runtime binding projection.

`run.py` checks the derived selected Program context, raw deployment/content
hashes, exact metadata count, provider close balance, and rejected-result
publication state. Hash begin/update/finish/wrong-result-length/abort failures
are injected in every baseline compiler/artifact hash context, with hit counters;
positive bytes always go through the real backend. Both hash objects, validator
and caller pools are reused. Failed opens returning a non-null source still close
that source. An aliased hash argument clears an initially ready staging Program.
Begin/middle/exact-EOF source failures for every source in the
baseline bundle are followed by healthy calls on the same validator and storage.
`--no-faults` runs only fixed complete bundles for development diagnostics.

Audit source files are `generate.py`, `cli.cpp`, `Support.h`, `run.py`,
`metadata.cpp`, and `run_metadata.py`; the two JSON files are preserved fixed
vectors. Neither original P0 fixtures nor product/build wiring are modified here.

Host sizing (GCC 5.1, 32-bit Windows, C++11, O0): Validator 31608 bytes,
Result 688, Catalog 252, metadata observer 1656, Program 336, legacy Profile
28636. Largest new product single static frame from `-fstack-usage` is 1232
bytes (Validator::validate); dependencies 1200, expanded 1104, metadata token
128. These exclude caller pools and accumulated call/recursive stack. They are
host measurements, not ESP task-stack or real-time qualification. The CLI offers
`--layout` to reproduce object sizes.

Scope qualifications: T1 or T2 metadata permits static subject reads/triggers;
actual source emission requires T1. T2 is no proof of a real override grant, and
fallback/shared-latch authority remains unsupported. SourceFailure wins over
crypto failure when encountered while reading the bounded artifact. Invalid
oversized/overlong sources may stop at their declared/profile bound; the validator
does not drain an unbounded bad source to search for a later failure. Virtual path
checks are not filesystem/symlink/archive containment proof. Integrity checking
is not authentication, activation, device inventory or durable commit proof.

Final formatted-source working-tree audit (2026-10-31, Windows raw SHA-256):

| File | SHA-256 |
| --- | --- |
| PackageArtifacts.h | `212d8ffff5ffa30b8232cb39da24a16fa20ea43ae08eef58efda21830fe7f3dc` |
| PackageArtifacts.cpp | `d0dfcd88cef84fc951f8dcc8bbf3c2589e2432c8ebdbaef7d49ade2d90252788` |
| PackageMetadata.h | `38c1039442b98e0db57c8682a421ea44760ba0d4932f4667996448dd7f9196c4` |
| PackageMetadata.cpp | `146fceb7cd338e65555b94a7fe8559ad1eb73b9e0958dffb5faf1970ca8a1854` |
| PackageModules.h | `84d35afb78e392831214631929baa06a02a373f1dde69ecdd2a0a55bab8686b7` |
| PackageModules.cpp | `6d35c57ba7babfb6540841d7eb82fd7ded54972accd63dd6c287a51c915d83c7` |
