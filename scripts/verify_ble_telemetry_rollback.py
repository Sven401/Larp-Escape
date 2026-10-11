"""Verify the pre-BLE-telemetry Reichstagsbox backup; restore only on explicit command.

The default invocation reads local files only. --port invokes esptool flash_id,
which can reset the attached ESP32. No command writes flash except the separately
named restore-pre-telemetry with --confirm-restore-pre-telemetry.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


BACKUP_NAME = "before-ble-telemetry.bin"
EXPECTED_MAC = "a8:42:e3:cd:4b:18"
FLASH_BYTES = 4 * 1024 * 1024
EXPECTED_SHA256 = "155c425805863d9d0a4b9464116c17962ad04ffc6fb8956768c2be0411fce1ec"


def default_manifest():
    profile = os.environ.get("USERPROFILE")
    if not profile:
        raise ValueError("USERPROFILE is unavailable; supply --manifest")
    return Path(profile) / "AppData" / "Local" / "LarpEscape" / "ESP32-Backups" / "COM3-20261011-PreBleStackTelemetry" / "before-ble-telemetry.manifest.json"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_backup(manifest_path):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if (manifest.get("backup_file") != BACKUP_NAME or
            manifest.get("chip_mac", "").lower() != EXPECTED_MAC or
            manifest.get("flash_bytes") != FLASH_BYTES or
            manifest.get("flash_sha256", "").lower() != EXPECTED_SHA256 or
            manifest.get("captured_utc") != "2026-10-11"):
        raise ValueError("manifest does not identify the pinned pre-telemetry Reichstagsbox backup")
    backup = manifest_path.parent / BACKUP_NAME
    if backup.stat().st_size != FLASH_BYTES:
        raise ValueError("backup length differs from pinned 4 MiB flash size")
    if sha256(backup) != EXPECTED_SHA256:
        raise ValueError("backup SHA-256 differs from pinned fingerprint")
    return backup


def verify_device(python, esptool, port):
    if not port:
        raise ValueError("a port is required for device verification")
    command = [python, str(esptool), "--chip", "esp32", "--port", port, "flash_id"]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    output = result.stdout + "\n" + result.stderr
    mac = re.search(r"\bMAC:\s*((?:[0-9a-f]{2}:){5}[0-9a-f]{2})\b", output, re.I)
    size = re.search(r"Detected flash size:\s*(\d+)\s*MB\b", output, re.I)
    if not mac or mac.group(1).lower() != EXPECTED_MAC:
        raise ValueError("connected ESP32 MAC differs from pinned backup")
    if not size or int(size.group(1)) * 1024 * 1024 != FLASH_BYTES:
        raise ValueError("connected ESP32 flash size differs from pinned backup")
    return [python, str(esptool), "--chip", "esp32", "--port", port]


def restore_current(base, backup):
    # This writes the complete 4 MiB, including bootloader, NVS, OTA data and
    # partition table. No automatic retry or fallback to another image.
    subprocess.run(base + ["write_flash", "0x0", str(backup)], check=True)
    with tempfile.TemporaryDirectory(prefix="larp-rollback-readback-",
                                     dir=os.environ.get("LARP_ROLLBACK_TEMP_DIR")) as directory:
        readback = Path(directory) / "readback.bin"
        subprocess.run(base + ["read_flash", "0x0", hex(FLASH_BYTES), str(readback)], check=True)
        if readback.stat().st_size != FLASH_BYTES or sha256(readback) != EXPECTED_SHA256:
            raise ValueError("full-flash readback differs from the pinned pre-telemetry backup")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("verify", "restore-pre-telemetry"), nargs="?", default="verify")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--port", help="optional for verify; invokes flash_id and may reset the device")
    parser.add_argument("--esptool", type=Path, help="path to esptool.py; required with --port")
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--dry-run", action="store_true", help="restore preflight; no write or readback")
    parser.add_argument("--confirm-restore-pre-telemetry", action="store_true")
    args = parser.parse_args(argv)
    if args.action == "verify" and (args.dry_run or args.confirm_restore_pre_telemetry):
        parser.error("--dry-run and --confirm-restore-pre-telemetry require restore-pre-telemetry")
    if args.action == "restore-pre-telemetry" and not args.port:
        parser.error("restore-pre-telemetry requires --port")
    if args.action == "restore-pre-telemetry" and not args.dry_run and not args.confirm_restore_pre_telemetry:
        parser.error("restore-pre-telemetry requires --confirm-restore-pre-telemetry")
    if bool(args.port) != bool(args.esptool):
        parser.error("--port and --esptool must be supplied together")
    try:
        backup = verify_backup(args.manifest or default_manifest())
        print("backup verified: bytes={} sha256={}".format(FLASH_BYTES, EXPECTED_SHA256))
        base = verify_device(args.python, args.esptool, args.port) if args.port else None
        if base:
            print("device verified: mac={} flash_bytes={}".format(EXPECTED_MAC, FLASH_BYTES))
        if args.action == "restore-pre-telemetry":
            if args.dry_run:
                print("dry-run passed: no flash write or readback performed")
            else:
                restore_current(base, backup)
                print("restore-pre-telemetry verified by full 4 MiB readback SHA-256")
        return 0
    except (OSError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print("error: {}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
