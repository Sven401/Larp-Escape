"""Own metadata/failure boundary tests, with explicitly synthetic policy seams."""
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
        if compiler is None or args.build_dir is None:
            raise SystemExit("Compiler and --build-dir are required")
        compiler = Path(compiler).resolve()
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        binary = build / ("evidence.exe" if os.name == "nt" else "evidence")
        sources = [
            "CompanionIntegration/RulesSignedEvidence.cpp",
            "Companion/D0Fact.cpp", "Companion/D0FactWriter.cpp",
            "GameExecution/RulesProgressCodec.cpp", "GameExecution/RulesPlanner.cpp",
            "GameConfiguration/PackageModules.cpp", "GameConfiguration/PackageDeclarations.cpp",
            "GameConfiguration/CanonicalPackageJson.cpp",
        ]
        subprocess.run(
            [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-I" + str(root / "include"), str(Path(__file__).with_name("main.cpp")),
             *[str(root / "src" / source) for source in sources],
             "-lbcrypt" if os.name == "nt" else "-lcrypto", "-o", str(binary)],
            check=True, env=env,
        )
    subprocess.run([str(binary)], check=True, env=env)


if __name__ == "__main__":
    main()
