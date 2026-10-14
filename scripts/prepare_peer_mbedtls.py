#!/usr/bin/env python3
"""Fetch and audit the pinned official Mbed TLS 2.28.4 native sources."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import tempfile
import urllib.request

sys.dont_write_bytecode = True
from native_peer_mbedtls import ARCHIVE_SHA256, audit

ARCHIVE_URL = "https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/refs/tags/mbedtls-2.28.4"
ARCHIVE_NAME = "mbedtls-2.28.4.tar.gz"
ARCHIVE_ROOT = "mbedtls-mbedtls-2.28.4"


def verify_archive(archive):
    if archive.is_symlink() or not archive.is_file():
        raise RuntimeError("Mbed TLS archive must be a regular file")
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise RuntimeError("Mbed TLS archive does not match the pinned SHA-256; existing inputs are not replaced")


def extract_sources(archive, destination):
    # Never use extractall: only ordinary include/library files may be written.
    verify_archive(archive)
    with tarfile.open(archive, "r:gz") as source:
        for member in source.getmembers():
            path = PurePosixPath(member.name)
            if (path.is_absolute() or ".." in path.parts or "\\" in member.name
                    or ":" in member.name or not path.parts or path.parts[0] != ARCHIVE_ROOT):
                raise RuntimeError("Unsafe Mbed TLS archive path: " + member.name)
            if not member.isdir() and not member.isfile():
                raise RuntimeError("Mbed TLS archive contains a link or special file: " + member.name)
            relative = path.parts[1:]
            if not relative or relative[0] not in ("include", "library"):
                continue
            target = destination.joinpath(*relative)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as incoming, target.open("xb") as outgoing:
                    shutil.copyfileobj(incoming, outgoing)


def prepare(directory):
    directory = directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / ARCHIVE_NAME
    root = directory / "source"
    if root.is_symlink() or (root.exists() and not root.is_dir()):
        raise RuntimeError("Existing Mbed TLS source must be a directory, not a link or file")
    if archive.exists() or archive.is_symlink():
        verify_archive(archive)
    else:
        with tempfile.TemporaryDirectory(prefix="mbedtls-download-", dir=directory) as temporary:
            downloaded = Path(temporary) / ARCHIVE_NAME
            with urllib.request.urlopen(ARCHIVE_URL, timeout=60) as incoming, downloaded.open("xb") as outgoing:
                shutil.copyfileobj(incoming, outgoing)
            verify_archive(downloaded)
            downloaded.rename(archive)
    if root.exists():
        checked = audit(root, archive)
    else:
        with tempfile.TemporaryDirectory(prefix="mbedtls-extract-", dir=directory) as temporary:
            staged = Path(temporary) / "source"
            staged.mkdir()
            extract_sources(archive, staged)
            checked = audit(staged, archive)
            if root.exists() or root.is_symlink():
                raise RuntimeError("Mbed TLS source appeared during preparation; refusing to replace it")
            staged.rename(root)
    return root, archive, checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    try:
        root, archive, checked = prepare(args.directory)
    except (OSError, RuntimeError, tarfile.TarError) as error:
        parser.exit(1, f"Mbed TLS preparation failed: {error}\n")
    print(f"Native crypto sources ready: {checked} official files")
    print(f"LARP_PEER_MBEDTLS_DIR={root}")
    print(f"LARP_PEER_MBEDTLS_ARCHIVE={archive}")


if __name__ == "__main__":
    main()
