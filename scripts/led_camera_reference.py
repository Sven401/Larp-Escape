"""Export the actual C++ LED engine's ideal submitted frames as CSV."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("effect", choices=["twinkle", "burst", "ripple"])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler or "g++")
    if compiler is None and args.compiler is None and os.name == "nt":
        candidate = Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe"
        if candidate.is_file():
            compiler = str(candidate)
    if compiler is None:
        parser.error("C++ compiler missing; use --compiler or CXX")
    root = Path(__file__).resolve().parents[1]
    firmware = root / "firmware/led"
    sources = [firmware / name for name in
               ("include/LedOutput.h", "src/LedOutput.cpp", "tests/export_camera_reference.cpp")]
    environment = os.environ.copy()
    environment["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="led-camera-reference-") as temp:
        exe = Path(temp) / ("reference.exe" if os.name == "nt" else "reference")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I", str(firmware / "include"), str(sources[1]), str(sources[2]),
                        "-o", str(exe)], check=True, env=environment)
        result = subprocess.check_output([str(exe), args.effect], env=environment)
    with args.output.open("xb") as stream:
        stream.write(result)
    args.output.with_suffix(args.output.suffix + ".json").write_text(json.dumps({
        "schema": 1, "effect": args.effect,
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "sources": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
        "csv_sha256": hashlib.sha256(result).hexdigest(),
        "meaning": "ideal host schedule, instant bus completion; not installed firmware or optical proof",
    }, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
