"""Actual Bundle+ProgressJournal driver; fixed independent raw-CBOR comparisons."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "private/rules-bundle-review")
    args = parser.parse_args()
    env = os.environ.copy()
    binary = args.binary
    if binary is None:
        if args.compiler is None:
            parser.error("--compiler is required unless --binary is provided")
        args.build_dir.mkdir(parents=True, exist_ok=True)
        binary = args.build_dir / "review.exe"
        env["PATH"] = str(args.compiler.parent) + os.pathsep + env.get("PATH", "")
        subprocess.run([str(args.compiler), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I" + str(ROOT / "include"), str(HERE / "cli.cpp"),
                        str(ROOT / "src/DeviceRuntime/ProgressJournal.cpp"),
                        str(ROOT / "src/DeviceRuntime/RulesBundleJournal.cpp"),
                        str(ROOT / "src/DeviceRuntime/RulesProgressControl.cpp"),
                        str(ROOT / "src/GameExecution/RulesPlanner.cpp"),
                        str(ROOT / "src/GameConfiguration/PackageModules.cpp"),
                        str(ROOT / "src/GameConfiguration/PackageDeclarations.cpp"),
                        str(ROOT / "src/GameConfiguration/CanonicalPackageJson.cpp"),
                        "-lbcrypt", "-o", str(binary)], check=True, env=env)
    fixed = json.loads((HERE / "vectors.json").read_text(encoding="utf-8"))
    vectors = fixed["vectors"]
    malformed = fixed["malformed"]
    rows = {x["name"]: x for x in vectors}
    data = "".join(f"{x['name']} {x['kind']} {x['blob']}\n" for x in vectors)
    for x in malformed:
        data += f"{x['name']} 0 {x['error']} {len(x['records'])}\n"
        for r in x['records']:
            data += f"{r['operation']} {r['request']} {r['kind']} {r['issues']} {r['profile']} {r['trust']} {r['payload'] or '-'}\n"
    result = subprocess.run([str(binary)], input=data, capture_output=True, text=True, env=env, timeout=300)
    if result.returncode:
        raise AssertionError(f"driver exited {result.returncode}: {result.stderr}\n{result.stdout[-500:]}")
    seen = {name: [] for name in rows}
    checks = None
    bad_seen = []
    for line in result.stdout.splitlines():
        if line.startswith(("CUTS ", "POSTWRITE ", "CAPACITY ", "CALLBACKS ", "HASH_RANDOM ", "SLOTS_FENCE_LIMITS ")):
            print(line)
            continue
        if line.startswith("BADPASS "):
            bad_seen.append(line.split()[1])
            continue
        if line.startswith("CHECKS "):
            checks = int(line.split()[1])
            continue
        fields = line.split()
        assert len(fields) == 10 and fields[0] == "RECORD", line[:150]
        _, name, index, operation, request, kind, issues, profile, trust, payload = fields[:10]
        # Strict field count below intentionally guards the driver's stable protocol.
        seen[name].append((int(index), operation, request, int(kind), int(issues), int(profile), int(trust), payload))
    for name, row in rows.items():
        expected = row["chunks"]
        actual = seen[name]
        assert len(actual) == len(expected) + 1, name
        for got, want in zip(actual, expected):
            index, operation, request, kind, issues, profile, trust, payload = got
            assert (index, operation, request, payload) == (want["index"], want["operation"], want["request"], want["payload"]), name
            assert (kind, issues, profile, trust) == (5, 0, 17, 23), name
            assert hashlib.sha256(bytes.fromhex(payload)).hexdigest() == request, name
        index, operation, request, kind, issues, profile, trust, payload = actual[-1]
        assert index == expected[-1]["index"] + 1, name
        assert operation == bytes([0x40 + row["kind"]] * 16).hex(), name
        assert request == row["final_sha256"] and payload == row["final"], name
        assert (kind, issues, profile, trust) == ((5, 0, 17, 23) if row["kind"] == 1 else (2, 8, 17, 23)), name
    assert bad_seen == [x["name"] for x in malformed]
    assert checks is not None
    print(f"{len(vectors)} exact raw Bundle vectors + {len(malformed)} hostile real-record sets; {checks} actual Journal assertions; 0 mismatches")


if __name__ == "__main__":
    main()
