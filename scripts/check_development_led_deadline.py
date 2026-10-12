#!/usr/bin/env python3
"""Check repeatable, independently triggered LED-off state without hardware."""
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
    with tempfile.TemporaryDirectory(prefix="larp-development-led-deadline-") as directory:
        output = Path(directory) / "led-deadline.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic",
                   "-Werror", "-I", str(root / "include"),
                   str(root / "test/test_development_led_deadline/test_main.cpp"),
                   str(root / "src/CompanionIntegration/DevelopmentLedDeadlineCore.cpp"),
                   "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        subprocess.run([str(output)], check=True, env=environment, timeout=30)
    return 0


if __name__ == "__main__":
    sys.exit(main())
