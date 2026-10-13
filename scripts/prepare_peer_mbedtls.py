#!/usr/bin/env python3
"""Prepare and audit the pinned official native peer-cryptography inputs."""

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import tempfile
from urllib.request import urlopen

sys.dont_write_bytecode = True

from native_peer_mbedtls import ARCHIVE_SHA256, audit


ARCHIVE_URL = "https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/refs/tags/mbedtls-2.28.4"
SOURCE_NAME = "mbedtls-2.28.4"


def verify_archive(archive):
    if archive.is_symlink() or not archive.is_file():
        raise RuntimeError("Expected a regular official Mbed TLS archive: " + str(archive))
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise RuntimeError("Mbed TLS archive SHA-256 mismatch; expected " + ARCHIVE_SHA256)


def download_archive(archive):
    # Only this newly created temporary file is removed if download/verification fails.
    with tempfile.NamedTemporaryFile(prefix=".mbedtls-download-", dir=archive.parent, delete=False) as output:
        temporary = Path(output.name)
        try:
            with urlopen(ARCHIVE_URL, timeout=60) as response:
                shutil.copyfileobj(response, output)
        except BaseException:
            output.close()
            temporary.unlink()
            raise
    try:
        verify_archive(temporary)
        # Exclusive creation preserves any file installed by another caller.
        with archive.open("xb") as output, temporary.open("rb") as source:
            shutil.copyfileobj(source, output)
    finally:
        temporary.unlink()


def extract_sources(archive, destination):
    """Copy only audited include/library inputs, without tar's link extraction."""
    verify_archive(archive)
    with tarfile.open(archive, "r:gz") as source:
        for member in source.getmembers():
            path = PurePosixPath(member.name)
            if (path.is_absolute() or ".." in path.parts or "\\" in member.name
                    or any(":" in part for part in path.parts)):
                raise RuntimeError("Unsafe archive path: " + member.name)
            if len(path.parts) < 2 or path.parts[1] not in ("include", "library"):
                continue
            relative = Path(*path.parts[1:])
            target = destination / relative
            if not target.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError("Archive path leaves the extraction directory: " + member.name)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as data, target.open("xb") as output:
                    shutil.copyfileobj(data, output)
            else:
                raise RuntimeError("Archive contains a link or special source entry: " + member.name)


def prepare(destination):
    destination = destination.expanduser()
    if destination.is_symlink():
        raise RuntimeError("Destination must not be a symbolic link")
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / (SOURCE_NAME + ".tar.gz")
    root = destination / SOURCE_NAME
    if not archive.exists() and not archive.is_symlink():
        download_archive(archive)
    verify_archive(archive)
    if root.is_symlink():
        raise RuntimeError("Existing source root must not be a symbolic link")
    if root.exists():
        checked = audit(root, archive)
    else:
        # The temporary directory belongs to this invocation; existing sources
        # are never deleted or rewritten, including after a failed audit.
        with tempfile.TemporaryDirectory(prefix=".mbedtls-extract-", dir=destination) as name:
            staging = Path(name) / SOURCE_NAME
            staging.mkdir()
            extract_sources(archive, staging)
            checked = audit(staging, archive)
            staging.rename(root)
        checked = audit(root, archive)
    return root, archive, checked


def export_environment(file, root, archive):
    values = {"LARP_PEER_MBEDTLS_DIR": root, "LARP_PEER_MBEDTLS_ARCHIVE": archive}
    if any("\n" in str(value) or "\r" in str(value) for value in values.values()):
        raise RuntimeError("Paths with line breaks cannot be exported to GitHub environment files")
    with file.open("a", encoding="utf-8", newline="\n") as output:
        for key, value in values.items():
            output.write(f"{key}={value}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True,
                        help="Dedicated directory for the verified archive and source tree")
    parser.add_argument("--github-env", type=Path,
                        help="Append verified source paths to this GitHub Actions environment file")
    args = parser.parse_args()
    try:
        root, archive, checked = prepare(args.destination)
        if args.github_env is not None:
            export_environment(args.github_env, root, archive)
    except (OSError, RuntimeError, tarfile.TarError, ValueError) as error:
        print("Native peer crypto preparation failed: " + str(error), file=sys.stderr)
        return 1
    print("Mbed TLS source: " + ARCHIVE_URL)
    print("Archive SHA-256: " + ARCHIVE_SHA256)
    print(f"Native crypto source audit passed: {checked} official include/library files")
    print("LARP_PEER_MBEDTLS_DIR=" + str(root))
    print("LARP_PEER_MBEDTLS_ARCHIVE=" + str(archive))
    return 0


if __name__ == "__main__":
    sys.exit(main())
