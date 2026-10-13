#!/usr/bin/env python3
"""Compile each hardware-free public header as an independent C++11 unit.

The scope comes from check_architecture.LAYER_ROOTS. No Arduino/FastLED host
stubs, hardware-adapter include paths or PlatformIO source builds are used.
Only temporary translation units are created; project files remain unchanged.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

# Read-only checks must not create project-local __pycache__ files.
sys.dont_write_bytecode = True

from check_architecture import LAYER_ROOTS, PURE_LIBRARIES


HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".hxx"}
COMPILER_FLAGS = ("-std=c++11", "-fsyntax-only", "-Wall", "-Wextra", "-Wpedantic", "-Werror")


def find_compiler(requested: str | None) -> Path:
    if requested:
        candidate = Path(requested).expanduser()
        if candidate.is_file():
            return candidate.resolve()
        found = shutil.which(requested)
        if found:
            return Path(found).resolve()
        raise ValueError(f"C++ compiler not found: {requested}")
    found = shutil.which("g++")
    if found:
        return Path(found).resolve()
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    directory = core / "packages" / "toolchain-gccmingw32" / "bin"
    for name in ("g++.exe", "i686-w64-mingw32-g++.exe"):
        candidate = directory / name
        if candidate.is_file():
            return candidate.resolve()
    raise ValueError("g++ not found on PATH or in PlatformIO's MinGW package; pass --compiler")


def collect_headers(root: Path) -> list[Path]:
    if not (root / "platformio.ini").is_file():
        raise ValueError(f"{root}: no platformio.ini; pass the project directory with --root")
    headers = {
        path.resolve()
        for prefix, _ in LAYER_ROOTS
        for path in (root / prefix).rglob("*")
        if path.is_file() and path.suffix.casefold() in HEADER_SUFFIXES
    }
    for header in headers:
        if not header.is_relative_to(root):
            raise ValueError(f"header leaves the project root: {header}")
    if not headers:
        raise ValueError("no headers found in the reviewed hardware-free layers")
    return sorted(headers, key=lambda path: path.relative_to(root).as_posix())


def include_directories(root: Path) -> list[Path]:
    libraries = root / "lib"
    pure_roots = sorted(
        (path for path in libraries.iterdir()
         if path.is_dir() and path.name.casefold() in PURE_LIBRARIES),
        key=lambda path: path.name,
    ) if libraries.is_dir() else []
    return [root, root / "include", *pure_roots]


def check(root: Path, compiler: Path, verbose: bool) -> tuple[int, int]:
    headers = collect_headers(root)
    include_flags = [flag for directory in include_directories(root) for flag in ("-I", str(directory))]
    # GCC launches cc1plus from libexec; MinGW runtime DLLs live in bin.
    # PlatformIO normally adds that directory to PATH. Keep this adjustment
    # local to compiler subprocesses when the guard runs independently.
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    failures = 0
    with tempfile.TemporaryDirectory(prefix="larp-header-check-") as temporary:
        source = Path(temporary) / "header.cpp"
        for header in headers:
            relative = header.relative_to(root).as_posix()
            source.write_text(f'#include "{relative}"\nint main() {{ return 0; }}\n', encoding="utf-8")
            command = [str(compiler), *COMPILER_FLAGS, *include_flags, str(source)]
            try:
                result = subprocess.run(command, capture_output=True, text=True,
                                        encoding="utf-8", errors="replace", timeout=60, env=environment)
            except subprocess.TimeoutExpired as error:
                failures += 1
                print(f"FAIL {relative}: compiler exceeded 60 seconds", file=sys.stderr)
                for output in (error.stdout, error.stderr):
                    if output:
                        print(output.decode("utf-8", "replace") if isinstance(output, bytes) else output,
                              file=sys.stderr)
                continue
            if result.returncode:
                failures += 1
                print(f"FAIL {relative}: compiler exit code {result.returncode}", file=sys.stderr)
                print("Command: " + subprocess.list2cmdline(command), file=sys.stderr)
                if not result.stdout and not result.stderr:
                    print("Compiler returned no diagnostics; check its runtime/DLL installation.",
                          file=sys.stderr)
                if result.stdout:
                    print(result.stdout.rstrip(), file=sys.stderr)
                if result.stderr:
                    print(result.stderr.rstrip(), file=sys.stderr)
            elif verbose:
                print(f"PASS {relative}")
    return failures, len(headers)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="PlatformIO project root (default: this script's repository)")
    parser.add_argument("--compiler", help="C++ compiler executable; defaults to g++ or PlatformIO MinGW")
    parser.add_argument("--verbose", action="store_true", help="also list successfully compiled headers")
    arguments = parser.parse_args()
    try:
        root = arguments.root.expanduser().resolve()
        compiler = find_compiler(arguments.compiler)
        failures, total = check(root, compiler, arguments.verbose)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Header check could not run: {error}", file=sys.stderr)
        return 2
    if failures:
        print(f"Header check failed: {failures}/{total} headers. Compiler: {compiler}", file=sys.stderr)
        return 1
    print(f"Header check passed: {total} independent C++11 headers. Compiler: {compiler}")
    return 0


if __name__ == "__main__":
    sys.exit(main())