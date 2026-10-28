"""Compile the real channel with a host mutex, then check context and thread ordering.

The optional baseline ref loads only the channel source/header into a temporary
directory, without resetting the checkout. A context failure is returned as a
failure; it is never converted into a successful compliance check.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def compiler_path(value):
    if value:
        return Path(value).resolve()
    found = shutil.which("g++")
    if found:
        return Path(found)
    candidate = Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe"
    if candidate.is_file():
        return candidate
    raise ValueError("g++ unavailable; pass --compiler (C++11 host threads required)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--baseline-ref")
    parser.add_argument("--result", type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[1]
    compiler = compiler_path(args.compiler)
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    result = {"observed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "channel_ref": args.baseline_ref or "working tree", "compiler": str(compiler),
              "limits": "Host std::mutex/std::thread; no ESP32 SMP, ISR, latency or device proof"}
    with tempfile.TemporaryDirectory(prefix="larp-channel-check-") as temporary:
        work = Path(temporary)
        channel = root / "lib/FreeRtosGameControlChannel"
        if args.baseline_ref:
            channel = work / "baseline"
            channel.mkdir()
            for name in ("FreeRtosGameControlChannel.cpp", "FreeRtosGameControlChannel.h"):
                source = subprocess.run(
                    ["git", "-C", str(root), "show",
                     f"{args.baseline_ref}:lib/FreeRtosGameControlChannel/{name}"],
                    check=True, capture_output=True).stdout
                (channel / name).write_bytes(source)
        result["channel_sha256"] = {
            name: hashlib.sha256((channel / name).read_bytes()).hexdigest()
            for name in ("FreeRtosGameControlChannel.cpp", "FreeRtosGameControlChannel.h")}
        program = work / "cases.cpp"
        program.write_text(f'#include "{(here / "ChannelThreadCases.h").as_posix()}"\n',
                           encoding="utf-8")
        executable = work / ("channel.exe" if os.name == "nt" else "channel")
        command = [str(compiler), "-std=c++11", "-O2", "-pthread", "-Wall", "-Wextra",
                   "-Wpedantic", "-Werror", "-I", str(here / "threaded_stubs"),
                   "-I", str(channel), "-I", str(root / "include"), str(program),
                   str(channel / "FreeRtosGameControlChannel.cpp"),
                   str(root / "src/Companion/AuthorityState.cpp"),
                   str(root / "src/Companion/GameCommandReader.cpp"), "-o", str(executable)]
        compiled = subprocess.run(command, capture_output=True, text=True, env=environment,
                                  timeout=60)
        result.update(compile_exit=compiled.returncode,
                      compile_output=compiled.stdout + compiled.stderr)
        code = compiled.returncode
        if code == 0:
            try:
                run = subprocess.run([str(executable)], capture_output=True, text=True,
                                     env=environment, timeout=30)
                code = run.returncode
                result.update(run_exit=code, run_output=run.stdout + run.stderr)
            except subprocess.TimeoutExpired:
                code = 124
                result.update(run_exit=code, run_output="Timeout: thread completion not observed")
        if args.result:
            args.result.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return code


if __name__ == "__main__":
    raise SystemExit(main())
