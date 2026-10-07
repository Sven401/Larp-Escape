#!/usr/bin/env python3
"""Independent original callback/reconnection counterpart; no SDK/RF proof."""
from copy import deepcopy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True

FIXED = {"scope": "host_callback_lifecycle", "frameCopies": 13,
         "afterStartNextRequest": 2, "staleOutputDiscarded": True,
         "oldRetirementIgnored": True, "sameSdkHandle": True, "running": True,
         "sdkOrderingProven": False, "rfInitialized": False}
COUNTERS = {"originalToken", "authorityEpoch", "replacementToken", "replacementEpoch", "statusEpoch"}


def validate(observation, status, validator):
    from jsonschema.exceptions import ValidationError
    if not isinstance(observation, dict) or set(observation) != set(FIXED) | COUNTERS:
        raise ValueError("wrong callback lifecycle fields")
    if any(type(observation[key]) is not type(value) or observation[key] != value
           for key, value in FIXED.items()):
        raise ValueError("wrong callback lifecycle scope/outcome")
    if any(type(observation[key]) is not int or not 0 < observation[key] <= 0xffffffff
           for key in COUNTERS):
        raise ValueError("wrong callback lifecycle counter")
    values = [observation[key] for key in
              ("originalToken", "authorityEpoch", "replacementToken", "replacementEpoch")]
    if not all(a < b for a, b in zip(values, values[1:])) or observation["statusEpoch"] != values[-1]:
        raise ValueError("old connection/role and fresh connection/role correlation differs")
    try:
        validator.validate(status)
    except ValidationError as error:
        raise ValueError("invalid current lifecycle status") from error
    if (status["game"]["running"] is not True or status["control"]["lease"] != 3 or
            status["control"]["nextRequest"] != 1 or status["application"]["phase"] != "ready" or
            status["application"]["storedMatchesActive"] is not True):
        raise ValueError("fresh-session/current Running status differs")


def check_gatts_lifecycle(base, validator):
    directory = base / "fixtures" / "gatts"
    observation = json.loads((directory / "lifecycle-observation.json").read_text(encoding="utf-8"))
    status = json.loads((directory / "lifecycle-status-v2.json").read_text(encoding="utf-8"))
    validate(observation, status, validator)
    negatives = []
    for key, value in FIXED.items():
        bad = deepcopy(observation)
        bad[key] = not value if type(value) is bool else value + 1 if type(value) is int else "sdk_proven"
        negatives.append((bad, status))
    for key in COUNTERS:
        bad = deepcopy(observation); bad[key] = 0
        negatives.append((bad, status))
    for key, value in (("authorityEpoch", observation["originalToken"]),
                       ("replacementToken", observation["authorityEpoch"]),
                       ("replacementEpoch", observation["replacementToken"]),
                       ("statusEpoch", observation["authorityEpoch"]), ("frameCopies", True)):
        bad = deepcopy(observation); bad[key] = value
        negatives.append((bad, status))
    bad = deepcopy(observation); bad["extra"] = True
    negatives.append((bad, status))
    for key, value in (("lease", 2), ("nextRequest", 2)):
        bad = deepcopy(status); bad["control"][key] = value
        negatives.append((observation, bad))
    bad = deepcopy(status); bad["game"]["running"] = False
    negatives.append((observation, bad))
    for bad, bad_status in negatives:
        try:
            validate(bad, bad_status, validator)
        except ValueError:
            continue
        raise ValueError("counterpart accepted altered original lifecycle/status")
    return len(negatives)


if __name__ == "__main__":
    from jsonschema import Draft202012Validator
    base = Path(__file__).resolve().parents[1] / "docs" / "companion"
    schema = json.loads((base / "status-v2.schema.json").read_text(encoding="utf-8"))
    print(f"Callback lifecycle: {check_gatts_lifecycle(base, Draft202012Validator(schema))} negatives rejected; Host only")
