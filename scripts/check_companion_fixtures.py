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
    print(f"{len(names)} firmware fixtures validate against canonical JSON schemas.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
