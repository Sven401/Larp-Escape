"""Regenerate PUBLIC wire vectors. Never use this known test secret on hardware."""
import hashlib
import hmac
import json
import struct
from pathlib import Path

def generate():
    secret = bytes(range(32))
    device = bytes(range(0x10, 0x20))
    boot = bytes(range(0x20, 0x30))
    connection = bytes(range(0x30, 0x40))
    client = bytes(range(0x40, 0x50))
    context = device + boot + connection + client
    def tag(domain, body):
        return hmac.new(secret, ("LARP-LED/1/" + domain).encode("ascii") + body, hashlib.sha256).digest()
    session = tag("session", context)[:16]
    def signed(domain, body):
        return body + tag(domain, body)
    def status(kind, revision, request, now, sequence, effect, flags, wake=0):
        return signed("status", bytes([1, kind]) + session + struct.pack("<IIII", revision, request, now, sequence) + bytes([effect, flags, 0, wake]))
    def command(request, revision, expires, op, effect):
        return signed("command", bytes([1, 3]) + session + struct.pack("<III", request, revision, expires) + bytes([op, effect]))
    def result(request, next_request, revision, now, sequence, effect, flags):
        return signed("result", bytes([1, 0x83]) + session + struct.pack("<IIIII", request, next_request, revision, now, sequence) + bytes([0, effect, flags, 0]))
    values = dict(secret=secret, device=device, boot=boot, connection=connection, client=client, session=session,
        hello=bytes([1, 1])+client,
        challenge=signed("challenge", bytes([1, 0x81])+context),
        proof=bytes([1, 2])+tag("proof", context),
        granted=status(0x82, 0, 1, 1002, 1, 0, 5),
        command_start_twinkle=command(1, 0, 4002, 1, 1),
        result_start=result(1, 2, 1, 1011, 2, 1, 1),
        status_twinkle=status(0x84, 1, 2, 1250, 3, 1, 1),
        command_stop=command(2, 1, 4250, 2, 0),
        result_stop=result(2, 3, 2, 1252, 4, 0, 5),
        granted_wake_unknown=status(0x82, 0, 1, 1002, 1, 0, 5, 0x80),
        granted_wake_all=status(0x82, 0, 1, 1002, 1, 0, 5, 0xcf),
        status_wake_1_3=status(0x84, 0, 1, 1250, 2, 0, 5, 0xc5))
    audio = struct.pack("<IHBB", 0, 0, 5, 9)
    values["granted_audio"] = signed("status", values["granted"][:38] + audio)
    values["command_audio_100"] = signed("audio-command", bytes([1, 4]) + session +
        struct.pack("<IIIBHB", 1, 0, 4002, 1, 100, 5))
    values["result_audio_100"] = signed("audio-result", bytes([1, 0x85]) + session +
        struct.pack("<IIIII", 1, 2, 0, 1011, 2) + bytes([0, 0, 5, 0]) +
        struct.pack("<IHBB", 1, 100, 5, 13))
    return {"warning": "Public deterministic test secret. NEVER provision on a real device.",
            **{key: value.hex() for key, value in values.items()}}

if __name__ == "__main__":
    destination = Path(__file__).resolve().parents[2] / "docs/led/fixtures/wire-v1.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(generate(), indent=2) + "\n", encoding="utf-8")
