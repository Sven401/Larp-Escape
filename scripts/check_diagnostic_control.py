#!/usr/bin/env python3
"""Compile and run the actual C++11 diagnostic owner with fake component ports."""
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
    sources = ["test/test_diagnostic_control/test_main.cpp",
               "src/CompanionIntegration/DiagnosticControl.cpp", "src/Companion/Identity.cpp",
               "src/Companion/IdentityAccess.cpp", "src/Companion/AuthorityState.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-diagnostic-control-") as directory:
        output = Path(directory) / "diagnostic-test.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-I", str(root / "include"), *[str(root / item) for item in sources], "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)
    return 0

if __name__ == "__main__":
    sys.exit(main())
