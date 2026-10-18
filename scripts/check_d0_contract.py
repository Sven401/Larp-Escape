"""Executable D0 decision examples, NOT a codec, verifier or firmware adapter.

Inputs are decoded projections and simulated trusted observations. No fixture
can establish an actual signature, public-key enrollment or durable commit.
Run: python scripts/check_d0_contract.py
"""
from copy import deepcopy
import json
from pathlib import Path
import re


MAX_SEQUENCE = (1 << 64) - 1


def integer(value, minimum=1, maximum=MAX_SEQUENCE):
    return type(value) is int and minimum <= value <= maximum


def identifier(value, size=16):
    return (isinstance(value, str) and
            re.fullmatch(r"[0-9A-F]{%d}" % (size * 2), value) is not None and
            (size != 16 or value != "0" * 32))


def context_gate(message, context):
    if not context["verified"]:
        return "reject_signature"
    if any(message[k] != context[k] for k in ("event", "run", "packageDigest")):
        return "reject_context"
    if not context["authorized"] or message["origin"] != context["origin"]:
        return "reject_trust"
    if any(message[k] != context[k] for k in
           ("station", "generation", "bindingDigest")):
        return "reject_binding"
    return None


def evaluate(model, message, context):
    """Pure admission oracle for the checked subset of D0; no side effects."""
    if model == "allocate":
        if (not context["storageCertain"] or
                not integer(context["storedEnd"], 0) or
                not integer(context["blockSize"]) or
                context["storedEnd"] > MAX_SEQUENCE - context["blockSize"]):
            return "blocked"
        return [context["storedEnd"] + 1,
                context["storedEnd"] + context["blockSize"]]
    if model == "prefix":
        # Only verified accepted facts and verified unused-range closures here.
        present = set(context["present"])
        intervals = [[n, n] for n in present]
        previous_end = 0
        for first, last in context["closedUnused"]:
            if (not integer(first) or not integer(last) or last < first or
                    first <= previous_end or any(first <= n <= last for n in present)):
                return "conflict"
            intervals.append([first, last])
            previous_end = last
        prefix = 0
        for first, last in sorted(intervals):
            if first > prefix + 1:
                break
            prefix = max(prefix, last)
        return prefix
    if model == "migration":
        if not context["capable"]:
            return "unsupported"
        if not context["complete"] or not context["hashesMatch"]:
            return "reject_package"
        if not context["authorized"] or not context["confirmed"]:
            return "pending_confirmation"
        if not context["stopped"] or not context["expectedRevision"]:
            return "reject_state"
        return "ready_for_atomic_activation"

    required = {"version", "kind", "event", "run", "origin", "station",
                "generation", "bindingDigest", "packageDigest"}
    specific = {
        "fact": {"epoch", "sequence", "factType", "subject", "value",
                 "dependencies", "unusedRanges", "contentDigest"},
        "observation": {"boot", "observationSequence", "state", "challenge"},
        "command": {"commandId", "targetBox", "targetBoot", "grant",
                    "targetRevision", "action", "resource", "contentDigest"},
        "receipt": {"targetBox", "receiptId", "stage", "reason", "contentDigest"},
    }
    if (model not in specific or set(message) != required | specific[model] or
            type(message["version"]) is not int or message["version"] != 1 or
            message["kind"] != model or
            not all(identifier(message[k]) for k in ("event", "run", "origin", "station")) or
            not integer(message["generation"]) or
            not all(identifier(message[k], 32) for k in ("bindingDigest", "packageDigest"))):
        return "reject_format"
    gate = context_gate(message, context)
    if gate:
        return gate
    if model == "fact":
        if (not identifier(message["epoch"]) or not integer(message["sequence"]) or
                type(message["factType"]) is not int or message["factType"] not in (1, 2, 3) or
                not identifier(message["subject"]) or message["value"] is not True or
                not identifier(message["contentDigest"], 32) or
                type(message["dependencies"]) is not list or
                len(message["dependencies"]) > 8 or
                len({json.dumps(d, sort_keys=True) for d in message["dependencies"]}) != len(message["dependencies"]) or
                type(message["unusedRanges"]) is not list):
            return "reject_format"
        if message["epoch"] != context["epoch"]:
            return "reject_epoch"
        # Projection dependencies are full validated reference tokens supplied
        # by the hypothetical decoder; this is not a FactRef byte decoder.
        if context["selfDependency"]:
            return "reject_dependency"
        if message["factType"] == 3:
            ranges = message["unusedRanges"]
            if (message["subject"] != message["origin"] or message["dependencies"] or
                    not 1 <= len(ranges) <= 8):
                return "reject_format"
            end = 0
            for pair in ranges:
                if (not isinstance(pair, list) or len(pair) != 2 or
                        not all(integer(x) for x in pair) or
                        not end < pair[0] <= pair[1] < message["sequence"]):
                    return "reject_format"
                end = pair[1]
            if context["rangeOverlapsFact"]:
                return "conflict"
        elif message["unusedRanges"]:
            return "reject_format"
        known = context["knownDigest"]
        if known is not None:
            return "duplicate" if known == message["contentDigest"] else "conflict"
        if message["factType"] == 3:
            return "record_unused" if context["capacity"] else "reject_capacity"
        if not context["dependenciesPresent"] or (context["orderedRule"] and context["gap"]):
            return "deferred"
        if not context["capacity"]:
            return "reject_capacity"
        return "already_released" if context["released"] else "apply_once"
    if model == "observation":
        if (not identifier(message["boot"]) or not integer(message["observationSequence"]) or
                type(message["state"]) is not int or not 0 <= message["state"] <= 5 or
                (message["challenge"] is not None and not identifier(message["challenge"]))):
            return "reject_format"
        if message["boot"] == context["latestBoot"] and message["observationSequence"] <= context["lastSequence"]:
            return "stale_observation"
        return ("fresh_observation" if message["challenge"] == context["pendingChallenge"] and
                identifier(message["challenge"]) and context["challengeLive"] else
                "observation_age_unknown")
    if model == "command":
        if (not all(identifier(message[k]) for k in ("commandId", "targetBox", "targetBoot", "grant")) or
                not integer(message["targetRevision"], 0) or
                type(message["action"]) is not int or message["action"] not in (1, 2) or
                not integer(message["resource"], 1, 65535) or
                not identifier(message["contentDigest"], 32)):
            return "reject_format"
        if any(message[k] != context[k] for k in ("targetBox", "targetBoot", "grant")):
            return "reject_grant"
        if not 0 <= context["grantAgeMs"] < context["grantLifetimeMs"] <= 5000:
            return "expired"
        if message["targetRevision"] != context["targetRevision"] or not context["stopped"]:
            return "reject_state"
        if context["spentCommand"] is not None:
            return ("duplicate" if context["spentCommand"] == message["commandId"] and
                    context["spentDigest"] == message["contentDigest"] else "grant_conflict")
        return "consume_then_execute"
    if (not identifier(message["targetBox"]) or not identifier(message["receiptId"]) or
            not integer(message["stage"], 1, 7) or not integer(message["reason"], 0, 7) or
            not identifier(message["contentDigest"], 32)):
        return "reject_format"
    if (message["origin"] != context["expectedReceiver"] or
            message["targetBox"] != context["targetBox"] or
            message["contentDigest"] != context["expectedDigest"]):
        return "reject_receipt_binding"
    return "not_applied" if message["stage"] < 3 else "record_stage"


def main():
    root = Path(__file__).resolve().parents[1]
    path = root / "docs/veranstaltung/fixtures/d0-v1.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    assert data["scope"] == "semantic_model_not_crypto_or_storage"
    ids = set()
    for case in data["cases"]:
        assert case["id"] not in ids, case["id"]
        ids.add(case["id"])
        template = data["templates"][case["model"]]
        message = deepcopy(template["message"])
        message.update(case.get("message", {}))
        context = deepcopy(template["context"])
        context.update(case.get("context", {}))
        actual = evaluate(case["model"], message, context)
        if actual != case["expected"]:
            raise AssertionError(f'{case["id"]}: expected {case["expected"]!r}, got {actual!r}')
    print(f"D0: {len(ids)} semantic contract cases passed; no crypto/storage/device proof")


if __name__ == "__main__":
    main()
