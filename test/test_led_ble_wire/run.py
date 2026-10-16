"""Build and run the isolated wire tests with real platform HMAC, no device access."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", help="C++11 compiler; defaults to g++ or PlatformIO MinGW")
    args = parser.parse_args()
    compiler = args.cxx or shutil.which("g++")
    if not compiler and os.name == "nt":
        candidate = Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe"
        if candidate.is_file():
            compiler = str(candidate)
    if not compiler:
        parser.error("Provide --cxx or install g++; Linux also needs OpenSSL development headers.")
    spec = importlib.util.spec_from_file_location("wire_vectors", Path(__file__).with_name("generate_vectors.py"))
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    expected = json.loads((ROOT / "docs/led/fixtures/wire-v2.json").read_text(encoding="utf-8"))
    if generator.generate() != expected:
        raise RuntimeError("Committed wire vectors differ from independent Python HMAC output")
    environment = dict(os.environ)
    environment["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="larp-led-wire-") as directory:
        executable = Path(directory) / ("wire-test.exe" if os.name == "nt" else "wire-test")
        command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        if os.name == "nt":
            command += ["-static"]
        command += ["-I", str(ROOT / "firmware/led/include"),
                    str(ROOT / "test/test_led_ble_wire/main.cpp"),
                    str(ROOT / "firmware/led/src/LedWire.cpp"),
                    str(ROOT / "firmware/led/src/NfcBleWire.cpp"),
                    "-lbcrypt" if os.name == "nt" else "-lcrypto", "-o", str(executable)]
        subprocess.run(command, cwd=ROOT, env=environment, check=True)
        subprocess.run([str(executable), str(ROOT / "docs/led/fixtures/wire-v2.json")],
                       cwd=ROOT, env=environment, check=True)
    print("Public fixture generation and native wire/security tests passed.")
if __name__ == "__main__":
    main()
