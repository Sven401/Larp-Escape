"""Host regressions for contaminated or stale prepared WLED source trees."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("prepare_wled", ROOT / "scripts/prepare_wled.py")
prepare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare)


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "upstream"
        for directory in ("wled00", "include", "lib"):
            (self.source / directory).mkdir(parents=True)
        self.sample = self.source / "wled00/my_config_sample.h"
        self.sample.write_text("// pinned upstream sample\n", encoding="utf-8")
        self.original_source = prepare.SOURCE
        prepare.SOURCE = self.source
        self.addCleanup(setattr, prepare, "SOURCE", self.original_source)
        self.git_patch = patch.object(prepare, "git", return_value="wled00/my_config_sample.h\0")
        self.git_patch.start()
        self.addCleanup(self.git_patch.stop)

    def test_clean_inventory_and_exact_generated_config_are_allowed(self):
        prepare.verify_source_inventory()
        (self.source / "wled00/my_config.h").write_bytes(self.sample.read_bytes())
        prepare.verify_source_inventory()

    def test_untracked_translation_unit_is_rejected(self):
        (self.source / "wled00/extra.cpp").write_text("void injected() {}")
        with self.assertRaisesRegex(RuntimeError, "extra.cpp"):
            prepare.verify_source_inventory()

    def test_ignored_header_is_not_hidden_by_git_rules(self):
        # The inventory reads actual files, not git status (which can omit ignores).
        (self.source / "include/ignored.h").write_text("#define INJECTED 1")
        with self.assertRaisesRegex(RuntimeError, "ignored.h"):
            prepare.verify_source_inventory()

    def test_modified_generated_config_is_rejected(self):
        (self.source / "wled00/my_config.h").write_text("#define UNREVIEWED_CONFIG 1")
        with self.assertRaisesRegex(RuntimeError, "differs"):
            prepare.verify_source_inventory()

    def module_fixture(self):
        module = self.root / "module"
        target = self.root / "target"
        module.mkdir()
        target.mkdir()
        (module / "current.cpp").write_text("void current() {}")
        return module, target, self.root / "manifest.json"

    def test_unknown_target_source_is_rejected_without_deleting_it(self):
        module, target, manifest = self.module_fixture()
        unexpected = target / "extra.cpp"
        unexpected.write_text("void extra() {}")
        with self.assertRaisesRegex(RuntimeError, "extra.cpp"):
            prepare.copy_usermod(module, target, manifest)
        self.assertTrue(unexpected.exists())
        self.assertFalse(manifest.exists())

    def test_only_manifest_owned_stale_file_is_removed(self):
        module, target, manifest = self.module_fixture()
        (target / "old.cpp").write_text("void old() {}")
        manifest.write_text(json.dumps(["old.cpp"]))
        prepare.copy_usermod(module, target, manifest)
        self.assertFalse((target / "old.cpp").exists())
        self.assertEqual((target / "current.cpp").read_bytes(), (module / "current.cpp").read_bytes())
        self.assertEqual(json.loads(manifest.read_text()), ["current.cpp"])

    def test_manifest_cannot_delete_outside_target(self):
        module, target, manifest = self.module_fixture()
        outside = self.root / "outside.cpp"
        outside.write_text("preserve")
        for name in ("../outside.cpp", "C:/outside.cpp", "..\\outside.cpp"):
            with self.subTest(name=name):
                manifest.write_text(json.dumps([name]))
                with self.assertRaisesRegex(RuntimeError, "manifest path"):
                    prepare.copy_usermod(module, target, manifest)
                self.assertEqual(outside.read_text(), "preserve")


if __name__ == "__main__":
    unittest.main()
