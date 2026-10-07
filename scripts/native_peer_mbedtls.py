"""Build only the audited official Mbed TLS2.28.4 native sources.

The standalone --audit invocation shares the exact pre-build guard. Source
roots are external inputs; no vendored source or fake crypto fallback exists.
"""
import argparse
import hashlib
import os
from pathlib import Path
import tarfile

ARCHIVE_SHA256 = "504bd29af6e7f9f3de1f0f7b7e16c73987a4194338681acd72b82383a49d55d5"


def audit(root, archive):
    root, archive = Path(root).resolve(), Path(archive)
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise RuntimeError("Provide the pinned official Mbed TLS2.28.4 archive; native crypto has no fake fallback")
    expected = set()
    with tarfile.open(archive, "r:gz") as source:
        for member in source.getmembers():
            relative = Path(*Path(member.name).parts[1:])
            if not member.isfile() or not relative.parts or relative.parts[0] not in ("include", "library"):
                continue
            path = root / relative
            if path.is_symlink() or not path.resolve().is_relative_to(root) or not path.is_file() or path.read_bytes() != source.extractfile(member).read():
                raise RuntimeError("Native Mbed TLS source differs from official archive: " + relative.as_posix())
            expected.add(relative.as_posix())
    if len(expected) < 100:
        raise RuntimeError("Incomplete native crypto source audit")
    actual = set()
    for part in ("include", "library"):
        directory = root / part
        if directory.is_symlink():
            raise RuntimeError("Native crypto source directory is a symbolic link")
        for path in directory.rglob("*"):
            if path.is_symlink():
                raise RuntimeError("Native crypto source contains a symbolic link")
            if path.is_file():
                actual.add(path.relative_to(root).as_posix())
    if actual != expected:
        raise RuntimeError("Native crypto source inventory differs from the official archive")
    return len(expected)


def configure(env):
    root = Path(os.environ.get("LARP_PEER_MBEDTLS_DIR", ""))
    archive = Path(os.environ.get("LARP_PEER_MBEDTLS_ARCHIVE", ""))
    checked = audit(root, archive)
    env.Append(CPPPATH=[str(root / "include")])
    # Vendor C has its own compile boundary; project C++ retains Werror.
    crypto = env.Clone()
    toolchain = Path(env.PioPlatform().get_package_dir("toolchain-gccmingw32")) / "bin"
    crypto.Replace(CC=str(toolchain / "gcc.exe"), AR=str(toolchain / "ar.exe"), RANLIB=str(toolchain / "ranlib.exe"))
    crypto["ENV"]["PATH"] = str(toolchain) + os.pathsep + crypto["ENV"].get("PATH", "")
    crypto.Replace(CCFLAGS=[v for v in crypto.get("CCFLAGS", []) if v not in ("-std=gnu++11", "-Werror", "-Wpedantic")])
    crypto.Append(CCFLAGS=["-std=c99"])
    library = crypto.BuildLibrary("$BUILD_DIR/peer_mbedtls", str(root / "library"))
    env.Append(LIBS=[library])
    print(f"Native peer crypto: {checked} official include/library files checked; real source library build")


if "Import" in globals():
    Import("env")
    configure(env)
elif __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audit", action="store_true", required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    args = parser.parse_args()
    print(f"Native crypto source audit passed: {audit(args.root, args.archive)} official files")
