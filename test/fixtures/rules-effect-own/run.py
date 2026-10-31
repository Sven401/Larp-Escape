"""Own byte/header boundaries; no independent review or physical-effect claim."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    env = os.environ.copy()
    if args.binary:
        binary = args.binary.resolve()
    else:
        compiler = shutil.which(args.compiler)
        if compiler is None or args.build_dir is None:
            raise SystemExit("Compiler and --build-dir required")
        compiler = Path(compiler).resolve()
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        binary = build / ("effect.exe" if os.name == "nt" else "effect")
        subprocess.run([str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-I" + str(root / "include"), str(here / "main.cpp"),
                        str(root / "src/DeviceRuntime/RulesProgressEffect.cpp"), "-o", str(binary)],
                       check=True, env=env)
    vectors = json.loads((here / "vectors.json").read_text(encoding="utf8"))
    data = "".join(f'{v["phase"]} {v["result"]} {v["index"]} {v["hex"]}\n' for v in vectors)
    subprocess.run([str(binary)], input=data, text=True, check=True, env=env)


if __name__ == "__main__":
    main()
