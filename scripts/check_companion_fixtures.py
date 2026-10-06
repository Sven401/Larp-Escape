#!/usr/bin/env python3
"""Validate firmware-generated v2 fixtures against canonical schemas.

Use jsonschema==4.23.0 in a project-independent environment. Native tests
separately compare bytes and exercise semantics; schemas do not authenticate.
"""
from pathlib import Path
import json
import sys

sys.dont_write_bytecode = True


def main() -> int:
    try:
        from jsonschema import Draft202012Validator
    except ImportError:
        print("Requires jsonschema==4.23.0 in this Python environment.", file=sys.stderr)
        return 2
    root = Path(__file__).resolve().parents[1]
    base = root / "docs" / "companion"
    names = {
        "status-ready": "status-v2.schema.json",
        "status-running": "status-v2.schema.json",
        "command-start": "game-command-v2.schema.json",
        "command-stale-state": "game-command-v2.schema.json",
        "reply-executed": "game-reply-v2.schema.json",
        "reply-duplicate": "game-reply-v2.schema.json",
        "reply-conflict": "game-reply-v2.schema.json",
        "reply-stale-state": "game-reply-v2.schema.json",
    }
    expected = {name + ".json" for name in names}
    actual = {path.name for path in (base / "fixtures" / "v2").glob("*.json")}
    if expected != actual:
        print(f"Fixture set mismatch: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}", file=sys.stderr)
        return 1
    validators = {}
    for name, schema_name in names.items():
        if schema_name not in validators:
            schema = json.loads((base / schema_name).read_text(encoding="utf-8"))
            Draft202012Validator.check_schema(schema)
            validators[schema_name] = Draft202012Validator(schema)
        value = json.loads((base / "fixtures" / "v2" / (name + ".json")).read_text(encoding="utf-8"))
        errors = sorted(validators[schema_name].iter_errors(value), key=lambda error: str(error.path))
        if errors:
            for error in errors:
                print(f"{name}: {list(error.path)}: {error.message}", file=sys.stderr)
            return 1
    capabilities = json.loads((base / "fixtures" / "profile-capabilities-v1.json").read_text(encoding="utf-8"))
    schema = json.loads((base / "profile-capabilities-v1.schema.json").read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)
    errors = list(Draft202012Validator(schema).iter_errors(capabilities))
    if errors:
        for error in errors:
            print(f"profile-capabilities-v1: {list(error.path)}: {error.message}", file=sys.stderr)
        return 1
    profile = json.loads((root / "docs" / "configuration" / "profile-v1.schema.json").read_text(encoding="utf-8"))
    from generate_profile_schema import generate
    Draft202012Validator.check_schema(profile)
    if profile != generate(root):
        print("Generated profile schema differs from canonical source generator", file=sys.stderr)
        return 1
    definitions, properties = profile["$defs"], profile["properties"]
    expectations = {
        "boards": [properties["board"]["const"]],
        "componentKinds": definitions["component"]["properties"]["kind"]["enum"],
        "crystalCategories": definitions["crystal"]["properties"]["category"]["enum"],
        "fields": definitions["condition"]["oneOf"][0]["properties"]["field"]["enum"],
        "comparisons": definitions["condition"]["oneOf"][0]["properties"]["comparison"]["enum"],
        "actionKinds": definitions["action"]["properties"]["kind"]["enum"],
        "effectKinds": definitions["effect"]["properties"]["kind"]["enum"],
        "gameCommands": validators["game-command-v2.schema.json"].schema["properties"]["kind"]["enum"],
    }
    kinds = []
    for branch in definitions["condition"]["oneOf"]:
        kind = branch["properties"]["kind"]
        kinds.extend(kind.get("enum", [kind.get("const")]))
    expectations["conditionKinds"] = kinds
    for name, expected in expectations.items():
        if set(capabilities[name]) != set(expected):
            print(f"Capabilities/schema vocabulary mismatch: {name}", file=sys.stderr)
            return 1
    expected_limits = {name: properties[name]["maxItems"] for name in
                       ("components", "states", "transitions", "conditions", "actions", "effects", "roles")}
    expected_limits.update({
        "crystalsPerRole": definitions["role"]["properties"]["crystals"]["maxItems"],
        "stateActions": definitions["state"]["properties"]["entry"]["maxItems"],
        "transitionActions": definitions["transition"]["properties"]["actions"]["maxItems"],
        "matrixLines": definitions["component"]["properties"]["rows"]["maxItems"],
        "wakeInputs": definitions["component"]["properties"]["wakeInputs"]["maxItems"],
        "nameUtf8Bytes": properties["name"]["x-maxUtf8Bytes"],
        "paletteEntries": definitions["effect"]["properties"]["palette"]["maxItems"],
        "roleUidBytes": definitions["role"]["properties"]["uid"]["maxItems"],
        "componentPinSlots": definitions["component"]["properties"]["pins"]["maxItems"],
        "expanderPinModes": definitions["component"]["properties"]["startupModes"]["maxItems"],
        "peerAddressBytes": definitions["component"]["properties"]["peer"]["maxItems"],
        # Semantic limits are annotations, not bounds on inactive/irrelevant fields.
        "leds": profile["x-maxEnabledLeds"],
        "conditionDepth": profile["x-maxConditionDepth"],
        "profileDocumentBytes": profile["x-maxDocumentBytes"],
        "profileContainerDepth": profile["x-maxContainerDepth"],
    })
    for name, expected in expected_limits.items():
        if capabilities["limits"][name] != expected:
            print(f"Capabilities/schema limit mismatch: {name}", file=sys.stderr)
            return 1
    if capabilities["profileSchemaVersion"] != properties["schemaVersion"]["const"]:
        print("Capabilities/profile schema version mismatch", file=sys.stderr)
        return 1
    print(f"{len(names)} v2 fixtures + profile capabilities validate; vocabularies and {len(expected_limits)} limits agree.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
