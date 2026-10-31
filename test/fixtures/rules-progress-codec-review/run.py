"""Implementation representation checks; no owner/crypto integration claim."""
import argparse
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
    root = Path(__file__).resolve().parents[3]
    env = os.environ.copy()
    if args.binary:
        binary = args.binary.resolve()
    else:
        compiler = shutil.which(args.compiler)
        if compiler is None:
            raise SystemExit("C++11 compiler not found")
        if args.build_dir is None:
            raise SystemExit("--build-dir is required when compiling")
        compiler = Path(compiler).resolve()
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        binary = build / ("codec.exe" if os.name == "nt" else "codec")
        sources = [
            "GameExecution/RulesProgressCodec.cpp",
            "GameExecution/RulesPlanner.cpp",
            "GameConfiguration/PackageModules.cpp",
            "GameConfiguration/PackageDeclarations.cpp",
            "GameConfiguration/CanonicalPackageJson.cpp",
        ]
        subprocess.run(
            [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-I" + str(root / "include"), str(Path(__file__).with_name("main.cpp")),
             *[str(root / "src" / source) for source in sources], "-o", str(binary)],
            check=True, env=env,
        )
    subprocess.run([str(binary)], check=True, env=env)


if __name__ == "__main__":
    main()
