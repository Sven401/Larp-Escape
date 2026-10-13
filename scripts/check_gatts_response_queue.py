#!/usr/bin/env python3
"""Independent actual Host queue observation check. No ATT wire/RF claim."""
from copy import deepcopy
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
EXPECTED = {
    "scope": "host_gatts_response_queue", "depth": 4, "itemBytes": 44,
    "maxAgeMilliseconds": 1000, "frameCopies": 13, "responseCopies": 13,
    "workerPosts": 13, "callbackPosts": 0, "firstReceivedAt": 17,
    "lastReceivedAt": 41, "lastWorkerAt": 42, "runningBeforeOwner": False,
    "runningAfterOwner": True, "peerDeliveryMeasured": False, "rfInitialized": False,
}


def validate_observation(value):
    if not isinstance(value, dict) or value.keys() != EXPECTED.keys():
        raise ValueError("wrong response queue observation fields")
    if any(type(value[key]) is not type(expected) or value[key] != expected
           for key, expected in EXPECTED.items()):
        raise ValueError("wrong original time/copy/worker/application observation")


def check_gatts_response_queue(base: Path) -> int:
    fixture = base / "fixtures" / "gatts"
    observed = json.loads((fixture / "response-queue-observation.json").read_text(encoding="utf-8"))
    validate_observation(observed)
    lines = (fixture / "command-frames-v1.txt").read_text(encoding="utf-8").splitlines()
    if len(lines) != observed["frameCopies"]:
        raise ValueError("response count differs from actual command frame sequence")
    negatives = []
    for key, original in EXPECTED.items():
        bad = deepcopy(observed)
        bad[key] = not original if type(original) is bool else (
            original + 1 if type(original) is int else "physical_att_wire")
        negatives.append(bad)
    bad = deepcopy(observed)
    bad["peerDeliveryMeasured"] = 0
    negatives.append(bad)
    bad = deepcopy(observed)
    bad["currentHandleInferred"] = True
    negatives.append(bad)
    bad = deepcopy(observed)
    del bad["firstReceivedAt"]
    negatives.append(bad)
    for bad in negatives:
        try:
            validate_observation(bad)
        except ValueError:
            continue
        raise ValueError("counterpart accepted an altered response queue observation")
    return len(negatives)


if __name__ == "__main__":
    base = Path(__file__).resolve().parents[1] / "docs" / "companion"
    try:
        count = check_gatts_response_queue(base)
    except (OSError, ValueError, TypeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
    print(f"GATTS response queue counterpart: exact Host observation, {count} negatives rejected; RF unmeasured")
