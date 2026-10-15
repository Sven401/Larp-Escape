"""Run NFC core/transaction checks without RF or a device."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = shutil.which(os.environ.get("CXX", "g++"))
if compiler is None:
    compiler = str(Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe")
environment = os.environ.copy()
environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="nfc-probe-tests-") as directory:
    output = Path(directory) / "nfc-tests.exe"
    subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                    "-I", str(root / "include"), str(root / "src/NfcProbe.cpp"),
                    str(root / "tests/test_core.cpp"), "-o", str(output)], check=True, env=environment)
    subprocess.run([str(output)], check=True, env=environment)
