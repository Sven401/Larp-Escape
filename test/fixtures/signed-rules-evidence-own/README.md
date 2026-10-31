# Signed rules evidence: implementation boundary checks

Root-authored tests, not independent approval. Current local run: **28 checks**,
strict GCC 5.1 C++11/Wpedantic/Werror, actual Planner.initialize/plan, real CNG
SHA-256. The minimal owned runtime program is validated directly; it is not a
new package-compiler fixture. The policy seams are deliberately synthetic and
do not qualify any real installation, input owner, GM, event or key.

Covered: complete metadata packing/reload through actual RulesProgressCodec,
unchanged plan and prior state, historical code never querying current sources,
default missing-install/input closure, wrong final range, wrong clock, bad
scope/error combination, output clearing and reuse, ledger/solved overlap and
plan dependency view outside the owned pool rejected before callbacks.

**No signing path is executed by these 28 tests.** Actual writeFact/checkFact
graph/outbox paths, independently verified signatures, dependency closure and
cycles, original policy implementations, native PSA, full journal recovery and
device behavior remain required. A manually supplied ProgressRecord in this
fixture is not a committed journal transaction.

Reproduce: `python test/fixtures/signed-rules-evidence-own/run.py --compiler
/path/to/g++ --build-dir private/signed-evidence-own`; an already qualified
prebuilt executable can be supplied with `--binary /path/to/evidence`.
