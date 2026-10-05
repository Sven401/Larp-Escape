#!/usr/bin/env python3
"""Generate the profile-v1 structural schema from the reviewed model vocabulary.

Numeric capacities are read from Profile.h. Cross references, GPIO ownership,
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
            "description": "Firmware also requires <=32 UTF-8 bytes and rejects control characters."}
    palette = sequence(integer(0xFFFFFF), 16, 16)
    endpoint = obj({"expander": identifier, "pin": integer(15)})
    definitions = {
        "component": obj({
            "id": identifier, "name": name,
            "kind": enum("nfc_spi", "io_expander", "matrix", "audio_uart", "led_strip", "esp_now"),
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
            "audioTrack": integer(65535, 1), "category": enum("animal", "item", "color"),
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
            obj({"id": identifier, "kind": {"const": "predicate"}, "field": enum(
                "audio_busy", "role_accepted", "crystal_available", "crystal_audio_pending",
                "puzzle_complete", "incorrect_crystal", "keystone_error", "all_wake_active",
                "seen_role_count", "state_elapsed_ms", "transition_elapsed_ms", "round_count"),
                "component": optional_id, "comparison": enum("eq", "ne", "lt", "le", "gt", "ge"),
                "operand": integer()}),
            obj({"id": identifier, "kind": enum("all", "any"), "first": identifier, "second": identifier}),
            obj({"id": identifier, "kind": {"const": "not"}, "first": identifier})]},
        "action": obj({"id": identifier, "kind": enum("apply_effect", "play_track", "play_role_track",
            "play_crystal_track", "wake_audio", "reset_round", "reset_game", "reset_card_retry", "complete_round",
            "send_activation", "consume_crystal", "consume_error", "read_role", "read_crystals",
            "observe_keystone", "animate_wake_inputs"), "component": optional_id, "source": optional_id,
            "effect": optional_id, "value": integer()}),
        "effect": obj({"id": identifier, "name": name, "component": identifier,
            "kind": enum("twinkle", "burst", "ripple", "brightness_ramp", "palette"),
            "density": integer(8), "changesSpeed": flag, "speed": integer(8), "brightness": integer(255),
            "intervalMilliseconds": integer(65535), "frames": integer(255), "colored": flag,
            "colorRgb": integer(0xFFFFFF), "rippleIndex": integer(255), "origin": integer(65535), "palette": palette})}
    document = obj({"schemaVersion": {"const": limits["SchemaVersion"]}, "revision": integer(),
        "name": name, "board": {"const": "esp32_devkit_v4"}, "initialState": identifier, "errorState": identifier,
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
