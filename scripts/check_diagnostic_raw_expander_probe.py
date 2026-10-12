#!/usr/bin/env python3
"""Compile the exploratory register probe against a fake I2C reader."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_headers import find_compiler


def main():
    root = Path(__file__).resolve().parents[1]
    compiler = find_compiler(None)
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="larp-raw-expander-") as directory:
        output = Path(directory) / "raw-expander.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic",
                   "-Werror", "-I", str(root / "include"),
                   str(root / "test/test_diagnostic_raw_expander_probe/test_main.cpp"),
                   str(root / "src/CompanionIntegration/DiagnosticRawExpanderProbe.cpp"),
                   "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)
    return 0


if __name__ == "__main__":
    sys.exit(main())
