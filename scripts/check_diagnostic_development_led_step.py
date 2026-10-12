#!/usr/bin/env python3
"""Check the development LED-step frame without hardware."""
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
    sources = ["test/test_diagnostic_development_led_step/test_main.cpp",
               "src/CompanionIntegration/DiagnosticDevelopmentLedStep.cpp",
               "src/CompanionIntegration/DiagnosticControl.cpp",
               "src/Companion/Identity.cpp",
               "src/Companion/IdentityAccess.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-development-led-step-") as directory:
        output = Path(directory) / "led-step.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic",
                   "-Werror", "-I", str(root / "include"),
                   *[str(root / source) for source in sources], "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)
    return 0


if __name__ == "__main__":
    sys.exit(main())
