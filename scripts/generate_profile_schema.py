#!/usr/bin/env python3
"""Generate the profile-v1 structural schema from the reviewed model vocabulary.

Numeric capacities and wire names are read from the C++ profile headers.
Cross references, GPIO ownership,
condition cycles/depth and enabled-device contracts remain firmware validation.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def generate(root: Path) -> dict:
    header = (root / "include/GameConfiguration/Profile.h").read_text(encoding="utf-8-sig")
    limits = {name: int(value) for name, value in re.findall(
        r"static const std::(?:size_t|uint16_t)\s+(\w+)\s*=\s*(\d+)\s*;", header)}
    vocabulary_header = (root / "include/GameConfiguration/ProfileVocabulary.h").read_text(encoding="utf-8-sig")
    vocabulary = {kind: re.findall(r'"([^"\\]+)"', values) for kind, values in re.findall(
        r"static const char \*const (\w+)Names\[\]\s*=\s*\{([^}]+)\};", vocabulary_header, re.DOTALL)}
    import_header = (root / "include/GameConfiguration/ProfileReader.h").read_text(encoding="utf-8-sig")
    import_limits = {name: int(value) for name, value in re.findall(
        r"static const std::size_t\s+(\w+)\s*=\s*(\d+)\s*;", import_header)}
    def integer(maximum=4294967295, minimum=0):
        return {"type": "integer", "minimum": minimum, "maximum": maximum}
    def sequence(items, maximum, minimum=0):
        return {"type": "array", "items": items, "minItems": minimum, "maxItems": maximum}
    def obj(properties):
        return {"type": "object", "additionalProperties": False,
                "required": list(properties), "properties": properties}
    def ref(name):
        return {"$ref": "#/$defs/" + name}
    def enum(*values):
        return {"type": "string", "enum": list(values)}
    flag = {"type": "boolean"}
    identifier = integer(65535, 1)
    optional_id = integer(65535)
    name = {"type": "string", "minLength": 1, "maxLength": 32,
            "x-maxUtf8Bytes": 32,
            "description": "Firmware also requires <=32 UTF-8 bytes and rejects ASCII C0 control characters."}
    palette = sequence(integer(0xFFFFFF), 16, 16)
    endpoint = obj({"expander": identifier, "pin": integer(15)})
    definitions = {
        "component": obj({
            "id": identifier, "name": name,
            "kind": enum(*vocabulary["ComponentKind"]),
            "enabled": flag, "pins": sequence(integer(255), 4, 4), "bus": integer(255),
            "address": integer(255), "ledCount": integer(65535),
            "powerLimitMilliamperes": integer(65535), "volume": integer(255), "baud": integer(),
            "initialBrightness": integer(255), "initialTwinkleDensity": integer(255),
            "initialTwinkleSpeed": integer(255), "startupModes": sequence(integer(3), 16, 16),
            "peer": sequence(integer(255), 6, 6), "channel": integer(255),
            "rows": sequence(endpoint, limits["MatrixLines"]),
            "columns": sequence(endpoint, limits["MatrixLines"]),
            "wakeInputs": sequence(endpoint, limits["WakeInputs"])}),
        "crystal": obj({"name": name, "row": integer(9, 1), "column": integer(8),
            "audioTrack": integer(65535, 1), "category": enum(*vocabulary["CrystalCategory"]),
            "changesPalette": flag, "palette": palette}),
        "role": obj({"id": identifier, "name": name, "uid": sequence(integer(255), 7, 7),
            "audioTrack": integer(65535, 1), "crystals": sequence(ref("crystal"), limits["CrystalsPerRole"], 1)}),
        "state": obj({"id": identifier, "name": name, "manualAllowed": flag,
            "entry": sequence(identifier, limits["StateActions"]),
            "tick": sequence(identifier, limits["StateActions"]),
            "exit": sequence(identifier, limits["StateActions"])}),
        "transition": obj({"id": identifier, "source": identifier, "target": identifier,
            "condition": identifier, "priority": integer(65535), "manualAllowed": flag,
            "actions": sequence(identifier, limits["TransitionActions"])}),
        "condition": {"oneOf": [
            obj({"id": identifier, "kind": {"const": vocabulary["ConditionKind"][0]}, "field": enum(*vocabulary["Field"]),
                "component": optional_id, "comparison": enum(*vocabulary["Compare"]),
                "operand": integer()}),
            obj({"id": identifier, "kind": enum(*vocabulary["ConditionKind"][1:3]), "first": identifier, "second": identifier}),
            obj({"id": identifier, "kind": {"const": vocabulary["ConditionKind"][3]}, "first": identifier}),
            obj({"id": identifier, "kind": {"const": vocabulary["ConditionKind"][4]},
                 "operand": integer(0x7FFFFFFF, 1)})]},
        "action": obj({"id": identifier, "kind": enum(*vocabulary["ActionKind"]), "component": optional_id, "source": optional_id,
            "effect": optional_id, "value": integer()}),
        "effect": obj({"id": identifier, "name": name, "component": identifier,
            "kind": enum(*vocabulary["EffectKind"]),
            "density": integer(8), "changesSpeed": flag, "speed": integer(8), "brightness": integer(255),
            "intervalMilliseconds": integer(65535), "frames": integer(255), "colored": flag,
            "colorRgb": integer(0xFFFFFF), "rippleIndex": integer(255), "origin": integer(65535), "palette": palette})}
    document = obj({"schemaVersion": {"const": limits["SchemaVersion"]}, "revision": integer(),
        "name": name, "board": {"const": vocabulary["Board"][0]}, "initialState": identifier, "errorState": identifier,
        "inputs": obj({"cardTimeoutMilliseconds": integer(65535, 1), "emptyReadsBeforeClear": integer(255, 1),
            "keystoneFailureThreshold": integer(255, 1), "consecutiveEmptyReads": flag,
            "wakeRippleOrigins": sequence(integer(65535), limits["WakeInputs"], limits["WakeInputs"]),
            "wakeRippleFrames": integer(255)}),
        "components": sequence(ref("component"), limits["Components"]),
        "roles": sequence(ref("role"), limits["Roles"]),
        "states": sequence(ref("state"), limits["States"], 1),
        "transitions": sequence(ref("transition"), limits["Transitions"]),
        "conditions": sequence(ref("condition"), limits["Conditions"]),
        "actions": sequence(ref("action"), limits["Actions"]),
        "effects": sequence(ref("effect"), limits["Effects"])})
    document.update({"$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Larp-Escape configuration profile v1", "$defs": definitions,
        "x-maxDocumentBytes": import_limits["DocumentBytes"],
        "x-maxContainerDepth": import_limits["ContainerDepth"],
        "description": "Structural contract; semantic validation by the firmware is required before activation."})
    return document

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or args.root / "docs/configuration/profile-v1.schema.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(generate(args.root), indent=2) + "\n", encoding="utf-8")
    print(output)
