#!/usr/bin/env python3
"""Reject a different, incomplete or modified ESP-IDF security-qualification SDK."""
import argparse
from pathlib import Path
import subprocess

IDF_COMMIT = "9a97f6c54ec638111ce55cd36581b3c192f15207"


def check(root):
    def git(*args):
        return subprocess.check_output(["git", "-c", "core.longpaths=true", "-C", str(root), *args], text=True).strip()
    if git("rev-parse", "--show-toplevel").replace("\\", "/").casefold() != root.resolve().as_posix().casefold():
        raise ValueError("IDF_PATH must identify the SDK Git root")
    if git("rev-parse", "HEAD") != IDF_COMMIT:
        raise ValueError("ESP-IDF commit differs from the exact security-qualification pin")
    states = git("submodule", "status", "--recursive")
    if not states or any(line and line[0] in "-+U" for line in states.splitlines()):
        raise ValueError("ESP-IDF submodule missing, conflicted or at another commit")
    if git("status", "--porcelain", "--untracked-files=no"):
        raise ValueError("ESP-IDF or a submodule has tracked modifications")
    print("ESP-IDF source pin and recursive submodules verified:", IDF_COMMIT)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("idf", type=Path)
    args = parser.parse_args()
    try:
        check(args.idf)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"SDK source check failed: {error}\n")
