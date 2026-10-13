"""Independent service-definition/Host-observation counterpart; no RF proof."""
from copy import deepcopy
import json
from pathlib import Path
import re
import sys
from uuid import UUID
sys.dont_write_bytecode = True

DEFINITION = {
    "version": 1,
    "service": "407e6a1d-1cc2-43fd-856b-4aff85a20001",
    "command": "407e6a1d-1cc2-43fd-856b-4aff85a20002",
    "profile": "407e6a1d-1cc2-43fd-856b-4aff85a20003",
    "application": 19525, "handles": 5, "writeValueMaximum": 128,
    "writeResponse": "application", "writePermissions": "encrypted_mitm",
    "setupDeadlineMilliseconds": 30000, "endpointDeadlineMilliseconds": 5000,
}
OBSERVATION = {
    "scope": "host_service_sdk_model", "sdkRequests": 4, "sourcePosts": 14,
    "service": 32, "command": 34, "profile": 36, "ready": True,
    "frameCopies": 13, "running": True, "rfInitialized": False, "deviceMeasured": False,
}

def validate(value, expected):
    if not isinstance(value, dict) or set(value) != set(expected):
        raise ValueError("service counterpart fields differ")
    for key, wanted in expected.items():
        if type(value[key]) is not type(wanted) or value[key] != wanted:
            raise ValueError("service counterpart correlation differs: " + key)

def negatives(actual, expected):
    bad_values = []
    for key, value in expected.items():
        bad = deepcopy(actual)
        bad[key] = not value if type(value) is bool else value+1 if type(value) is int else "unverified"
        bad_values.append(bad)
        if type(value) is int:
            bad = deepcopy(actual); bad[key] = True; bad_values.append(bad)
    for key in expected:
        bad = deepcopy(actual); del bad[key]; bad_values.append(bad)
    bad = deepcopy(actual); bad["peerDelivered"] = True; bad_values.append(bad)
    for bad in bad_values:
        try: validate(bad, expected)
        except ValueError: continue
        raise ValueError("service counterpart accepted a wrong observation/definition")
    return len(bad_values)

def check_gatts_service(base):
    root = base / "fixtures/gatts"
    definition = json.loads((root / "service-definition.json").read_text(encoding="utf-8"))
    observation = json.loads((root / "service-setup-observation.json").read_text(encoding="utf-8"))
    validate(definition, DEFINITION)
    validate(observation, OBSERVATION)
    header = (base.parent.parent / "lib/GattsWriteAdmission/GattsServiceDefinition.h").read_text(encoding="utf-8")
    for name in ("service", "command", "profile"):
        match = re.search(r"\b" + name + r"\(\).*?return\s+\{\{(.*?)\}\};", header, re.S)
        if not match:
            raise ValueError("service UUID header definition missing: " + name)
        value = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})\b", match.group(1)))
        if value != UUID(definition[name]).bytes[::-1]:
            raise ValueError("SDK UUID byte order differs from canonical UUID: " + name)
    for pattern in (r"Application\s*=\s*0x4c45", r"Handles\s*=\s*5",
                    r"SetupDeadlineMilliseconds\s*=\s*30000", r"EndpointDeadlineMilliseconds\s*=\s*5000"):
        if not re.search(pattern, header):
            raise ValueError("service setup header/contract bound differs")
    frames = (root / "command-frames-v1.txt").read_text(encoding="utf-8").splitlines()
    if len(frames) != observation["frameCopies"] or observation["sourcePosts"] != len(frames)+1:
        raise ValueError("service observation differs from original command/endpoint posts")
    return negatives(definition, DEFINITION) + negatives(observation, OBSERVATION)

if __name__ == "__main__":
    print(f"Service setup counterpart: {check_gatts_service(Path(__file__).resolve().parents[1]/'docs/companion')} negatives rejected; Host only")
