"""Compile the actual audio owner and UART adapter against deterministic fakes."""
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
    with tempfile.TemporaryDirectory(prefix="larp-audio-tests-") as temporary:
        for name, sources in (
            ("audio-output", ("src/AudioOutput.cpp", "tests/test_audio_output.cpp")),
            ("audio-uart", ("src/AudioOutput.cpp", "src/DfPlayerBus.cpp", "tests/test_audio_bus.cpp")),
        ):
            executable = Path(temporary) / (name + (".exe" if os.name == "nt" else ""))
            subprocess.run([
                compiler, "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                "-I", str(ROOT / "tests/audio_stubs"), "-I", str(ROOT / "include"),
                *(str(ROOT / source) for source in sources), "-o", str(executable),
            ], check=True, env=environment)
            cases = ("config", "partial", "tx_failure", "install_failure", "gpio_failure") if name == "audio-uart" else (None,)
            for case in cases:
                subprocess.run([str(executable)] + ([case] if case else []), check=True, env=environment)
    print("Audio output and SDK UART cases passed")

if __name__ == "__main__":
    main()
