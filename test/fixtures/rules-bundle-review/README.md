# Independent RulesBundleJournal review

This is an actual Bundle + unchanged ProgressJournal storage audit. It is not a
positive ManualOverrideOwner, authority, RulesProgressCodec, outbox, activation,
intent execution, real entropy or hardware power-loss qualification.

## Preserved sources and bytes

- `generate.py` is an independent minimal definite CBOR encoder. It imports no
  product codec. `vectors.json` preserves eight exact canonical Bundle-v1 sets:
  catalog/application blobs at 1, 2048, 2049 and 32768 bytes, original blob/chunk/
  final bytes, secure-ID fixture inputs and independently computed SHA-256.
- The blobs are opaque canonical CBOR storage fixtures, not fabricated positive
  semantic catalog/plan documents. Application sets really finalize a preceding
  matching catalog. A 32768-byte blob uses 16 chunks; its marker is 1041 bytes.
- `vectors.json` also preserves 239 hostile actual lowlevel-record sets, including
  all 194 shorter prefixes of a minimal valid chunk. The real lowlevel journal
  commits and recovers these physical records before the Bundle rejects their
  malformed CBOR, conflicting headers/parents, missing/duplicate/wrong references
  or false actual blob hash. They fail before semantic callbacks/publication.
- `cli.cpp`, `Faults.h` and `Store.h` are the independently owned actual driver.
  `run.py` compares every positive physical payload and header with the preserved
  independent bytes, including raw request hashes and full assigned-ID range.

## Recovery and failures

`Store.h` separates session-visible and durable NOR bytes. Program only touches
erased requested bytes. Successful sync preserves all preceding writes. A failed
sync is tested with its pending bytes both lost and persisted. Session loss
discards visible bytes; recovery creates a new backend from durable bytes and a
new actual lowlevel journal. A new wrapper around an offline failed session is
explicitly insufficient. This modeled backend contract is not physical flash
qualification or a claim that the Bundle can detect a dishonest backend reopen.

The campaign checks 16390 cutpoints: each programmed prefix and the full 8194
bytes of an application chunk+Final transaction, under both write-through and
buffered persistence. Only a full unique Final publishes the application and
allocates all eight IDs. Orphan chunks remain inert and cannot revive an old
command. Every occupied torn tail blocks recovery. A partly programmed version
can correctly return Unsupported rather than Corrupt; both must be hard failures.
The lowlevel initialization/header campaign remains its separate existing test.

Additional actual gates cover 14 postwrite-read/sync branches, seven input-copy/
reentry/end-snapshot callbacks, nine hash/random rejection+healthy-reuse cases,
physical slot 63/+1, reserved intent slots 63/+1, caller-pool shrink, overlapping
byte pools, 32768/+1 blob limit, eight/+1 IDs, staged-byte mutation, and fence
revocation before chunk or Final. Unknown remains locked through actual recovery;
postwrite failure must not be presented as definite rejection. Exact Final replay
retains original assigned IDs; changed issueCount/HeaderMeta conflicts. Recovery
consumer callbacks are provisional: abort preserves an existing published value.
The explicit auxiliary test consumer is storage-only, not a production typed
Received/effect reducer. Whole owner intent-budget serialization remains its owner.

## Reproduce

Windows, from the repository root:

```powershell
python -B test/fixtures/rules-bundle-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

The runner uses a separate `private/rules-bundle-review` directory, C++11,
-Wall/-Wextra/-Wpedantic/-Werror and -O2. Hashes use real BCrypt on Windows.
The long host fault campaign is deliberately optimized; it is not an ESP timing
or stack bound. Native mode uses actual PsaPackageDigest/tfpsacrypto:

```text
python -B test/fixtures/rules-bundle-review/run.py --binary /path/to/rules-bundle-review-psa
```

Build `cli.cpp`, `ProgressJournal.cpp`, `RulesBundleJournal.cpp` and
`PsaPackageDigest.cpp` with RULES_BUNDLE_REVIEW_PSA for that target; link the actual
PSA library. A 300-second timeout is suitable for the bounded host campaign.
Regenerate preserved vectors with `python -B test/fixtures/rules-bundle-review/generate.py`.
The production identity source is not qualified by deterministic synthetic test IDs.

Fresh Control/Aux body regression: unchanged eight fixed vectors plus 239
hostile real record sets passed again (167269 assertions / 16390 cuts). Runner
now links actual RulesProgressControl.cpp and its RulesPlanner/PackageModules/
PackageDeclarations/CanonicalPackageJson dependencies, not stubs. Reviewed Bundle
body1777ec938e0c0bc5111db28e4bdcc9bf87df52c6b16de0a8199f43b638541089.
This preserves the old corpus; it does not claim new Control/Aux positive or
actual Owner authority evidence. Those remain separate pending integration.
