#!/usr/bin/env python3
"""Check the closed diagnostic GATT route against the real identity implementation."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from check_headers import find_compiler


def main():
    root = Path(__file__).resolve().parents[1]
    compiler = find_compiler(None)
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    sources = ["test/test_diagnostic_gatt_route/test_main.cpp",
               "src/CompanionIntegration/DiagnosticGattsRoute.cpp",
               "src/Companion/IdentityAccess.cpp", "src/Companion/Identity.cpp",
               "src/Companion/AuthorityState.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-diagnostic-gatt-") as directory:
        output = Path(directory) / "route-test.exe"
        subprocess.run([str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I", str(root / "include"),
                        *[str(root / source) for source in sources], "-o", str(output)],
                       check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)


if __name__ == "__main__":
    sys.exit(main())
