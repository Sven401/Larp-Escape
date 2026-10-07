#!/usr/bin/env python3
"""Exercise the exact native/SDK source guards on isolated temporary copies."""
import argparse
import contextlib
import importlib.util
import io
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
SCRIPTS = Path(__file__).resolve().parent


def rejected(action):
    try:
        action()
    except RuntimeError:
        return 1
    raise AssertionError("Changed crypto input was accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("native_peer_crypto_guard", SCRIPTS / "native_peer_mbedtls.py")
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    checked = native.audit(args.root, args.archive)
    with tempfile.TemporaryDirectory(prefix="larp-peer-crypto-guards-") as name:
        temporary = Path(name).resolve()
        if not temporary.is_relative_to(Path(tempfile.gettempdir()).resolve()):
            raise AssertionError("Temporary copy escaped the intended directory")
        copied = temporary / "source"
        for part in ("include", "library"):
            shutil.copytree(args.root / part, copied / part)
        audit = lambda: native.audit(copied, args.archive)
        assert audit() == checked
        negatives = 0
        for relative in ("include/mbedtls/config.h", "include/mbedtls/ecdsa.h", "library/ecdsa.c"):
            target = copied / relative
            original = target.read_bytes()
            target.write_bytes(original + b"\n/* changed source */\n")
            negatives += rejected(audit)
            target.write_bytes(original)
        removed = copied / "library/sha256.c"
        original = removed.read_bytes()
        removed.unlink()
        negatives += rejected(audit)
        removed.write_bytes(original)
        for relative in ("include/mbedtls/extra.h", "library/extra.c"):
            extra = copied / relative
            extra.write_bytes(b"/* extra unreviewed input */\n")
            negatives += rejected(audit)
            extra.unlink()
        assert audit() == checked
        changed_archive = temporary / "changed.tar.gz"
        changed_archive.write_bytes(args.archive.read_bytes() + b"changed")
        negatives += rejected(lambda: native.audit(copied, changed_archive))
        negatives += rejected(lambda: native.audit(copied, temporary / "missing.tar.gz"))

        sdk_copy = temporary / "sdk"
        sdk_lib = sdk_copy / "tools/sdk/esp32/lib"
        sdk_lib.mkdir(parents=True)
        for file in ("libbt.a", "libmbedcrypto.a"):
            shutil.copyfile(args.sdk / "tools/sdk/esp32/lib" / file, sdk_lib / file)
        environment = SimpleNamespace(PioPlatform=lambda: SimpleNamespace(get_package_dir=lambda package: str(sdk_copy)))

        def sdk_audit():
            with contextlib.redirect_stdout(io.StringIO()):
                runpy.run_path(str(SCRIPTS / "pin_gatt_source_sdk.py"), init_globals={"Import": lambda name: None, "env": environment})

        sdk_audit()
        sdk_negatives = 0
        crypto = sdk_lib / "libmbedcrypto.a"
        original = crypto.read_bytes()
        crypto.write_bytes(original + b"changed")
        sdk_negatives += rejected(sdk_audit)
        crypto.unlink()
        sdk_negatives += rejected(sdk_audit)
        crypto.write_bytes(original)
        sdk_audit()
    # Recheck every original after the temporary-only mutations/cleanup.
    assert native.audit(args.root, args.archive) == checked
    actual = SimpleNamespace(PioPlatform=lambda: SimpleNamespace(get_package_dir=lambda package: str(args.sdk)))
    with contextlib.redirect_stdout(io.StringIO()):
        runpy.run_path(str(SCRIPTS / "pin_gatt_source_sdk.py"), init_globals={"Import": lambda name: None, "env": actual})
    print(f"Crypto source guards passed: {checked} official files; {negatives} native and {sdk_negatives} SDK negative inputs rejected; original inputs unchanged")


if __name__ == "__main__":
    main()
