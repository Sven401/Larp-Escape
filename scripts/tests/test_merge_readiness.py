"""Policy boundary tests, plus project-agent and required-job consistency."""
import copy
import importlib.util
from pathlib import Path
import re
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("readiness", ROOT / "scripts/merge_readiness.py")
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
HEAD, BASE, MERGE = "a" * 40, "b" * 40, "c" * 40


def snapshot():
    return dict(repository=POLICY.REPOSITORY, target=POLICY.TARGET, state="open",
        head=HEAD, base=BASE, test_merge=MERGE, risk="routine", mergeable=True,
        draft=False, protected=True, rules_satisfied=True, all_pages_read=True,
        scope_accepted=True, evidence_complete=True, no_change_requests=True,
        threads_resolved=True, independent_review=True, author="author",
        last_pusher="pusher", implementer="developer-agent", reviewer="review-agent",
        files=[dict(filename="companion/android/app/src/main/example.kt", status="modified")],
        approvals=[dict(login="other-account", head=HEAD, state="APPROVED", eligible=True)],
        checks=[dict(name=n, workflow=".github/workflows/"+w, event="pull_request",
                     head=HEAD, base=BASE, tested_sha=MERGE, status="completed",
                     conclusion="success", app_id=POLICY.ACTIONS_APP_ID)
                for n, w in POLICY.CHECKS.items()])


class ReadinessTests(unittest.TestCase):
    def assert_blocked(self, data):
        self.assertTrue(POLICY.blockers(data, HEAD, BASE, MERGE))

    def test_complete_routine_snapshot(self):
        self.assertEqual([], POLICY.blockers(snapshot(), HEAD, BASE, MERGE))

    def test_missing_or_malformed_snapshot_fields(self):
        for key in snapshot():
            with self.subTest(key=key):
                data = snapshot(); del data[key]; self.assert_blocked(data)
        for value in (None, [], "yes"):
            self.assert_blocked(value)
        self.assertTrue(POLICY.blockers(snapshot(), "short", BASE, MERGE))
        data = snapshot(); data["draft"] = 0; self.assert_blocked(data)

    def test_changed_source_or_wrong_target(self):
        for key, value in (("head", BASE), ("base", HEAD), ("test_merge", HEAD),
                           ("target", "main"), ("repository", "other/repo"),
                           ("state", "closed"), ("mergeable", None), ("draft", True),
                           ("protected", False), ("threads_resolved", False),
                           ("no_change_requests", False), ("evidence_complete", False)):
            with self.subTest(key=key):
                data=snapshot(); data[key]=value; self.assert_blocked(data)

    def test_nonroutine_and_governance(self):
        for risk in ("security", "architecture", "destructive-migration", "upload", "release", "unknown"):
            data=snapshot(); data["risk"]=risk; self.assert_blocked(data)
        for path in ("AGENTS.md", "src/AGENTS.md", "src/AGENTS.override.md", ".github/workflows/quality.yml", ".codex/agents/reviewer.toml",
                     "docs/quality/merge-policy.md", "scripts/merge_readiness.py",
                     ".agents/skills/ui-ux-design/SKILL.md", "../escape", "a/../escape"):
            with self.subTest(path=path):
                data=snapshot(); data["files"]=[dict(filename=path, status="modified")]; self.assert_blocked(data)

    def test_rename_copy_and_removed_governance(self):
        for status in ("renamed", "copied"):
            data=snapshot()
            data["files"]=[dict(filename="docs/old.md", status=status, previous_filename="AGENTS.md")]
            self.assert_blocked(data)
            del data["files"][0]["previous_filename"]
            self.assert_blocked(data)
        data=snapshot(); data["files"]=[dict(filename="AGENTS.md", status="removed")]
        self.assert_blocked(data)
        data=snapshot(); data["files"]=["src/example.cpp"]
        self.assert_blocked(data)

    def test_independent_identities_and_current_approval(self):
        for login in ("author", "AUTHOR", "pusher", "", None):
            data=snapshot(); data["approvals"][0]["login"]=login; self.assert_blocked(data)
        for key,value in (("head", BASE), ("state", "DISMISSED"), ("eligible", False)):
            data=snapshot(); data["approvals"][0][key]=value; self.assert_blocked(data)
        data=snapshot(); data["reviewer"]=data["implementer"]; self.assert_blocked(data)

    def test_every_required_check_is_mandatory(self):
        for index in range(len(POLICY.CHECKS)):
            data=snapshot(); del data["checks"][index]; self.assert_blocked(data)

    def test_non_success_and_unqualified_runs(self):
        for conclusion in ("failure", "cancelled", "skipped", "neutral", "timed_out", None):
            data=snapshot(); data["checks"][0]["conclusion"]=conclusion; self.assert_blocked(data)
        for key,value in (("event", "push"), ("head", BASE), ("base", HEAD),
                          ("tested_sha", HEAD), ("status", "in_progress"),
                          ("workflow", ".github/workflows/fake.yml"), ("app_id", 1)):
            data=snapshot(); data["checks"][0][key]=value; self.assert_blocked(data)
        data=snapshot(); data["checks"].append(copy.deepcopy(data["checks"][0])); self.assert_blocked(data)

    def test_project_agent_definitions(self):
        names=set()
        for path in (ROOT/".codex/agents").glob("*.toml"):
            data=tomllib.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(path.stem, data["name"])
            self.assertNotIn(data["name"], names)
            names.add(data["name"])
            self.assertIn("rollenarchitektur.md", data["developer_instructions"])
            self.assertTrue(data["description"])
        self.assertEqual(names, {"project_manager", "maintainer", "reviewer",
            "android_developer", "firmware_developer", "game_designer", "ui_ux_designer"})

    def test_required_jobs_exist_and_pr_is_unfiltered(self):
        for name, workflow in POLICY.CHECKS.items():
            source=(ROOT/".github/workflows"/workflow).read_text(encoding="utf-8")
            self.assertIn("    name: "+name+"\n", source)
            self.assertRegex(source, r"(?m)^  pull_request:\s*$")
            block=source.split("  pull_request:",1)[1].split("  workflow_dispatch:",1)[0]
            # Only inspect the PR event block (push may follow it).
            block=block.split("  push:",1)[0]
            self.assertNotRegex(block, r"(?m)^    (paths|paths-ignore|branches|branches-ignore):")


if __name__ == "__main__":
    unittest.main()
