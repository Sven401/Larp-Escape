"""Exercise the SDK guard against real isolated Git repositories."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("check_idf_sources", Path(__file__).parents[1] / "check_idf_sources.py")
guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guard)


class SourceGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="larp-sdk-guard-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "sdk"
        self.child = self.base / "dependency"
        for root in (self.root, self.child):
            root.mkdir()
            self.git(root, "init", "-q")
            self.git(root, "config", "user.name", "Guard test")
            self.git(root, "config", "user.email", "guard@example.invalid")
            (root / "source.txt").write_text("original\n")
            self.git(root, "add", "--", "source.txt")
            self.git(root, "commit", "-qm", "fixture")
        self.git(self.root, "-c", "protocol.file.allow=always", "submodule", "add", "-q", str(self.child), "component")
        self.git(self.root, "commit", "-qam", "dependency")
        self.previous_pin = guard.IDF_COMMIT
        guard.IDF_COMMIT = self.git(self.root, "rev-parse", "HEAD")
        self.addCleanup(setattr, guard, "IDF_COMMIT", self.previous_pin)

    def git(self, root, *args):
        return subprocess.check_output(["git", "-C", str(root), *args], text=True, stderr=subprocess.STDOUT).strip()

    def check(self, root=None):
        with contextlib.redirect_stdout(io.StringIO()):
            guard.check(root or self.root)

    def test_clean_recursive_pin_is_accepted(self):
        self.check()

    def test_other_head_is_rejected(self):
        self.git(self.root, "commit", "--allow-empty", "-qm", "other")
        with self.assertRaisesRegex(ValueError, "commit differs"):
            self.check()

    def test_tracked_main_source_change_is_rejected(self):
        (self.root / "source.txt").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "tracked modifications"):
            self.check()

    def test_tracked_submodule_change_is_rejected(self):
        (self.root / "component/source.txt").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "tracked modifications"):
            self.check()

    def test_other_submodule_commit_is_rejected(self):
        self.git(self.child, "commit", "--allow-empty", "-qm", "new dependency")
        self.git(self.root / "component", "fetch", "-q")
        self.git(self.root / "component", "checkout", "-q", self.git(self.child, "rev-parse", "HEAD"))
        with self.assertRaisesRegex(ValueError, "submodule missing"):
            self.check()

    def test_uninitialised_submodule_is_rejected(self):
        clone = self.base / "incomplete"
        subprocess.check_output(["git", "clone", "-q", str(self.root), str(clone)], stderr=subprocess.STDOUT)
        with self.assertRaisesRegex(ValueError, "submodule missing"):
            self.check(clone)

    def test_subdirectory_cannot_masquerade_as_sdk(self):
        subdir = self.root / "somewhere"
        subdir.mkdir()
        with self.assertRaisesRegex(ValueError, "Git root"):
            self.check(subdir)


if __name__ == "__main__":
    unittest.main()
