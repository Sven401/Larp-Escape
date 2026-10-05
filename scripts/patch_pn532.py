#!/usr/bin/env python3
"""Pinned build repair of Adafruit PN532 1.3.4 UID response handling.

PlatformIO post extra script runs after library dependency discovery, before
compilation. Only this library's downloaded cpp file is modified. A verified
original is retained in its ignored dependency directory for local rollback.
Unknown source/version/patch content aborts the build; no network/download.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

PRISTINE_SHA256 = "9c6c93f6b1e4aac0f03394058c46673a0ff178e4c9fea0a80d0f9ceab53192f4"
PATCHED_SHA256 = "dff799bcb2237543bcfa875a28489b85da00c9ca8193e93f10fd734ca1a04bc3"
BACKUP = "_larp_pn532_original_1_3_4.txt"


def digest(text: str) -> str:
    # Repository/tool downloads may use CRLF. All checked hashes use normalized LF.
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def patch(library: Path, replacement: Path, restore: bool = False) -> bool:
    properties = {}
    for line in (library / "library.properties").read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            properties[key.strip()] = value.strip()
    if properties.get("name") != "Adafruit PN532" or properties.get("version") != "1.3.4":
        raise ValueError("requires exact Adafruit PN532 1.3.4; review a changed dependency")
    target = library / "Adafruit_PN532.cpp"
    current = target.read_text(encoding="utf-8")
    current_hash = digest(current)
    if current_hash not in (PRISTINE_SHA256, PATCHED_SHA256):
        raise ValueError("PN532 source differs from reviewed original/patch; refusing overwrite")
    backup = library / BACKUP
    if restore:
        if current_hash == PRISTINE_SHA256:
            return False
        original = backup.read_text(encoding="utf-8")
        if digest(original) != PRISTINE_SHA256:
            raise ValueError("original backup differs; refusing restore")
        target.write_bytes(original.encode("utf-8"))
        return True
    body = replacement.read_text(encoding="utf-8").rstrip()
    if current_hash == PATCHED_SHA256:
        # Still validate the submitted replacement against the verified backup.
        original = backup.read_text(encoding="utf-8")
    else:
        original = current
    if digest(original) != PRISTINE_SHA256:
        raise ValueError("original backup differs; refusing patch")
    start = original.index("bool Adafruit_PN532::readDetectedPassiveTargetID(")
    end = original.index("\n}\n", start) + 2
    repaired = original[:start] + body + original[end:]
    if digest(repaired) != PATCHED_SHA256:
        raise ValueError("replacement differs from reviewed patch; update hashes after review")
    if current_hash == PATCHED_SHA256:
        return False
    if backup.exists():
        if digest(backup.read_text(encoding="utf-8")) != PRISTINE_SHA256:
            raise ValueError("existing original backup differs; refusing overwrite")
    else:
        with backup.open("x", encoding="utf-8", newline="\n") as output:
            output.write(original)
    # Check the actual source again immediately before changing it.
    if digest(target.read_text(encoding="utf-8")) != current_hash:
        raise ValueError("PN532 source changed during preparation; retry after reconciliation")
    target.write_bytes(repaired.encode("utf-8"))
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path, help="exact downloaded Adafruit PN532 directory")
    parser.add_argument("--restore", action="store_true", help="restore the verified local original")
    arguments = parser.parse_args()
    replacement = Path(__file__).resolve().parent / "patches/pn532-1.3.4/read_detected.cpp"
    try:
        changed = patch(arguments.library, replacement, arguments.restore)
    except (OSError, ValueError) as error:
        parser.exit(1, f"PN532 repair rejected: {error}\n")
    print(f"PN532 1.3.4 {'restore' if arguments.restore else 'repair'}: "
          f"{'written' if changed else 'already present'}")
    return 0


if "Import" in globals():
    Import("env")
    project = Path(env.subst("$PROJECT_DIR"))
    library = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "Adafruit PN532"
    try:
        changed = patch(library, project / "scripts/patches/pn532-1.3.4/read_detected.cpp")
    except (OSError, ValueError) as error:
        raise RuntimeError(f"PN532 repair rejected: {error}") from error
    print(f"PN532 1.3.4 bounded UID repair: {'applied' if changed else 'verified'}")
elif __name__ == "__main__":
    raise SystemExit(main())
