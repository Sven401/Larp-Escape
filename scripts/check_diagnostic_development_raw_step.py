#!/usr/bin/env python3
"""Check the fixed development raw-step frame without hardware."""
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
    sources = ["test/test_diagnostic_development_raw_step/test_main.cpp",
               "src/CompanionIntegration/DiagnosticDevelopmentRawStep.cpp",
               "src/CompanionIntegration/DiagnosticControl.cpp",
               "src/Companion/Identity.cpp",
               "src/Companion/IdentityAccess.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-development-raw-step-") as directory:
        output = Path(directory) / "raw-step.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic",
                   "-Werror", "-I", str(root / "include"),
                   *[str(root / source) for source in sources], "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)
    return 0


if __name__ == "__main__":
    sys.exit(main())
