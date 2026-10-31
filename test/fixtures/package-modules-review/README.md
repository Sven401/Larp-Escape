# Independent P1a-4 module compiler review

Status: independent, hardware-free review, 31 October 2026. Sources are P0
§3 and its three reference modules, plus the explicit P1a-4 maintainer
clarifications. These artifacts were authored separately from product code.

`vectors.json` preserves 89 fixed canonical input cases, source pins/bytes,
expected acceptance, selected exact errors, and full expected canonical station
output for every accepted case. `generate.py` is their editable independent
source; it uses Python's standard JSON and SHA-256 implementations, never a
product encoder or compiler. Regeneration must preserve the reviewed fixture
bytes unless expectations are deliberately reviewed again.

Windows, using an independent real public BCrypt SHA-256 backend:

```powershell
python test/fixtures/package-modules-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

For a separately built native executable using the production PSA adapter:

```sh
python test/fixtures/package-modules-review/run.py --binary /absolute/path/package-modules-review-psa
```

Build `cli.cpp` with `PACKAGE_MODULE_REVIEW_PSA`, the `PsaPackageDigest` include
path and implementation, and the real qualified PSA library. This selects the
production backend; the fixture bytes and assertions remain identical. The
Windows command compiles with C++11 and warnings as errors. Build outputs stay
under ignored `private/package-modules-review` unless `--build-dir` is supplied.

The runner checks:

- 89 fixed cases, including `threshold`, `ordered_window`, `all_solved`, renamed
  equivalence, nested solved composition, exact parameter/maps/types/resources,
  eager constraints, unused-definition validation, pins, changed raw bytes,
  escaped 485-byte names and direct/nested naming collisions.
- Exact/+1 P0 expansion limits: definitions 16/17, occurrences 32/33, depth 4/5,
  values 32/33, timers 16/17, rules 64/65, nodes 512/513, actions 8/9 and expression
  depth 8/9. Separate operating cases cover parameters 64/65, interfaces 128/129,
  this driver's expanded trigger pool 4096/4097, interned scalars 1024/1025 and
  the inherited metadata count 32768/32769. These profiles are not new P0 limits.
- 2843 source failures at every content/module byte boundary, including EOF;
  2842 truncated-input/late-source-priority cases; 30 hash setup/update/finish,
  wrong-size/abort, source-priority and healthy-reuse cases; ten sink
  begin/station/write/end/commit failure, abort and healthy-reuse cases.
- Correct deferred source/path attribution, mandatory provider close, actual
  raw-content digest on acceptance, zero digest on rejection, and no rejected
  transaction committed. Compiler/workspaces are reused between cases.

The two cycle cases use an explicitly **negative-only** constant-digest
seam to exercise unused self/mutual DAG-cycle rejection. Real circular
content-addressed definitions would require digest fixed points. Both cases must
return `Cycle`; they provide no accepted hashing evidence. Every successful
case uses real BCrypt or the real production PSA adapter, according to build.

This review establishes bounded module expansion, not author authentication,
full planner/confluence, deployment/activation validity or device qualification.
Native PSA/SDK evidence must be reported separately at its actual tested commit.
Host object/frame measurements do not establish device heap or total task stack.
