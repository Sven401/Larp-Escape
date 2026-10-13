"""Independent counterpart for application-owned temporary-file observations."""
from pathlib import Path
import json


def accept_observation(value, source="esp_adapter_host_vfs", storage="host_model"):
    expected = {"scope": "host_application_file_owner", "temporarySource": source,
                "journalStorage": storage, "blockedBeforeCleanup": True,
                "failedCleanupBlocked": True, "abandonBlocked": True,
                "newBeginBeforeCleanup": False, "cleanedStart": True, "runningIoCalls": 0,
                "profileImportExecuted": False, "radioMeasured": False}
    if value != expected or any(type(value[key]) is not type(wanted) for key, wanted in expected.items()):
        raise ValueError("application file ownership/quarantine/model scope")


def check_application_file_owner(base: Path) -> int:
    fixture = base / "fixtures" / "profile-source" / "application-file-owner-observation.json"
    observed = json.loads(fixture.read_text(encoding="utf-8"))
    accept_observation(observed)
    negatives = []
    for key, value in (("scope", "device"), ("temporarySource", "flash"),
                       ("journalStorage", "physical_flash"), ("blockedBeforeCleanup", False),
                       ("failedCleanupBlocked", False), ("abandonBlocked", False),
                       ("newBeginBeforeCleanup", True), ("cleanedStart", False),
                       ("runningIoCalls", 1), ("profileImportExecuted", True), ("radioMeasured", True),
                       ("runningIoCalls", False), ("cleanedStart", 1)):
        changed = dict(observed); changed[key] = value; negatives.append(changed)
    for changed in negatives:
        try:
            accept_observation(changed)
        except ValueError:
            continue
        raise ValueError("accepted a wrong application file ownership observation")
    return len(negatives)
