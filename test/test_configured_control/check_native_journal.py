"""Check real host journal creation/reopen and a missing-directory failure."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("file_owner", root / "scripts/check_application_file_owner.py")
owner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owner)
tool = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="larp-native-journal-") as folder:
    base = Path(folder)
    journal = base / "journal"
    journal.mkdir()
    result = subprocess.run([str(tool), "--file-owner-probe", str(journal)],
                            check=True, capture_output=True, text=True, timeout=30)
    owner.accept_observation(json.loads(result.stdout), "actual_host_file", "host_file")
    if not list(journal.glob("profile-?.larp")):
        raise AssertionError("No actual journal slot was saved")
    exported = base / "restored.json"
    subprocess.run([str(tool), "--restore-journal", str(journal), "--active-export", str(exported)],
                   check=True, capture_output=True, text=True, timeout=30)
    json.loads(exported.read_text(encoding="utf-8"))
    missing = base / "missing"
    failed = subprocess.run([str(tool), "--restore-journal", str(missing)],
                            capture_output=True, text=True, timeout=30)
    if failed.returncode == 0 or missing.exists():
        raise AssertionError("Missing journal directory must fail without creating storage")
print("PASS: actual host journal create/reopen/export and missing-directory rejection")
