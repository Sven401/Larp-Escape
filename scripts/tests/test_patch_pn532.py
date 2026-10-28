"""Exercise PN532 repair safety in temporary libraries, without downloads.

Synthetic sources and matching test pins isolate the helper's mutation contract.
They do not certify the real upstream/patch hashes or hardware behaviour.
"""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import patch_pn532 as repair


class Pn532RepairTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="larp-pn532-repair-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.target = self.root / "Adafruit_PN532.cpp"
        self.properties = self.root / "library.properties"
        self.properties.write_text("name=Adafruit PN532\nversion=1.3.4\n", encoding="utf-8")
        self.original = "// retained prefix\nbool Adafruit_PN532::readDetectedPassiveTargetID() {\n    return false;\n}\n// retained suffix\n"
        self.body = "bool Adafruit_PN532::readDetectedPassiveTargetID() {\n    return true;\n}"
        self.patched = "// retained prefix\n" + self.body + "\n// retained suffix\n"
        self.target.write_text(self.original, encoding="utf-8", newline="\n")
        self.replacement = self.root / "replacement.cpp"
        self.replacement.write_text(self.body + "\n", encoding="utf-8")
        self.backup = self.root / repair.BACKUP
        self.pins = mock.patch.multiple(repair, PRISTINE_SHA256=repair.digest(self.original),
                                       PATCHED_SHA256=repair.digest(self.patched))
        self.pins.start()
        self.addCleanup(self.pins.stop)

    def apply(self):
        return repair.patch(self.root, self.replacement)

    def test_apply_verify_restore_preserves_surrounding_source(self):
        self.assertTrue(self.apply())
        self.assertEqual(self.target.read_text(encoding="utf-8"), self.patched)
        self.assertEqual(self.backup.read_text(encoding="utf-8"), self.original)
        self.assertFalse(self.apply())
        self.assertTrue(repair.patch(self.root, self.replacement, restore=True))
        self.assertEqual(self.target.read_text(encoding="utf-8"), self.original)
        self.assertFalse(repair.patch(self.root, self.replacement, restore=True))
        self.assertTrue(self.apply())
        self.assertEqual(self.target.read_text(encoding="utf-8"), self.patched)

    def test_source_change_during_preparation_is_preserved(self):
        real_read = Path.read_text
        foreign = self.original + "// concurrent change\n"

        def read_and_change(path, *args, **kwargs):
            text = real_read(path, *args, **kwargs)
            if path == self.replacement:
                self.target.write_text(foreign, encoding="utf-8")
            return text

        # Exercises the helper's second read, not atomicity of read/write or restore.
        with mock.patch.object(Path, "read_text", read_and_change):
            with self.assertRaisesRegex(ValueError, "source changed during preparation"):
                self.apply()
        self.assertEqual(self.target.read_text(encoding="utf-8"), foreign)
        self.assertEqual(self.backup.read_text(encoding="utf-8"), self.original)

    def test_wrong_library_or_version_is_rejected_before_any_mutation(self):
        for content in ("name=Other\nversion=1.3.4\n", "name=Adafruit PN532\nversion=1.3.5\n"):
            with self.subTest(properties=content):
                self.properties.write_text(content, encoding="utf-8")
                before = self.target.read_bytes()
                with self.assertRaisesRegex(ValueError, "requires exact"):
                    self.apply()
                self.assertEqual(self.target.read_bytes(), before)
                self.assertFalse(self.backup.exists())

    def test_unknown_source_is_preserved(self):
        self.target.write_text(self.original + "// foreign edit\n", encoding="utf-8")
        before = self.target.read_bytes()
        with self.assertRaisesRegex(ValueError, "source differs"):
            self.apply()
        self.assertEqual(self.target.read_bytes(), before)
        self.assertFalse(self.backup.exists())

    def test_changed_replacement_is_rejected_without_backup_or_rewrite(self):
        self.replacement.write_text(self.body + "// changed", encoding="utf-8")
        before = self.target.read_bytes()
        with self.assertRaisesRegex(ValueError, "replacement differs"):
            self.apply()
        self.assertEqual(self.target.read_bytes(), before)
        self.assertFalse(self.backup.exists())

    def test_existing_foreign_backup_is_preserved(self):
        self.backup.write_bytes(b"foreign backup")
        before = self.target.read_bytes()
        with self.assertRaisesRegex(ValueError, "existing original backup differs"):
            self.apply()
        self.assertEqual(self.target.read_bytes(), before)
        self.assertEqual(self.backup.read_bytes(), b"foreign backup")

    def test_already_patched_source_requires_original_backup(self):
        self.assertTrue(self.apply())
        self.backup.unlink()
        before = self.target.read_bytes()
        with self.assertRaises(FileNotFoundError):
            self.apply()
        self.assertEqual(self.target.read_bytes(), before)

    def test_corrupt_backup_blocks_verification_and_restore(self):
        self.assertTrue(self.apply())
        self.backup.write_bytes(b"corrupt original")
        before = self.target.read_bytes()
        for restore in (False, True):
            with self.subTest(restore=restore), self.assertRaisesRegex(ValueError, "original backup differs"):
                repair.patch(self.root, self.replacement, restore=restore)
            self.assertEqual(self.target.read_bytes(), before)
            self.assertEqual(self.backup.read_bytes(), b"corrupt original")

    def test_replacement_is_revalidated_even_when_source_already_patched(self):
        self.assertTrue(self.apply())
        self.replacement.write_text(self.body + "// changed", encoding="utf-8")
        before = self.target.read_bytes()
        with self.assertRaisesRegex(ValueError, "replacement differs"):
            self.apply()
        self.assertEqual(self.target.read_bytes(), before)

    def test_crlf_source_is_normalized_and_backup_is_reusable(self):
        self.target.write_bytes(self.original.replace("\n", "\r\n").encode("utf-8"))
        self.assertTrue(self.apply())
        self.assertFalse(self.apply())
        self.assertTrue(repair.patch(self.root, self.replacement, restore=True))
        self.assertEqual(self.target.read_text(encoding="utf-8"), self.original)


if __name__ == "__main__":
    unittest.main()
