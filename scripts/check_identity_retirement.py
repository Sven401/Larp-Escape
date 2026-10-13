#!/usr/bin/env python3
"""Independent producer-retirement/actual status counterpart, Host scope only."""
from copy import deepcopy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True

FIXED = {"scope": "host_identity_retirement", "closeOutcome": "authority_unavailable",
         "oldOwnerRetired": True, "newOwnerExecuted": True, "replyPrincipal": 99,
         "rfInitialized": False, "sdkOriginProven": False}
INTEGERS = {"oldConnectionToken", "oldAuthorityEpoch", "replacementEpoch", "currentEpoch",
            "replyEpoch", "statusEpoch", "lease"}


def validate(observed, status, validator):
    if not isinstance(observed, dict) or set(observed) != set(FIXED) | INTEGERS:
        raise ValueError("wrong retirement observation fields")
    if any(type(observed[key]) is not type(value) or observed[key] != value
           for key, value in FIXED.items()):
        raise ValueError("wrong retirement scope/outcome")
    if any(type(observed[key]) is not int or not 0 < observed[key] <= 0xffffffff
           for key in INTEGERS):
        raise ValueError("wrong bounded retirement counter")
    token, old, new = (observed[key] for key in
                       ("oldConnectionToken", "oldAuthorityEpoch", "replacementEpoch"))
    if not token < old < new or any(observed[key] != new for key in
                                   ("currentEpoch", "replyEpoch", "statusEpoch")):
        raise ValueError("old connection/role epoch or new producer correlation differs")
    validator.validate(status)
    if (status["game"]["running"] is not True or status["control"]["lease"] != observed["lease"] or
            status["control"]["nextRequest"] != 2 or status["application"]["phase"] != "ready" or
            status["application"]["storedMatchesActive"] is not True):
        raise ValueError("actual new-owner execution/status differs")


def check_identity_retirement(base, validator):
    fixture = base / "fixtures" / "gatts"
    observed = json.loads((fixture / "retirement-observation.json").read_text(encoding="utf-8"))
    status = json.loads((fixture / "retirement-status-v2.json").read_text(encoding="utf-8"))
    validate(observed, status, validator)
    negatives = []
    for key, original in FIXED.items():
        bad = deepcopy(observed)
        bad[key] = not original if type(original) is bool else (
            original + 1 if type(original) is int else "physical_sdk")
        negatives.append((bad, status))
    for key in INTEGERS:
        bad = deepcopy(observed)
        bad[key] = 0
        negatives.append((bad, status))
    for key, value in (("currentEpoch", observed["replacementEpoch"] + 1),
                       ("replyEpoch", observed["oldAuthorityEpoch"]),
                       ("statusEpoch", observed["oldAuthorityEpoch"]),
                       ("oldAuthorityEpoch", observed["oldConnectionToken"]),
                       ("replacementEpoch", observed["oldAuthorityEpoch"])):
        bad = deepcopy(observed); bad[key] = value
        negatives.append((bad, status))
    bad_status = deepcopy(status)
    bad_status["control"]["lease"] += 1
    negatives.append((observed, bad_status))
    for key, value in (("running", False),):
        bad_status = deepcopy(status)
        bad_status["game"][key] = value
        negatives.append((observed, bad_status))
    bad = deepcopy(observed); bad["oldOwnerRetired"] = 1
    negatives.append((bad, status))
    bad = deepcopy(observed); bad["extra"] = True
    negatives.append((bad, status))
    for bad, bad_status in negatives:
        try:
            validate(bad, bad_status, validator)
        except ValueError:
            continue
        raise ValueError("counterpart accepted altered retirement/status correlation")
    return len(negatives)


if __name__ == "__main__":
    from jsonschema import Draft202012Validator
    base = Path(__file__).resolve().parents[1] / "docs" / "companion"
    validator = Draft202012Validator(json.loads((base / "status-v2.schema.json").read_text(encoding="utf-8")))
    print(f"Retirement counterpart: {check_identity_retirement(base, validator)} wrong observations/statuses rejected; Host only")
