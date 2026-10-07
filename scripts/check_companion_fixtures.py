#!/usr/bin/env python3
"""Validate firmware-generated v2 fixtures against canonical schemas.

Use jsonschema==4.23.0 in a project-independent environment. Native tests
separately compare bytes and exercise semantics; schemas do not authenticate.
"""
from pathlib import Path
import json
import sys

sys.dont_write_bytecode = True


def decode_document_frames(lines: list[str], expected_kind: int, maximum: int) -> bytes:
    """Independent bounded assembly; no identity/authorization/delivery proof."""
    import re
    import struct
    output = bytearray()
    message = total = 0
    if not lines:
        raise ValueError("empty document frame sequence")
    for index, line in enumerate(lines):
        if not re.fullmatch(r"[0-9A-F]{18,256}", line) or len(line) % 2:
            raise ValueError(f"invalid hex/outer length at frame {index}")
        value = bytes.fromhex(line)
        version, kind, identity, length, offset = struct.unpack("<BBHHH", value[:8])
        count = len(value) - 8
        if version != 1 or kind != expected_kind or not identity or not 1 <= length <= maximum:
            raise ValueError(f"unsupported/invalid document header at frame {index}")
        if count > 120 or offset >= length or count > length - offset:
            raise ValueError(f"invalid payload range at frame {index}")
        if not message:
            message, total = identity, length
        if identity != message or length != total or offset != len(output):
            raise ValueError(f"mixed, duplicated or out-of-order frame {index}")
        output.extend(value[8:])
    if len(output) != total:
        raise ValueError("incomplete framed document")
    return bytes(output)



def decode_reply_frames(lines: list[str]) -> bytes:
    return decode_document_frames(lines, 2, 511)


def decode_status_frames(lines: list[str]) -> bytes:
    return decode_document_frames(lines, 4, 6143)


def check_status_frames(base: Path) -> tuple[int, int, int]:
    """Actual native mailbox/encoder/sender fixtures; sizes are not RF rates."""
    frame_count = document_bytes = rejected = 0
    for name, frame_name, maximum_value in (
        ("status-ready", "status-ready-frames-v1", 20),
        ("status-running", "status-running128-frames-v1", 128),
    ):
        lines = (base / "fixtures" / (frame_name + ".txt")).read_text(encoding="utf-8").splitlines()
        expected = (base / "fixtures" / "v2" / (name + ".json")).read_bytes().rstrip(bytes((13, 10)))
        actual = decode_status_frames(lines)
        if actual != expected:
            raise ValueError("status frames differ from actual firmware snapshot: " + name)
        if any(len(line) // 2 > maximum_value for line in lines):
            raise ValueError("status fixture exceeds chosen outer value size")
        if len(lines) != (len(expected) + maximum_value - 9) // (maximum_value - 8):
            raise ValueError("status fixture fragment count differs from bound")
        negatives = [[], lines[:-1], [lines[0], *lines],
                     [lines[1], lines[0], *lines[2:]],
                     [*lines, lines[-1]], ["GG", *lines[1:]],
                     [lines[0] + "FF" * 121, *lines[1:]]]
        for offset, value in ((0, 2), (1, 1), (1, 2), (1, 3), (2, 0), (4, 0), (6, 1)):
            changed = bytearray.fromhex(lines[0])
            changed[offset] = value
            if offset in (2, 4):
                changed[offset + 1] = 0
            negatives.append([changed.hex().upper(), *lines[1:]])
        changed = bytearray.fromhex(lines[0])
        changed[4:6] = (6144).to_bytes(2, "little")
        negatives.append([changed.hex().upper(), *lines[1:]])
        changed = bytearray.fromhex(lines[1])
        changed[2:4] = (2).to_bytes(2, "little")
        negatives.append([lines[0], changed.hex().upper(), *lines[2:]])
        changed = bytearray.fromhex(lines[1])
        changed[4:6] = (len(expected) + 1).to_bytes(2, "little")
        negatives.append([lines[0], changed.hex().upper(), *lines[2:]])
        for bad in negatives:
            try:
                decode_status_frames(bad)
            except ValueError:
                rejected += 1
                continue
            raise ValueError("status counterpart accepted malformed fixture: " + name)
        frame_count += len(lines)
        document_bytes += len(actual)
    return frame_count, document_bytes, rejected


def check_reply_frames(base: Path) -> tuple[int, int]:
    lines = (base / "fixtures" / "reply-frames-v1.txt").read_text(encoding="utf-8").splitlines()
    expected = (base / "fixtures" / "v2" / "reply-executed.json").read_bytes().rstrip(bytes((13, 10)))
    actual = decode_reply_frames(lines)
    if actual != expected:
        raise ValueError("reply frames do not reconstruct the actual firmware receipt")
    # Tool failures must be explicit; none of these are complete client documents.
    negatives = [[], lines[:-1], [lines[0], *lines], [lines[1], lines[0], *lines[2:]]]
    for offset, byte in ((0, 2), (1, 1), (2, 0), (4, 0), (6, 1)):
        changed = bytearray.fromhex(lines[0])
        changed[offset] = byte
        if offset in (2, 4):
            changed[offset + 1] = 0
        negatives.append([changed.hex().upper(), *lines[1:]])
    negatives.extend([["GG", *lines[1:]], [lines[0] + "FF" * 121, *lines[1:]]])
    for bad in negatives:
        try:
            decode_reply_frames(bad)
        except ValueError:
            continue
        raise ValueError("reply counterpart accepted a malformed fixture mutation")
    return len(lines), len(actual)


def decode_command_receipt(line: str) -> tuple[int, ...]:
    import re
    import struct
    if not re.fullmatch(r"[0-9A-F]{36}", line):
        raise ValueError("invalid processing receipt hex/length")
    values = struct.unpack("<BBBBHHHHHHH", bytes.fromhex(line))
    version, kind, code, reserved, source, length, offset, count, active, total, received = values
    if version != 1 or kind != 3 or reserved or not 1 <= code <= 13:
        raise ValueError("invalid processing receipt header")
    if not source or not 1 <= length <= 1024 or not 1 <= count <= 120:
        raise ValueError("invalid processing receipt source")
    if offset >= length or count > length - offset:
        raise ValueError("invalid processing receipt source range")
    if (not active and (total or received)) or (active and (not 1 <= total <= 1024 or received > total)):
        raise ValueError("invalid processing receipt stream")
    same = active == source and total == length
    if code in (1, 2) and not (same and 0 < received < total):
        raise ValueError("invalid partial receipt")
    if code in (3, 4, 5, 13) and not (same and received == total):
        raise ValueError("invalid complete receipt")
    if code == 9 and (not active or active == source):
        raise ValueError("invalid busy receipt")
    if code == 11 and active != source:
        raise ValueError("invalid conflict receipt")
    if code == 10 and active and not same:
        raise ValueError("invalid out-of-order receipt")
    return values


def check_command_receipts(base: Path) -> tuple[int, int]:
    import struct
    lines = (base / "fixtures" / "command-receipts-v1.txt").read_text(encoding="utf-8").splitlines()
    commands = (base / "fixtures" / "command-frames-v1.txt").read_text(encoding="utf-8").splitlines()
    if len(lines) != len(commands) or not lines:
        raise ValueError("receipt/command fixture count mismatch")
    for index, (line, command) in enumerate(zip(lines, commands)):
        values = decode_command_receipt(line)
        raw = bytes.fromhex(command)
        version, kind, source, length, offset = struct.unpack("<BBHHH", raw[:8])
        count = len(raw) - 8
        expected_code = 3 if offset + count == length else 1
        if version != 1 or kind != 1 or values[2] != expected_code:
            raise ValueError("receipt outcome/command kind mismatch")
        if values[4:] != (source, length, offset, count, source, length, offset + count):
            raise ValueError(f"receipt source/progress mismatch at {index}")
    negatives = ["", lines[0][:-2], lines[0] + "00", "GG" + lines[0][2:]]
    for byte, number in ((0, 2), (1, 2), (2, 0), (2, 14), (3, 1),
                         (4, 0), (6, 0), (10, 0), (14, 0), (16, 255)):
        changed = bytearray.fromhex(lines[0])
        changed[byte] = number
        if byte in (4, 6, 10, 14, 16):
            changed[byte + 1] = 255 if byte == 16 else 0
        negatives.append(changed.hex().upper())
    complete = bytearray.fromhex(lines[-1])
    complete[16] -= 1
    negatives.append(complete.hex().upper())
    for bad in negatives:
        try:
            decode_command_receipt(bad)
        except ValueError:
            continue
        raise ValueError("receipt counterpart accepted malformed mutation")
    return len(lines), len(negatives)


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
    identity = json.loads((base / "fixtures" / "device-identity-v1.json").read_text(encoding="utf-8"))
    identity_schema = json.loads((base / "device-identity-v1.schema.json").read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(identity_schema)
    identity_validator = Draft202012Validator(identity_schema)
    identity_validator.validate(identity)
    identity_negatives = [dict(identity, deviceId="0" * 32), dict(identity, deviceId=identity["deviceId"].lower()),
                          dict(identity, requiresVerifiedConnection=False),
                          dict(identity, requiresGameMasterAssignment=False),
                          dict(identity, scope="authenticated_identity"), dict(identity, role="game_master"),
                          dict(identity, deviceId="01"), dict(identity, deviceId=identity["deviceId"] + "\n")]
    missing = dict(identity)
    del missing["requiresGameMasterAssignment"]
    identity_negatives.append(missing)
    if any(identity_validator.is_valid(bad) for bad in identity_negatives):
        print("Public identity counterpart accepted an invalid fixture", file=sys.stderr)
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
    try:
        frame_count, reply_bytes = check_reply_frames(base)
        receipt_count, receipt_negatives = check_command_receipts(base)
        status_count, status_bytes, status_negatives = check_status_frames(base)
    except ValueError as error:
        print(f"Frame counterpart: {error}", file=sys.stderr)
        return 1
    print(f"{len(names)} v2 fixtures + profile capabilities validate; vocabularies and {len(expected_limits)} limits agree.")
    print(f"Reply counterpart reconstructs {reply_bytes} exact bytes from {frame_count} frames; 11 malformed variants rejected.")
    print(f"Receipt counterpart correlates {receipt_count} actual command fragments; {receipt_negatives} malformed variants rejected.")

    print(f"Status counterpart reconstructs {status_bytes} exact bytes from {status_count} frames; {status_negatives} malformed variants rejected.")

    print(f"Public identity metadata validates; {len(identity_negatives)} malformed variants rejected. No authentication proof.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
