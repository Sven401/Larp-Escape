"""Independent kind7/8 byte contract; model observations are not RF/auth proof."""
import struct
from pathlib import Path

def command(value: bytes) -> tuple[int, int, int]:
    if len(value) != 12:
        raise ValueError("control length")
    version, kind, transfer, op, reserved, request = struct.unpack("<BBHB3sI", value)
    if version != 1 or kind != 7 or not transfer or not request or op not in (1, 2, 3) or reserved != bytes(3):
        raise ValueError("control header")
    return transfer, request, op

def response(value: bytes) -> tuple[int, int, int, int, int, int, int]:
    if len(value) != 20:
        raise ValueError("response length")
    version, kind, transfer, request, op, code, detail, draft, cfg = struct.unpack("<BBHIBBHII", value)
    if version != 1 or kind != 8 or not transfer or not request or op not in (1, 2, 3) or not 1 <= code <= 13 or detail > 11:
        raise ValueError("response header")
    if code <= 4 and detail != 0 or code <= 3 and (not draft or not cfg):
        raise ValueError("response revision/detail")
    if code == 1 and op != 1 or code in (2, 3, 13) and op != 3 or code == 4 and op != 2:
        raise ValueError("response operation")
    return transfer, request, op, code, detail, draft, cfg

def check_profile_control(base: Path, status_validator) -> int:
    fixture = base / "fixtures" / "profile-source"
    values = {name: bytes.fromhex((fixture / ("control-" + name + "-v1.txt")).read_text().strip())
              for name in ("finish-command", "apply-command", "abort-command", "draft", "queued", "saved", "aborted", "failed")}
    for name, expected in (("finish-command", (1, 1, 1)), ("apply-command", (1, 2, 3)), ("abort-command", (1, 3, 2))):
        if command(values[name]) != expected:
            raise ValueError("control fixture correlation")
    expected = {"draft": (1, 1, 1, 1, 0, 3, 1), "queued": (1, 2, 3, 2, 0, 3, 1),
                "saved": (1, 2, 3, 3, 0, 4, 2), "aborted": (1, 2, 2, 4, 0, 3, 1), "failed": (1, 2, 3, 13, 11, 4, 2)}
    for name, wanted in expected.items():
        if response(values[name]) != wanted:
            raise ValueError(f"control fixture phase {name}")
    import json
    status = json.loads((fixture / "control-status-v2.json").read_text(encoding="utf-8"))
    status_validator.validate(status)
    if status["application"]["configurationRevision"] != 2 or not status["application"]["storedMatchesActive"] or status["application"]["draftRevision"] != 4 or status["game"]["running"]:
        raise ValueError("control saved/status mismatch")
    negatives = []
    c = values["finish-command"]
    for index, byte in ((0, 2), (1, 8), (2, 0), (4, 0), (4, 4), (5, 1), (6, 1), (7, 1), (8, 0)):
        bad = bytearray(c);bad[index] = byte;negatives.append((command, bytes(bad)))
    negatives.extend((command, b) for b in (c[:11], c + b"x"))
    s = values["saved"]
    for index, byte in ((0, 2), (1, 7), (2, 0), (4, 0), (8, 1), (9, 0), (9, 14), (10, 1), (12, 0), (16, 0)):
        bad = bytearray(s);bad[index] = byte;negatives.append((response, bytes(bad)))
    negatives.extend((response, b) for b in (s[:19], s + b"x"))
    for decoder, value in negatives:
        try:
            decoder(value)
        except ValueError:
            pass
        else:
            raise ValueError("accepted malformed profile control/response")
    return len(negatives)


def check_profile_response_queue(base: Path) -> int:
    import json
    fixture = base / "fixtures" / "profile-source"
    actual = response(bytes.fromhex((fixture / "response-queue-saved-v1.txt").read_text().strip()))
    if actual != (1, 2, 3, 3, 0, 4, 2):
        raise ValueError("response queue wire correlation")
    expected = {"scope": "host_profile_response_queue", "depth": 4, "itemBytes": 36,
                "producedAt": 23, "senderServiceAt": 25, "code": 3, "request": 2,
                "configurationRevision": 2, "storedGeneration": 2, "radioMeasured": False}
    observation = json.loads((fixture / "response-queue-observation.json").read_text())
    def accept(value):
        if value != expected:
            raise ValueError("response queue changed original time/model scope")
    accept(observation)
    negatives = []
    for key, value in (("depth", 5), ("itemBytes", 20), ("producedAt", 25), ("code", 2),
                       ("configurationRevision", 1), ("storedGeneration", 1), ("radioMeasured", True)):
        changed = dict(observation); changed[key] = value; negatives.append(changed)
    for changed in negatives:
        try:
            accept(changed)
        except ValueError:
            continue
        raise ValueError("response queue accepted a wrong observation")
    return len(negatives)
