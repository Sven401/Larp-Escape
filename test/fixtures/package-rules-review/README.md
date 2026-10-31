# Independent P1a-3 review vectors

Status: independent bounded-reader review, 2026-10-31. Source: P0 v1 contract
and issue #93 acceptance criteria; prepared separately from the implementation.
`vectors.json` preserves the exact canonical input bytes as JSON strings with
case name and expected status: A = DirectRulesOnly, R = rejected, U = unsupported, P = operating-profile limit, C = static conflict.
The fixtures do not claim full package validity, a planner, or device behavior.

Run from repository root with a C++11 GCC/Clang compiler:

```powershell
python test/fixtures/package-rules-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

The runner compiles independent drivers against all three actual reader sources,
then validates 187 fixed semantic vectors with one reused reader. It also checks
source failure at every byte position, including immediately after root end, and
all shorter prefixes of five selected valid canonical documents (including
unsupported content and a semantic conflict). Outputs stay under ignored
`private/package-rules-review` by default; `--build-dir` changes that destination.

Cases cover exact schemas and types, all expression families, int32 boundaries,
8/9 expression depth and actions, 64/65 rules, 512/513 nodes, two stations with
512 nodes each, forward references, input context/direction and resource kind,
64/65 ports and trigger entries, 128/129 subjects, 4096/4097 subject references,
fact monotonicity/output exclusivity, conservative local conflict and emit
profiles, unsupported modules/instances/fallback, and reuse after rejection.
Repeated cancel actions are rejected as a guaranteed within-rule conflict; the
separate eight-distinct-timers case tests the exact action budget. Shared effect
ports on disjoint local triggers are accepted. Empty, reordered and duplicate
trigger lists are not assigned an invented P0 ordering restriction.

Byte grammar regressions, complete planner conflict analysis, full module expansion, and firmware/device qualification
remain separate checks. Compiler/layout measurements here are host evidence.

The runner also constructs three targeted compact-context regressions at
station 7 / rule 63 / action 7, checking deferred guard, trigger and action
paths. These follow-up tests are implementation-author regressions, separate
from the original 187 independently authored fixed vectors.
