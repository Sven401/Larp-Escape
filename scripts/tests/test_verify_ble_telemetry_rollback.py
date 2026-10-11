"""Offline tests. The fake esptool only copies local fixture files."""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


SOURCE = Path(__file__).resolve().parents[1] / "verify_ble_telemetry_rollback.py"
SPEC = importlib.util.spec_from_file_location("verify_ble_telemetry_rollback", SOURCE)
rollback = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rollback)

FAKE_ESPTOOL = '''import os, pathlib, shutil, sys
args = sys.argv[1:]
with open(os.environ["FAKE_ESPTOOL_LOG"], "a", encoding="ascii") as output:
    output.write(" ".join(args) + "\\n")
if "flash_id" in args:
    print("MAC: " + os.environ.get("FAKE_MAC", "a8:42:e3:cd:4b:18"))
    print("Detected flash size: " + os.environ.get("FAKE_SIZE", "4") + "MB")
elif "write_flash" in args:
    shutil.copyfile(args[-1], os.environ["FAKE_FLASH"])
elif "read_flash" in args:
    shutil.copyfile(os.environ["FAKE_FLASH"], args[-1])
    if os.environ.get("FAKE_CORRUPT_READBACK") == "1":
        with open(args[-1], "r+b") as target:
            target.write(b"z")
else:
    sys.exit(3)
'''


class BleTelemetryRollbackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=Path.cwd())
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.backup = self.root / rollback.BACKUP_NAME
        self.backup.write_bytes(b"y" * rollback.FLASH_BYTES)
        self.assertEqual(self.backup.stat().st_size, rollback.FLASH_BYTES)
        self.digest = hashlib.sha256(self.backup.read_bytes()).hexdigest()
        self.manifest = self.root / "before-ble-telemetry.manifest.json"
        self.manifest.write_text(json.dumps({
            "backup_file": rollback.BACKUP_NAME,
            "chip_mac": rollback.EXPECTED_MAC,
            "flash_bytes": rollback.FLASH_BYTES,
            "flash_sha256": self.digest,
            "captured_utc": "2026-10-11",
        }), encoding="utf-8")
        self.esptool = self.root / "fake_esptool.py"
        self.esptool.write_text(FAKE_ESPTOOL, encoding="utf-8")
        self.log = self.root / "esptool.log"
        self.flash = self.root / "fake-flash.bin"
        self.env = patch.dict(os.environ, {
            "FAKE_ESPTOOL_LOG": str(self.log),
            "FAKE_FLASH": str(self.flash),
            "LARP_ROLLBACK_TEMP_DIR": str(self.root),
        })
        self.env.start()
        self.addCleanup(self.env.stop)
        self.fingerprint = patch.object(rollback, "EXPECTED_SHA256", self.digest)
        self.fingerprint.start()
        self.addCleanup(self.fingerprint.stop)

    def invoke(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                code = rollback.main(["--manifest", str(self.manifest), *args])
            except SystemExit as error:
                code = error.code
        return code, stdout.getvalue(), stderr.getvalue()

    def port_args(self):
        return ("--port", "COM3", "--esptool", str(self.esptool))

    def log_text(self):
        return self.log.read_text(encoding="ascii") if self.log.exists() else ""

    def test_default_verify_is_offline(self):
        code, output, _ = self.invoke()
        self.assertEqual(code, 0)
        self.assertIn("backup verified", output)
        self.assertFalse(self.log.exists())

    def test_hash_and_length_reject_before_device_access(self):
        self.backup.write_bytes(b"short")
        self.assertEqual(self.invoke("verify", *self.port_args())[0], 2)
        self.assertFalse(self.log.exists())
        self.backup.write_bytes(b"x" * rollback.FLASH_BYTES)
        self.assertEqual(self.invoke("verify", *self.port_args())[0], 2)
        self.assertFalse(self.log.exists())

    def test_wrong_mac_and_flash_size_reject_without_write(self):
        with patch.dict(os.environ, {"FAKE_MAC": "a8:42:e3:ce:de:d8"}):
            self.assertEqual(self.invoke("restore-pre-telemetry", "--dry-run", *self.port_args())[0], 2)
        with patch.dict(os.environ, {"FAKE_SIZE": "8"}):
            self.assertEqual(self.invoke("restore-pre-telemetry", "--dry-run", *self.port_args())[0], 2)
        self.assertNotIn("write_flash", self.log_text())

    def test_missing_confirmation_rejects_before_flash_id(self):
        self.assertNotEqual(self.invoke("restore-pre-telemetry", *self.port_args())[0], 0)
        self.assertFalse(self.log.exists())

    def test_correct_dry_run_only_checks_identity(self):
        code, output, _ = self.invoke("restore-pre-telemetry", "--dry-run", *self.port_args())
        self.assertEqual(code, 0)
        self.assertIn("dry-run passed", output)
        self.assertIn("flash_id", self.log_text())
        self.assertNotIn("write_flash", self.log_text())
        self.assertNotIn("read_flash", self.log_text())

    def test_confirmed_fake_restore_reads_back_full_image(self):
        code, output, _ = self.invoke("restore-pre-telemetry", "--confirm-restore-pre-telemetry", *self.port_args())
        self.assertEqual(code, 0)
        self.assertIn("full 4 MiB readback", output)
        self.assertIn("write_flash 0x0", self.log_text())
        self.assertIn("read_flash 0x0 0x400000", self.log_text())
        self.assertEqual(hashlib.sha256(self.flash.read_bytes()).hexdigest(), self.digest)

    def test_readback_mismatch_fails_after_fake_write(self):
        with patch.dict(os.environ, {"FAKE_CORRUPT_READBACK": "1"}):
            code, _, error = self.invoke("restore-pre-telemetry", "--confirm-restore-pre-telemetry", *self.port_args())
        self.assertEqual(code, 2)
        self.assertIn("readback differs", error)


if __name__ == "__main__":
    unittest.main()
