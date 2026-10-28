"""Observe queue calls under the actual channel's authority lock, hardware-free.

Only temporary copies of the existing deterministic RTOS stubs are instrumented.
Product sources are read directly. This is not an SMP/deadlock/latency test.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    args = parser.parse_args()
    root = args.repo.resolve()
    here = Path(__file__).resolve().parent
    relative = [
        "lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp",
        "lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.h",
        "src/Companion/AuthorityState.cpp",
        "src/Companion/GameCommandReader.cpp",
        "test/test_configured_control/host_stubs/freertos/FreeRTOS.h",
        "test/test_configured_control/host_stubs/freertos/queue.h",
    ]
    hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in relative}
    stub = root / "test/test_configured_control/host_stubs/freertos"
    queue = (stub / "queue.h").read_text(encoding="utf-8")
    declaration = ("namespace host_rtos { extern unsigned lockedSend, lockedReceive, lockedPeek; "
                   "extern unsigned totalSend, totalReceive, totalPeek; }\n")
    queue = queue.replace('#include <cstring>\n', '#include <cstring>\n' + declaration, 1)
    for function, counter in (("xQueueSendToBack", "Send"), ("xQueueReceive", "Receive"),
                              ("xQueuePeek", "Peek")):
        start = queue.index("inline int " + function + "(")
        brace = queue.index("{", start)
        insertion = (f"\n    ++host_rtos::total{counter};\n"
                     f"    if (host_rtos::depth) ++host_rtos::locked{counter};\n")
        queue = queue[:brace + 1] + insertion + queue[brace + 1:]
    environment = os.environ.copy()
    environment["PATH"] = str(args.compiler.resolve().parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="larp-rtos-audit-") as temporary:
        work = Path(temporary)
        (work / "freertos").mkdir()
        (work / "freertos/FreeRTOS.h").write_bytes((stub / "FreeRTOS.h").read_bytes())
        (work / "freertos/queue.h").write_text(queue, encoding="utf-8")
        executable = work / "probe.exe"
        command = [str(args.compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-I", str(work), "-I", str(root / "include"),
                   "-I", str(root / "lib/FreeRtosGameControlChannel"),
                   str(here / "probe.cpp"), str(root / relative[0]),
                   str(root / relative[2]), str(root / relative[3]), "-o", str(executable)]
        compiled = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=60)
        result = {"observed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  "repository": str(root), "source_sha256": hashes,
                  "compiler": str(args.compiler), "compile_exit": compiled.returncode,
                  "compile_output": compiled.stdout + compiled.stderr,
                  "limits": "Deterministic single-thread RTOS model; no SMP, deadlock or timing proof"}
        if compiled.returncode == 0:
            run = subprocess.run([str(executable)], capture_output=True, text=True, env=environment, timeout=15)
            result.update(run_exit=run.returncode, run_output=run.stdout + run.stderr)
            if run.returncode == 0:
                result["observations"] = json.loads(run.stdout)
        args.result.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return result.get("run_exit", compiled.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
