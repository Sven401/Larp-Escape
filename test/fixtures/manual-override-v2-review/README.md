# Independent manual override v2 review - source/writer checkpoint

This is a deliberately limited checkpoint of the complete manual-release task.
The concrete ManualOverrideOwner, actual Companion authority adapter, full
RulesProgressCodec/journal coordinator and qualified recovery are still absent.
No privileged admission is constructed, no Test Friend is defined, and no
positive manual-release/GM/D1/publication claim follows from these tests.

## Actual current evidence

- 61 complete raw artifact sets: 20 integrity/Program positives and 41 negatives,
  through the actual hash/compiler/IR comparison/Artifact/ProgramBuilder path.
- Every accepted set checks actual derived group/fallback names, stable IDs,
  distinct generations and CBOR-derived fact permission masks.
- Negative kernel ports only: default empty OverrideAdmission is rejected for
  all 20 accepted programs; four arbitrary nonzero epochs are rejected for the
  structurally valid explicit empty remote epoch list. No fake admission exists.
- The unchanged v1 review runner was freshly executed against this source:
  six original traces / 24 full state transitions, 54 independent scenarios and
  typed DAG 4096 accepted / 4097 OperatingProfile rejection all pass. Its genuine
  old SubjectBinding.epoch convenience API remains default epochsComplete=false.
- Independent WriterFaults: 22 genuine signed roundtrips, 44 copied/aliased-input
  roundtrips, and 36 wrong-key/hash/verifier-cleanup rejection/healthy-reuse pairs.
  Python P-256 independently verifies every success. The wrong-key seam produces
  a mathematically valid key2 signature over the original key1-kid Sig_structure;
  it is verified under key2, invalid under key1, and the actual writer rejects it.
  All failures clear workspace/output and the returned public-key copy. Successful
  returned XY is exactly the originally configured key1, including input aliasing.
- Author-owned d0-writer-review was independently run: 22 real signed payload
  roundtrips plus 14 rejection/reuse pairs pass. PSA signer lifecycle fault shims
  remain separately reviewer-owned and are no signature-success evidence.

Local crypto evidence is actual Windows BCrypt SHA-256/P-256. Native production
PSA targets are mandatory pending gates. No OpenSSL/PSA result is inferred from
this Windows run. Input-copy callback mutation is a defensive fault seam;
input=destination.fact is an explicitly supported real writer API alias.

## Fixture provenance and scope

BaseFixtures.py preserves only pure independent helpers from the committed prior
package-artifacts-review generator; importing it performs no generation and has
no product codec dependency. generate.py owns an independent limited expansion
oracle for these fixtures and constructs actual canonical source/module/profile
JSON, actual minimal CBOR binding bytes and all raw resource files with real
SHA-256. No product serializer, DOM, runtime projection or private owner admission
is supplied to Validator. The driver reports the Program it actually derives.

Coverage includes package.v2/module.v1+v2/rules.v2 gates, real T1+T2 permissions,
shared IDs only within exact groups, valid differing priorities/generations,
ordinary v1 wrapper importing a v2 child, nested and escaped qualified names,
485-byte generated names, real uint64 binding generation distinct from both rule generations, renamed modules, unused definition checks, matching
ordered actions, no effect-only/duplicate/unrelated producer exceptions, physical
alias conflict inside one variant, 8/+1 actions/emissions, and exact static unions
of both guards. Static union sizes9/10 remain structurally valid; their actual
runtime witness/new-fact limit is a later concrete Owner test, not assumed here.
The phase currently has no additional v2 every-byte source-fault campaign; the
existing v1 artifact/module parser/source/crypto regressions remain separate.

The new kernel helpers are explicitly negative misuse ports, not a trusted fact
producer, current GM permission, actual origin signer or D1 flash proof. Existing
P0 v1 traces are unchanged; these cases are administration/guard counterexamples,
not a third reference game.

## Reproduction

From repository root:

```powershell
python test/fixtures/manual-override-v2-review/generate.py
python test/fixtures/manual-override-v2-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
& C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe test/fixtures/manual-override-v2-review/run_writer.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
python test/fixtures/rules-planner-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe --build-dir private/manual-override-v2-review/v1-regression
```

Both runners accept --binary <externally-built-executable>; no compiler is then
required. run_writer.py requires a real cryptography Python package, already
available in the documented local bundled runtime and native SDK runtime.
Define MANUAL_OVERRIDE_V2_REVIEW_PSA for native builds. Artifact target links
actual core plus PsaPackageDigest; WriterFaults additionally links actual
PsaD0FactCrypto and PsaD0FactSigner. All use actual tfpsacrypto. The writer test
imports two publicly synthetic private scalars only in its test setup; product
signer neither creates, imports, exports private bytes nor destroys those keys.

Own paths: BaseFixtures.py, generate.py, vectors.json, cli.cpp, Kernel.h, run.py,
WriterFaults.cpp, run_writer.py, README.md. Product/build wiring remains developer
owned. No commits, pushes or product edits are performed by this review.
