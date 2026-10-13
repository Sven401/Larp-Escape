"""Exercise evidence collection with real, isolated child processes."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "quality_evidence.py"


class QualityEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="quality-evidence-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.repository = self.root / "repository"
        self.repository.mkdir()
        self.output = self.root / "evidence"
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith(("GIT_", "GITHUB_", "QUALITY_"))
        }
        self.environment["PYTHONUTF8"] = "1"
        self.git("init", "--quiet")
        self.git(
            "-c", "user.name=Evidence Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "-c", f"core.hooksPath={self.root / 'no-hooks'}",
            "commit", "--allow-empty", "--quiet", "-m", "Isolated test commit",
        )
        self.commit = self.git("rev-parse", "HEAD").stdout.strip()

    def git(self, *arguments):
        return subprocess.run(
            ["git", *arguments], cwd=self.repository, env=self.environment,
            text=True, encoding="utf-8", stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=True,
        )

    def invoke(self, *arguments):
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--output", str(self.output), *arguments],
            cwd=self.repository, env=self.environment, text=True, encoding="utf-8",
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    def initialize(self):
        result = self.invoke("init")
        self.assertEqual(result.returncode, 0, result.stderr)

    def read_result(self, name):
        return json.loads((self.output / f"{name}.json").read_text(encoding="utf-8"))

    def test_provenance_distinguishes_checkout_event_and_pull_request_head(self):
        self.environment.update({
            "GITHUB_SHA": "1" * 40,
            "QUALITY_HEAD_SHA": "2" * 40,
            "GITHUB_EVENT_NAME": "pull_request",
            "GITHUB_JOB": "hardware-independent",
            "GITHUB_SERVER_URL": "https://github.com",
            "GITHUB_REPOSITORY": "example/repository",
            "GITHUB_RUN_ID": "12345",
            "GITHUB_RUN_ATTEMPT": "2",
        })
        self.initialize()
        provenance = self.read_result("provenance")
        self.assertEqual(provenance["checked_out_commit"], self.commit)
        self.assertEqual(provenance["event_sha"], "1" * 40)
        self.assertEqual(provenance["pull_request_head_sha"], "2" * 40)
        self.assertEqual(provenance["event"], "pull_request")
        self.assertEqual(provenance["job"], "hardware-independent")
        self.assertEqual(provenance["run_attempt"], "2")
        self.assertEqual(
            provenance["run_url"], "https://github.com/example/repository/actions/runs/12345"
        )

    def test_child_exit_status_and_both_output_streams_are_preserved(self):
        self.initialize()
        child_directory = self.root / "child working directory"
        child_directory.mkdir()
        for exit_code in (0, 7):
            with self.subTest(exit_code=exit_code):
                name = f"child-{exit_code}"
                command = [sys.executable, "-c", (
                    "import os, sys; "
                    "print('stdout marker', flush=True); "
                    "print('stderr marker', file=sys.stderr, flush=True); "
                    "print(os.getcwd(), flush=True); "
                    f"sys.exit({exit_code})"
                )]
                result = self.invoke(
                    "run", "--name", name, "--cwd", str(child_directory), "--", *command
                )
                self.assertEqual(result.returncode, exit_code, result.stderr)
                evidence = self.read_result(name)
                self.assertEqual(evidence["exit_code"], exit_code)
                self.assertEqual(evidence["command"], command)
                self.assertEqual(Path(evidence["working_directory"]), child_directory)
                log = (self.output / evidence["log"]).read_text(encoding="utf-8")
                for marker in ("stdout marker", "stderr marker", str(child_directory)):
                    self.assertIn(marker, log)
                    self.assertIn(marker, result.stdout)

    def test_missing_executable_has_a_failed_result_and_error_log(self):
        self.initialize()
        missing = self.root / "missing-executable-for-evidence-test.exe"
        result = self.invoke("run", "--name", "missing", "--", str(missing))
        self.assertEqual(result.returncode, 1)
        evidence = self.read_result("missing")
        self.assertEqual(evidence["exit_code"], 1)
        self.assertEqual(evidence["command"], [str(missing)])
        self.assertTrue(result.stderr.strip())
        log = (self.output / evidence["log"]).read_text(encoding="utf-8")
        self.assertIn(result.stderr.strip(), log)

    @unittest.skipUnless(os.name == "nt", "The Android CI invokes a Windows batch wrapper")
    def test_windows_batch_wrapper_preserves_exit_status_and_arguments(self):
        self.initialize()
        child_directory = self.root / "batch working directory"
        child_directory.mkdir()
        (child_directory / "probe.bat").write_text(
            "@echo off\n"
            "echo batch stdout: %~1\n"
            "echo batch stderr 1>&2\n"
            "exit /b %~2\n",
            encoding="ascii",
        )
        for exit_code in (0, 7):
            with self.subTest(exit_code=exit_code):
                name = f"batch-{exit_code}"
                result = self.invoke(
                    "run", "--name", name, "--cwd", str(child_directory), "--",
                    "cmd", "/d", "/c", "probe.bat", "argument with spaces", str(exit_code),
                )
                self.assertEqual(result.returncode, exit_code, result.stderr)
                evidence = self.read_result(name)
                self.assertEqual(evidence["exit_code"], exit_code)
                log = (self.output / evidence["log"]).read_text(encoding="utf-8")
                for marker in ("batch stdout: argument with spaces", "batch stderr"):
                    self.assertIn(marker, log)
                    self.assertIn(marker, result.stdout)

    def test_summary_keeps_failure_missing_checks_and_commit_identity_explicit(self):
        self.environment.update({
            "QUALITY_HEAD_SHA": "2" * 40,
            "QUALITY_JOB_STATUS": "failure",
            "GITHUB_STEP_SUMMARY": str(self.root / "step-summary.md"),
        })
        summary_path = Path(self.environment["GITHUB_STEP_SUMMARY"])
        summary_path.write_text("Earlier step summary\n", encoding="utf-8")
        self.initialize()
        result = self.invoke(
            "run", "--name", "failed-check", "--", sys.executable,
            "-c", "import sys; sys.exit(7)",
        )
        self.assertEqual(result.returncode, 7, result.stderr)
        finished = self.invoke("finish")
        self.assertEqual(finished.returncode, 0, finished.stderr)
        summary = (self.output / "summary.md").read_text(encoding="utf-8")
        self.assertIn(f"Checked-out commit: `{self.commit}`", summary)
        self.assertIn(f"PR head: `{'2' * 40}`", summary)
        self.assertIn("Job status before reporting: **failure**", summary)
        self.assertIn("| failed-check |", summary)
        self.assertIn("| 7 |", summary)
        self.assertIn("A missing row is not a passed check.", summary)
        check_rows = [line for line in summary.splitlines() if line.startswith("| ")]
        self.assertEqual(len(check_rows), 3, "Only the header, separator and executed check belong here")
        self.assertEqual(
            summary_path.read_text(encoding="utf-8"), "Earlier step summary\n" + summary
        )
        self.assertEqual(self.read_result("failed-check")["exit_code"], 7)

    def test_duplicate_initialization_does_not_replace_provenance(self):
        self.initialize()
        provenance = (self.output / "provenance.json").read_bytes()
        self.environment["QUALITY_HEAD_SHA"] = "3" * 40
        result = self.invoke("init")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.output / "provenance.json").read_bytes(), provenance)

    def test_duplicate_check_does_not_replace_first_failure(self):
        self.initialize()
        result = self.invoke(
            "run", "--name", "once", "--", sys.executable, "-c", "raise SystemExit(7)"
        )
        self.assertEqual(result.returncode, 7, result.stderr)
        original_result = (self.output / "once.json").read_bytes()
        original_log = (self.output / "once.log").read_bytes()
        repeated = self.invoke(
            "run", "--name", "once", "--", sys.executable, "-c", "pass"
        )
        self.assertNotEqual(repeated.returncode, 0)
        self.assertEqual((self.output / "once.json").read_bytes(), original_result)
        self.assertEqual((self.output / "once.log").read_bytes(), original_log)

    def test_run_without_initialization_cannot_create_evidence(self):
        result = self.invoke(
            "run", "--name", "uninitialized", "--", sys.executable, "-c", "pass"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
