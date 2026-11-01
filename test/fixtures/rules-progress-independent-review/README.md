# Independent Rules progress integration review

Scope: actual source compiler, full artifact validator, owned Program, pure
planner, genuine D0 writer/verifier and canonical storage codec. No positive
OverrideAdmission, live authority, peer/Event permission, ManualOverrideOwner,
D1 commit, loopback, flash or device activation is fabricated or established.
The decoder always yields DecodedUnauthenticated and an unexecutable Plan.

## Actual inputs, independent bytes and signatures

`cli.cpp` preserves the input adapter from the checkpointed independent manual
artifact audit. It consumes all 61 unchanged complete raw artifact sets from
`../manual-override-v2-review/vectors.json`. Their 20 accepted Programs pass the
real catalog encode/decode path. `Catalog.h` exports every typed field in the
versioned wire contract, including Locations, all eight child slots, complete
string/epoch pools and actual derived groups. `Oracle.py` is an independently
written definite canonical CBOR encoder/decoder from
`docs/spielpakete/rules-progress-codec-v1.md`; it calls no product codec.
`catalog-goldens.json` preserves the observed compiler field models and the
independently encoded exact bytes, not an invented independent compiler.
There are 80 persisted-group tamper cases, including v2 group0/foreign group and
v1 group!=0. The original caller CBOR, source strings/nodes/epochs are destroyed;
reconstructed values, epoch views and exact re-encoding remain unchanged.

`trace.cpp` preserves the six unchanged P0 trace inputs and 24 complete decoded
state goldens from `../rules-planner-review`. The original fixture hash is guarded
as Gitblob LF SHA256 58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605;
normalization is CRLF-to-LF only. No third game is introduced. The synthetic trace
binding explicitly has generation1, required for genuine D0 generation, instead
of the former typed-only convenience default0. All old state goldens still match.

The trace adapter creates real COSE using actual D0FactWriter, actual decoder/
verifier and the real CNG or production PSA signer. Scalar1 is an explicitly
public synthetic test key; native setup imports and later destroys this test-owned
key. The product signer continues to borrow its configured key. Inputs reuse
frozen original bytes by Fact-ID; outgoing frames use the actual planner's full
candidate IDs and dependency witness. There are 20 actual Ready plans, 12 signed
proof/outbox occurrences independently checked with Python P-256, exact canonical
Key77 kid, protected header, original Sig_structure bytes, externalAAD13,
payload SHA256, full context, subject, sequence, type and actual witness. Twelve
occurrences are not asserted to be twelve distinct physical signatures.
Signatures do not prove original installation/origin/GM permissions.

`trace-catalog-goldens.json` preserves all six actual catalog byte oracles;
`application-goldens.json` preserves 20 original signed application snapshots.
Fresh signatures are randomized and independently verified; preserved snapshots
are separately verified and decoded without signing again. Public XY, COSE,
authorization and every evidence view are copied into caller-owned decode pools.
No decoded historical record creates private live admission. Encoder attempts
with an otherwise coherent public raw OverrideRecord fail Error::Admission.

## Boundaries and measured byte sizes

The artifact phase passes 660 C++ checks; the trace phase passes 602 host-CNG
checks. Native setup/key-cleanup adds its actual PSA-specific checks. Exact pool/
output capacities, -1 failures, alias-before-preflight unchanged old flags/counts,
post-preflight failure clearing, healthy reuse, nonowned typed evidence views
and source destruction are exercised using the actual compiled catalogs/plans.
Pool bytes need not be zeroed on ordinary failure; flags/counts are the gate.

Twenty artifact catalogs: 816..2570 bytes. Six unchanged trace catalogs:
A-source1090, A-lens1090, A-gate-left872, A-gate-right872,
B-echo-deadline1704, B-archive-alternative1232 bytes. Their 20 actual application
blobs are 1582..4813 bytes. These are host observations, not an ESP pool/stack/time
profile. The 4813-byte actual plan demonstrates why one3072-byte physical journal
payload cannot silently substitute for the complete Bundle transaction.

`generate_malformed.py` preserves complete hostile bytes in `malformed.json`.
The campaign runs 843 catalog decodes and 4855 application decodes, including
all 816 shorter prefixes of a real populated catalog and all 4813 shorter
prefixes of a genuine proof/outbox application. Five cases intentionally pass
only as unauthenticated representation: a bad signature, bad public point, raw
historical OverrideRecord, forward-only planning clock, and exact32768-byte outer
representation using the explicitly opaque evidence field. Their Plans stay
unexecutable and admissions stay empty. Both mathematical defects separately
fail actual Python verification. The large opaque evidence is not valid Owner
authorization. Reverse clock, any changed logical fence/lastIssued, substituted
program fingerprint,32769 bytes, malformed typedproofs/ranges/widths/containers,
missing/duplicate proofs and outgoing ID prediction errors reject without a
partial decoded result. A real ClockWitness remains the owner's obligation.

## Reproduce

From the repository root, using the existing compiler and bundled Python with
cryptography (the normal system Python may lack that independent verifier):

```powershell
python -B test/fixtures/rules-progress-independent-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
& 'C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B test/fixtures/rules-progress-independent-review/run_traces.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
$env:PATH='C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin;'+$env:PATH
& 'C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -B test/fixtures/rules-progress-independent-review/run_malformed.py --catalog-binary private/rules-progress-independent-review/catalog.exe --trace-binary private/rules-progress-independent-review/trace.exe
```

Both positive runners accept `--binary` instead of compilation. Native targets
use RULES_PROGRESS_REVIEW_PSA and the actual PsaPackageDigest; trace additionally
links actual PsaD0FactCrypto/PsaD0FactSigner and tfpsacrypto. The three runners
share the same fixed sources and exact byte oracles on Linux. Native actual PSA
qualification is a separate mandatory gate; Windows CNG is not its substitute.
Existing CI already provides cryptography45.0.7; local bundled verifier50.0.2.
The default separate build directory is `private/rules-progress-independent-review`.

Only explicit `--update-goldens` recreates preserved observations; do not use it
to hide a regression. `generate_malformed.py` deterministically regenerates hostile
bytes from those preserved actual cases. All editable adapters/oracles, original
raw inputs and production wire contract remain versioned and traceable.
