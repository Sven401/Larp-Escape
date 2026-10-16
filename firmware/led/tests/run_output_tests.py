"""Exercise the actual independent LED engine against an asynchronous fake bus."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler or "g++")
    if compiler is None and args.compiler is None and os.name == "nt":
        core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
        candidate = core / "packages/toolchain-gccmingw32/bin/g++.exe"
        if candidate.is_file():
            compiler = str(candidate)
    if compiler is None:
        raise SystemExit("C++ compiler not found; use --compiler or CXX")
    environment = os.environ.copy()
    environment["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="larp-led-tests-") as temporary:
        executable = Path(temporary) / ("output.exe" if os.name == "nt" else "output")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-I", str(ROOT / "include"), str(ROOT / "src/LedOutput.cpp"),
            str(ROOT / "tests/test_output.cpp"), "-o", str(executable),
        ], check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
        sdk_executable = Path(temporary) / ("rmt.exe" if os.name == "nt" else "rmt")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-I", str(ROOT / "tests/stubs"), "-I", str(ROOT / "include"),
            str(ROOT / "src/LedOutput.cpp"), str(ROOT / "src/RmtLedBus.cpp"),
            str(ROOT / "tests/test_rmt_bus.cpp"), "-o", str(sdk_executable),
        ], check=True, env=environment)
        for case in ("wire_and_completion", "bad_counter", "write_failure", "wait_failure"):
            subprocess.run([str(sdk_executable), case], check=True, env=environment)
        # The historical row wiring includes IO7, which requires an explicit
        # legacy-input opt-in; a passing host test cannot establish chip support.
        for legacy in (None, "0"):
            rejected = subprocess.run([
                compiler, "-std=c++17", "-fsyntax-only", "-x", "c++",
                "-I", str(ROOT / "include"), "-DLARP_MATRIX_TEST=1",
                *([] if legacy is None else [f"-DLARP_MATRIX_LEGACY_IO7_INPUT={legacy}"]), "-",
            ], input='#include "MatrixInputs.h"\n', text=True, capture_output=True, env=environment)
            assert rejected.returncode != 0, "matrix build accepted missing/disabled legacy IO7 opt-in"
            assert "Historical matrix test requires LARP_MATRIX_LEGACY_IO7_INPUT=1" in rejected.stderr
        for name, sources in (
            ("wake", ("src/WakeInputs.cpp", "tests/test_wake_inputs.cpp")),
            ("mcp-wake", ("src/McpWakeBus.cpp", "tests/test_mcp_wake_bus.cpp")),
            ("matrix", ("src/MatrixInputs.cpp", "src/WakeInputs.cpp", "tests/test_matrix_inputs.cpp")),
        ):
            wake_executable = Path(temporary) / (name + (".exe" if os.name == "nt" else ""))
            subprocess.run([
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                "-I", str(ROOT / "tests/stubs"), "-I", str(ROOT / "include"),
                *(["-DLARP_MATRIX_TEST=1", "-DLARP_MATRIX_LEGACY_IO7_INPUT=1"] if name == "matrix" else []),
                *(str(ROOT / source) for source in sources), "-o", str(wake_executable),
            ], check=True, env=environment)
            subprocess.run([str(wake_executable)], check=True, env=environment)
    print("16 LED engine cases, 4 SDK cases and Wake/Matrix reader/I2C cases passed")


if __name__ == "__main__":
    main()
