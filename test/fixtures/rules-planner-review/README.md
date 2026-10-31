# Independent P1a-5 planner review

Status: independently reviewed hardware-free scope, 31 October 2026. Sources:
P0/P1a-5 contracts, D0, and the unchanged P0 reference fixture. No runtime or
fixture code changes were made outside this directory by this reviewer.

The fixture Git-blob LF SHA-256 is
`58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605`.
The source guard replaces CRLF pairs with LF before hashing; it does not alter
other bytes, whitespace or JSON structure. This gives the same guard on Linux
and Windows without changing the original fixture. The historical Windows
CRLF checkout raw SHA-256 was `473346cfc53aea72de41a9f2be355964c8f7a4d037174a5716f7b22bccd9ac02`;
that platform-specific raw hash is provenance, not the portable guard.
`reference-traces.json` preserves all six original traces and 24 full state
projections materialized with the existing independent Python P0 model.
`reference-packages.json` expresses those same specifications as ordinary
canonical generic modules and content, not native game-name switches.

`witness-goldens.json` contains twelve certificate goldens computed by exhaustive
subset enumeration, independently of the product's antichain algorithm.
`consumer-goldens.json` records additional consumer/provenance requirements.
`proof-scenarios.json` freezes 54 executable module-chain cases. `generate.py`
is the editable source for these fixed fixtures; it uses standard Python JSON,
SHA-256 and the unchanged reference model, never product codecs or planner code.
The inherited 18 P0-model tests (including their reference-trace test) remain
separate checks; they are not relabelled as 18 newly executed runtime cases.

Windows, independent public BCrypt SHA-256:

```powershell
python test/fixtures/rules-planner-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

Native executable built with the real production PSA adapter:

```sh
python test/fixtures/rules-planner-review/run.py --binary /absolute/path/rules-planner-review-psa --budget-binary /absolute/path/rules-planner-budget-psa
```

Both C++ drivers support `RULES_PLANNER_REVIEW_PSA` (or the existing
`PACKAGE_MODULE_REVIEW_PSA` macro), the real `PsaPackageDigest` implementation,
and the qualified PSA library. `--budget-binary` is optional when the native
visit seam is registered separately. The Windows command builds both drivers
with C++11 and warnings as errors. Outputs stay in ignored
`private/rules-planner-review`, configurable with `--build-dir`.

The normal driver exercises real canonical module bytes → verified compiler →
owned immutable program with explicit D0 identities, allowed epochs and complete
physical-exclusive-resource/allowlist bindings → pure planner → alternating
separate RAM staging. Every original reference step compares its complete P0
state projection, including receipts, latches, timer generations and cumulative
committed emission/intent drafts. Own emissions are not silently looped back as
admitted facts.

The 54 additional cases exercise global witness minimization, false-proof duals,
value and timer provenance, eight dependencies per new fact versus larger
non-Fact consumers, 64 distinct references across eight new facts, irrelevant
proof branches, unknown/overflow behavior, ledger capacity and conflicting
rebinds, 32/33 explicit actions, implicit timerconsume/rearm, 8/9 emissions and
intents, named commit fences, staging alias/capacity rejection, physical aliases,
stable D0 rule ordering, FactId tuple ordering opposing name/admission order,
metadata conflicts, and valid/forged own-fact loopback.

`budget.cpp` is explicitly a **typed-consumer seam**, not a claim that canonical
JSON preserves DAG sharing. Its 512-node, depth-three shared graph has exactly
4096 guard/node visits: one true leaf, 447 middle ALL nodes, and 64 root ALL
nodes. Eight roots have eight children; the rest have seven. It must build and
apply with 64 latches. Giving a ninth root eight children raises the bound to
4097 and must fail before program publication as `OperatingProfile`.

`CompilerSupport.h` preserves the audit-owned real-crypto/provider/sink helpers
used by the earlier module review. Mock digest shortcuts are not used for any
successful compiler or planner case here. The twelve certificate goldens are
pure independent oracle calculations, not hashing or signature evidence.

This establishes host planning and revision-fenced RAM adoption only. Durable
D1 provenance/outbox recovery, authorization, deployment authentication, device
execution, actual SDK/PSA CI, resource timing and the multi-box product gates
remain distinct evidence. Host object/frame sizes exclude caller pool storage
and do not establish ESP heap or total recursive task stack.
