# Independent SerializedOrdering audit

Status: actual independent component run passed, 830 assertions, 2026-11-01.
This uses the concrete public SerializedOrdering API and real shared Context/
Fence equality. Copied metadata is explicitly synthetic, not actor rights or
Current GM permission. Supported exclusive single-worker profile only; no SDK,
SMP/RTOS/mutex/flash/semantic replay or overall Owner qualification.

Actual checks cover all25 binding fields,27 lease fields,30 recovery pin
fields, full uint64 values, same-operation fresh generations, stale releases,
Unknown/IO/Corrupt hold retention, positive Empty distinct from missing Boot or
unavailable metadata, before/after immutable pins, fallible prepare and callback-
free no-fail publication, abort remaining recovery-required, all supported
kinds/parent shapes and complete callback reentry/input copying.

No private state access, reinterpretation or test friend. Generation overflow
is unreachable through a practical public call count and remains a source check.

Reproduce: python -B test/fixtures/rules-commit-ordering-review/run.py --compiler <g++>
Own build: private/rules-commit-ordering-review-quality. --binary skips compilation.
Actual source links include RulesCommitOrdering and real Planner/compiler helper
implementations, no mocked shared equality. Product files/wiring are untouched.

Verified component freeze: header
1ba658f6c797a28c952796d4e35ca831c5adfe0990628dde4deadfe47451c5a7
and body9d6861b8b10e33a03126fcc98e22b73374d90f1a2db49826b963f7caf8a6ec78.
Fresh strict C++11/O2/Wall/Wextra/Wpedantic/Werror build:830 assertions pass.
Same public corpus also passes prebuilt-binary mode; no tests depend on backend
cryptography or IO. Source inspection confirms generation overflow guards before
hold/begin/prepare; prepare reserves the single no-fail publish increment.
publishRecovery has no source/IO/queue callback. None of this supplies a hardware
serialization bridge or proves the unfinished overall Owner integration.

The original450-assertion campaign remains included. Additional actual public
Claim->Result transitions keep the gate continuously Held, copy new operation/
commitment with higher generation, reject any shifted original parent/request/
catalog/Context/Fence, and leave stale Claim leases unable to resolve Result.
Actual Result WriteUncertain is pinned as Held Result inventory, never Empty or
Claim. Both committed and proven-denied Result recovery prepare/publish paths
are checked with source callbacks deliberately forbidden. Input/output aliasing
of the public lease object is exercised without accessing private state.
