# Independent ManualOverrideOwner automaton audit

This tests the actual owner state machine with the real copied Program/State
representations. Authority and transaction responses, identity values and clock
are explicitly synthetic bounded ports. This is not GM/Event admission, real
entropy, signing, qualified D1 status/recovery, an Authority adapter, publication,
Control transport or flash evidence. The transaction port never writes storage.

`catalog.json` preserves original actual canonical bytes produced by the prior
full raw Artifact/compiler audit, with named source fixture and SHA256 provenance.
Its SHA256 is independently guarded in Python and recomputed by real BCrypt
(or production PsaPackageDigest on native). The actual codec reconstructs the
complete owned Program with the shared validator. State snapshots are created
by actual Planner.initialize/control; a shared-release case uses an actual
Owner-produced admission, pure planner and RAMcommit to create/copy the latch.
No program.ready projection, private friend, reinterpretation of admission or
mock hash is used as positive proof.

18 named scenario groups / 806 host assertions currently pass:

- Effect-free Prepare; copied original request, complete program SHA, immutable
  absolute deadline and unchanged replay. First accepted Confirm phase, operation,
  digest and timestamp remain fixed through pending retries and terminal replies.
- Unknown from submit, status, wrong status operation or failed later disclosure
  fences blocks all new view/prepare/submit/service work and allocates no IDs.
  Only the synthetic same-operation status result resolves that RAM barrier.
  No durable resolution is inferred by this fixture.
- Known terminal replies are immutable even if the bounded backend would report
  contradictory NotCommitted. Fresh status-reader authority remains mandatory.
  An authorized new reader never retargets the old original grant.
- Actual catalog priority change produces a different full SHA under identical
  Context and rejects the prepared grant. All logical fences/lastIssued remain
  exact; activeTime alone may grow without changing original metadata or lease.
- TTL end is inclusive; rollback/unavailable clock, final expiry and uint64
  deadline overflow fail closed. A phase already expired before first acceptance
  reserves neither confirmId nor operation: another never accepted phase receives
  the same authorized terminal Expired result. This is distinct from a changed
  first valid Confirm phase, which conflicts, including AlreadyReleased.
- Every one of 44 complete OverrideRecord field mutations fails the actual scoped
  Final callback. A second legitimate pending record cannot substitute for the
  current submit. Retaining the callback past submit does not retain permission.
- Callback reentry through real-hash, clock, authority, identities, submit and
  status ports returns Busy for all five owner APIs without mutating request,
  view, replay pools or submission/identity counters.
- Active and replay capacities have no eviction. Expired preparation retires its
  active slot but preserves replay identity. Overlapping workspace rejects before
  callbacks; a new ownerBoot rejects prior tokens/views after modeled RAM loss.
- Hash failures/length/cleanup and identity failures/zero are rejection-only
  seams over the real hash. Healthy reuse is checked; no unbounded ID retry.

Scope misuse is tested using actual producers: the real owner creates Live only
under these explicitly synthetic authority/transaction assumptions. The actual
pure replayHistorical API creates Historical from that original record and
returns a non-ready plan in a separate inactive bank. Feeding that Historical
input into the normal live planner fails before a matching historical ledger;
changing the input kind cannot smuggle it through. This is a misuse boundary
case, not proof that historical COSE, original rights or D1 were checked.

Two actual findings were reported and corrected by the developer: status Unknown
formerly left a Confirming row resubmittable; contradictory same-operation status
formerly overwrote a known Committed row. Both have independent regressions here.

## Reproduce

```powershell
python -B test/fixtures/manual-owner-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

The runner builds strictly C++11/-Wall/-Wextra/-Wpedantic/-Werror/-O2 in its own
`private/manual-owner-review` directory. `--case status-unknown` (or another named
group) isolates a regression. `--binary /path/to/executable` skips compilation;
Windows needs the existing GCC runtime directory on PATH. Native mode defines
MANUAL_OWNER_REVIEW_PSA and links the actual PsaPackageDigest/tfpsacrypto plus
actual ManualOverrideOwner, RulesPlanner, RulesProgressCodec and compiler IR
helpers. This remains synthetic authority/transaction evidence on both hosts.
The preceding real codec/COSE native PSA gate is a separate mandatory proof.

Owned sources: cli.cpp, Memory.h, Cases.h, run.py, catalog.json and this README.
They preserve the actual input and reproducible faults. No product files, build
wiring, commits or pushes are made by this reviewer.

ReleaseDisposition delta: actual false-guard pure plan has no latch/T2. Synthetic
Committed/Duplicate receipts preserve all three release dispositions through
Confirm/Status/Service, including revoked readers. A modeled later original
latch cannot relabel an old NoRelease receipt; new AlreadyReleased refers only
to the existing group. Result equality includes release. There is currently no
actual historical ManualResult producer: inactive replay deltas do not establish
a historical disposition, GM permission or durable transaction proof.

Cross-platform provenance correction after actual Native CTest failure:
sourceCatalogGoldensSHA256 now guards the original Gitblob LF bytes
db50c0c16f07ccb2e153ae735f3ef961106a4b930255fd52fcb4fe82faeea72a.
Only CRLF-to-LF conversion is permitted before source-file SHA256; no JSON or
other whitespace normalization. The former Windows raw hash55cfa774... is kept
as optional provenance metadata, not an alternative accepted source. Catalog
canonical bytes/SHA, decoded expected Program, artifact vectors and all scenario
expectations are unchanged. Native at ac81f84e failed BEFORE this audit binary
ran; successful real-PSA tests elsewhere do not substitute for its next gate.
