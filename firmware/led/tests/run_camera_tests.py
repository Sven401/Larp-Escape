"""Exercise the actual USB calibration loop with an asynchronous fake LED bus."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = shutil.which(os.environ.get("CXX", "g++"))
if compiler is None and os.name == "nt":
    compiler = str(Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe")
environment = os.environ.copy()
environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="camera-loop-") as temp:
    exe = Path(temp) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-DLARP_LED_CAMERA_LAB=1",
                    "-I", str(root / "tests/camera_stubs"), "-I", str(root / "include"),
                    str(root / "tests/test_camera_lab.cpp"), str(root / "src/LedOutput.cpp"),
                    "-o", str(exe)], env=environment, check=True)
    for case in ("sequence", "watchdog", "stop-inflight", "twinkle", "burst", "ripple"):
        subprocess.run([str(exe), case], env=environment, check=True)
