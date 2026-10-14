"""Compile and exercise the actual output adapter against a controlled WLED port.

These are lifecycle tests, not a substitute for a WLED firmware build or visible
LED observations. Windows also discovers PlatformIO's installed MinGW compiler;
--compiler or CXX can select a specific compiler.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
ADAPTER = ROOT / "firmware/wled/usermods/larp_ble/src"
CASES = (
    "boot", "latest_stop", "stop_inflight", "same_restart", "burst_completion",
    "ripple_once", "ripple_replacement", "allocation_failure", "missing_frame",
    "bus_never_idle", "black_verified", "clock_wrap",
    "burst_missed_window", "replace_automatic_off", "hardware_fault",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler or "g++")
    if compiler is None and args.compiler is None and os.name == "nt":
        core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
        fallback = core / "packages/toolchain-gccmingw32/bin/g++.exe"
        if fallback.is_file():
            compiler = str(fallback)
    if compiler is None:
        raise SystemExit("C++ compiler not found; supply --compiler")
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="larp-wled-output-") as directory:
        executable = Path(directory) / ("tests.exe" if os.name == "nt" else "tests")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(TESTS / "stubs"), "-I", str(ADAPTER),
            str(TESTS / "test_output.cpp"), str(ADAPTER / "WledLedOutput.cpp"),
            "-o", str(executable),
        ], check=True, env=env)
        for case in CASES:
            subprocess.run([str(executable), case], check=True, env=env)
    print(f"{len(CASES)} WLED output lifecycle cases passed")


if __name__ == "__main__":
    main()
