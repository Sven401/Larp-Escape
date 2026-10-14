"""Prepare an unmodified, commit-pinned WLED source plus the local product usermod.

No build or upload is implicit. Generated files live below .pio/wled-source-stable.
"""
import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path, PurePosixPath

REVISION = "29b389df1c1aaec6ff53aea742d17063b985906c"
URL = "https://github.com/wled/WLED.git"
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / ".pio/wled-source-stable"


def git(*args, capture=False):
    return subprocess.run(["git", "-C", str(SOURCE), *args], check=True,
                          capture_output=capture, text=True).stdout



def file_inventory(directory):
    """Include ignored files and reject links before following any directories."""
    result = set()
    if not directory.exists():
        return result
    attributes = getattr(directory.lstat(), "st_file_attributes", 0)
    if directory.is_symlink() or attributes & 0x400:
        raise RuntimeError(f"Links are not allowed in build source: {directory}")
    for base, directories, files in os.walk(directory, followlinks=False):
        for name in directories + files:
            entry = Path(base) / name
            # Junctions/reparse points on Windows can also redirect compiler inputs.
            attributes = getattr(entry.lstat(), "st_file_attributes", 0)
            if entry.is_symlink() or attributes & 0x400:
                raise RuntimeError(f"Links are not allowed in build source: {entry}")
        for name in files:
            entry = Path(base) / name
            if not entry.is_file():
                raise RuntimeError(f"Non-regular build input: {entry}")
            result.add(entry.relative_to(directory).as_posix())
    return result


def verify_source_inventory():
    roots = ("wled00", "include", "lib")
    tracked = set(git("ls-files", "-z", "--", *roots, capture=True).split("\0"))
    actual = {f"{folder}/{relative}" for folder in roots
              for relative in file_inventory(SOURCE / folder)}
    # Upstream's user_config_copy.py produces exactly this copy. An existing local
    # config must never silently override the product's reviewed build flags.
    generated_config = "wled00/my_config.h"
    if generated_config in actual:
        if (SOURCE / generated_config).read_bytes() != (SOURCE / "wled00/my_config_sample.h").read_bytes():
            raise RuntimeError("Generated my_config.h differs from the pinned sample")
        actual.remove(generated_config)
    unexpected = actual - tracked
    if unexpected:
        raise RuntimeError("Unexpected WLED build inputs: " + ", ".join(sorted(unexpected)))


def read_generated_manifest(manifest, target):
    previous = json.loads(manifest.read_text(encoding="utf-8")) if manifest.exists() else []
    if not isinstance(previous, list) or not all(isinstance(value, str) for value in previous):
        raise RuntimeError("Invalid generated-file manifest")
    for relative in previous:
        path = PurePosixPath(relative)
        if (not relative or "\\" in relative or ":" in relative
                or path.is_absolute() or ".." in path.parts or path.as_posix() != relative
                or not (target / relative).resolve().is_relative_to(target.resolve())):
            raise RuntimeError("Invalid generated-file manifest path")
    return set(previous)


def copy_usermod(module, target, manifest):
    current = file_inventory(module)
    previous = read_generated_manifest(manifest, target)
    unexpected = file_inventory(target) - previous - current
    if unexpected:
        raise RuntimeError("Unexpected product usermod inputs: " + ", ".join(sorted(unexpected)))
    target.mkdir(parents=True, exist_ok=True)
    # Delete only obsolete files that this preparation step previously generated.
    for old in previous - current:
        obsolete = target / old
        if obsolete.is_file():
            obsolete.unlink()
    for relative in sorted(current):
        out = target / relative
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(module / relative, out)
    manifest.write_text(json.dumps(sorted(current), indent=2) + "\n", encoding="utf-8")

def prepare():
    if not (SOURCE / ".git").exists():
        if SOURCE.exists() and any(SOURCE.iterdir()):
            raise RuntimeError("Refusing nonempty source directory without Git metadata")
        SOURCE.mkdir(parents=True, exist_ok=True)
        git("init", "--quiet")
        git("remote", "add", "origin", URL)
        git("fetch", "--depth", "1", "origin", REVISION)
        git("checkout", "--detach", "FETCH_HEAD")
    if git("rev-parse", "HEAD", capture=True).strip() != REVISION:
        raise RuntimeError("WLED checkout is not the pinned revision")
    git("diff", "--quiet")
    git("diff", "--cached", "--quiet")
    origin = git("remote", "get-url", "origin", capture=True).strip()
    if origin != URL:
        raise RuntimeError("WLED checkout has an unexpected origin")
    verify_source_inventory()
    overlay = ROOT / "firmware/wled"
    for local, target in [("platformio_override.ini", "platformio_override.ini"),
                          ("partitions.csv", "larp_partitions.csv"),
                          ("verify_dependencies.py", "larp_verify_dependencies.py")]:
        shutil.copyfile(overlay / local, SOURCE / target)
    module = overlay / "usermods/larp_ble"
    target = SOURCE / "usermods/larp_ble"
    copy_usermod(module, target, SOURCE / ".larp-generated.json")
    print(f"WLED 16.0.1 {REVISION}: {SOURCE}")
    print("Prepared local product usermod; no build or upload performed.")


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    prepare()
