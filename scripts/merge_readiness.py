"""Fail-closed helper for a reviewer-owned, normalized GitHub read snapshot.

No network, credential access, approvals or mutations. The caller must freshly
read GitHub, paginate reviews/threads/checks completely, and independently pass
expected head/base/test-merge SHAs. This JSON is evidence input, not an approval
mechanism or a substitute for server-side branch protection. Run:
python scripts/merge_readiness.py snapshot.json --head SHA --base SHA --merge SHA
Exit 0 means the supplied snapshot meets the local routine policy; never trust
an author-supplied snapshot without independently rebuilding its evidence.
"""
import argparse
import json
import re
from pathlib import Path

TARGET = "plan/projektuebergabe-kuratierter-stand"
REPOSITORY = "Sven401/Larp-Escape"
CHECKS = {
    "Quality / Host": "quality.yml",
    "Quality / Firmware": "quality.yml",
    "Quality / Android": "quality.yml",
    "LED / Camera": "led-product.yml",
    "LED / Protocol": "led-product.yml",
    "LED / Firmware": "led-product.yml",
    "LED / Android": "led-product.yml",
    "NFC / Tests": "nfc-probe.yml",
    "NFC / Firmware": "nfc-probe.yml",
    "ESP-NOW / Firmware": "espnow-probe.yml",
    "Native / Qualification": "native-idf.yml",
    "Roles / Policy": "role-policy.yml",
}
# GitHub Actions App ID, independently verify on live checks during setup.
ACTIONS_APP_ID = 15368
GOVERNANCE = (
    "AGENTS.md", ".codex/", ".agents/", ".github/", "PROMOTION.md",
    "docs/quality/", "scripts/merge_readiness.py",
    "scripts/tests/test_merge_readiness.py", "docs/ui-ux/ROLLE.md",
    "docs/led/rollen/",
)


def blockers(data, head, base, merge):
    """Return all blockers; malformed/missing evidence never passes."""
    if not isinstance(data, dict):
        return ["invalid snapshot"]
    errors = []
    if not all(isinstance(s, str) and re.fullmatch(r"[0-9a-f]{40}", s)
               for s in (head, base, merge)):
        return ["invalid expected SHA"]
    for key, expected in {
        "repository": REPOSITORY, "target": TARGET, "state": "open",
        "head": head, "base": base, "test_merge": merge,
        "risk": "routine", "mergeable": True, "draft": False,
        "protected": True, "rules_satisfied": True,
        "all_pages_read": True, "scope_accepted": True,
        "evidence_complete": True, "no_change_requests": True,
        "threads_resolved": True, "independent_review": True,
    }.items():
        actual = data.get(key)
        if actual != expected or (isinstance(expected, bool) and type(actual) is not bool):
            errors.append("missing or unsatisfied " + key)
    author, pusher, implementer, reviewer = (data.get(k) for k in
        ("author", "last_pusher", "implementer", "reviewer"))
    if not all(isinstance(v, str) and v.strip() for v in
               (author, pusher, implementer, reviewer)) or str(implementer).casefold() == str(reviewer).casefold():
        errors.append("independent reviewer identity missing")
    files = data.get("files")
    paths = []
    valid_files = isinstance(files, list) and bool(files)
    for item in files if isinstance(files, list) else []:
        if not isinstance(item, dict) or item.get("status") not in {
                "added", "modified", "removed", "renamed", "copied", "changed"}:
            valid_files = False
            continue
        names = [item.get("filename")]
        if item.get("status") in {"renamed", "copied"} or "previous_filename" in item:
            names.append(item.get("previous_filename"))
        for name in names:
            if not isinstance(name, str) or not name or name.startswith(("/", "\\")) or "\\" in name or ".." in name.split("/"):
                valid_files = False
            else:
                paths.append(name)
    if not valid_files:
        errors.append("complete GitHub file entries including rename origins required")
    if any(any(p == prefix or p.startswith(prefix) for prefix in GOVERNANCE)
           for p in paths):
        errors.append("governance requires human approval")
    approvals = data.get("approvals")
    if not isinstance(approvals, list) or not any(
        isinstance(a, dict) and isinstance(a.get("login"), str)
        and a["login"].strip()
        and a["login"].casefold() not in {
            str(author).casefold(), str(pusher).casefold()}
        and a.get("head") == head and a.get("state") == "APPROVED"
        and a.get("eligible") is True for a in approvals
    ):
        errors.append("eligible non-author, non-pusher approval at head required")
    checks = data.get("checks")
    if not isinstance(checks, list):
        checks = []
    for name, workflow in CHECKS.items():
        candidates = [c for c in checks if isinstance(c, dict) and c.get("name") == name]
        if len(candidates) != 1:
            errors.append("missing or ambiguous check: " + name)
            continue
        c = candidates[0]
        expected = {"workflow": ".github/workflows/" + workflow,
                    "event": "pull_request", "head": head, "base": base,
                    "tested_sha": merge, "status": "completed",
                    "conclusion": "success", "app_id": ACTIONS_APP_ID}
        if any(c.get(k) != v for k, v in expected.items()):
            errors.append("unqualified check: " + name)
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    for key in ("head", "base", "merge"):
        parser.add_argument("--" + key, required=True)
    args = parser.parse_args()
    try:
        result = blockers(json.loads(args.snapshot.read_text(encoding="utf-8")),
                          args.head, args.base, args.merge)
    except (OSError, ValueError) as exc:
        result = ["unreadable snapshot: " + type(exc).__name__]
    print(json.dumps({"ready": not result, "blockers": result}, ensure_ascii=True))
    return 1 if result else 0


if __name__ == "__main__":
    raise SystemExit(main())
