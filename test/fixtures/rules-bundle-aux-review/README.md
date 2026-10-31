# Focused Control/Aux and prewrite-cancel audit

Uses actual ProgressJournal, RulesBundleJournal, RulesProgressControl and the
canonical catalog codec with real BCrypt SHA-256. The empty catalog is actually
finalized in storage and decoded on a new simulated durable session. The store
model and SHA helper are reused from other audit support; expected outcomes and
this driver are independently written.

The consumer checks representation only. Its tiny authorization bytes and
eligibility fence are explicitly synthetic. No GM, event installation, setup,
clock authority, complete D1 owner, hardware or physical power-loss qualification.

Covers valid Control+exact reservation+duplicate+fresh-session recovery; absent/
wrong catalog/deployment, operation/header/actual-digest mismatch at write and
hostile persisted-record recovery; raw SHA mismatch for all four Aux kinds before
any provisional consumer callback; denied eligibility; postwrite scan IO with
durable commit recovered; cancellation before any write versus an inert written
chunk and an uncertain zero-byte attempt. No active or poisoned attempt is erased.

Run `python test/fixtures/rules-bundle-aux-review/run.py --compiler <g++>`.
For an already built binary, use `--binary <path>` instead; this path does not
select a compiler or a platform backend. CMake can supply the actual PSA hash
using `RULES_PLANNER_REVIEW_PSA` and production `PsaPackageDigest.cpp`.
Windows callers must provide the compiler runtime DLL directory in PATH,
exactly as for other prebuilt MinGW audit programs.
Build output belongs only in `private/rules-bundle-aux-review`; never p1a3-host.
The audit-only `--bundle-source` option can compile a separate regression or
mutation body for a negative demonstration. Its hash must be reported;
the default always compiles the actual current product source.
`--rawsha-kind received|claim|result` selects one isolated persisted-digest
regression; omission always executes the complete campaign.

## Observed local evidence, 2026-11-01

306 assertions pass with C++11/Wpedantic/Werror and real BCrypt against bundle
body SHA256 `032c01cb157fbc8a8302d1be7282509458310d0345e6c2e6695ae04a2553dc8a`
and header `2522cb887df7fd24048eec13a676fe8639fd3d1fc7b1e3a68a6e961f30c3df35`.
Both compile-and-run and prebuilt-binary runner modes were exercised.

Each isolated Received/EffectClaim/EffectResult bad-digest case fails when ONLY
the new non-Control recovery RawSHA guard is removed in a private mutation body
(`623c69e9d281a713767c38d8c119e77c2d68c04924b155befdf4f50a6e3475fc`),
and each passes against the actual corrected source. This mutation is not
claimed to reproduce a previous committed or frozen source hash. Production
files were not edited for the demonstration. Separate reviewer reexecution,
native PSA, actual semantic owners and device qualification remain separate.
