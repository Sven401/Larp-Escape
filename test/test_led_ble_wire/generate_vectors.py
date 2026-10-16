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
        return hmac.new(secret, ("LARP-LED/2/" + domain).encode("ascii") + body, hashlib.sha256).digest()
    session = tag("session", context)[:16]
    def signed(domain, body):
        return body + tag(domain, body)
    def status(kind, revision, request, now, sequence, effect, flags, wake=0, matrix=None, audio=None):
        extension = bytes([audio is not None]) + (audio or bytes(8)) + (bytes(12) if matrix is None else bytes([matrix[0]]) + matrix[1].to_bytes(11, "little"))
        return signed("status", bytes([2, kind]) + session + struct.pack("<IIII", revision, request, now, sequence) + bytes([effect, flags, 0, wake]) + extension)
    def command(request, revision, expires, op, effect):
        return signed("command", bytes([2, 3]) + session + struct.pack("<III", request, revision, expires) + bytes([op, effect]))
    def result(request, next_request, revision, now, sequence, effect, flags):
        return signed("result", bytes([2, 0x83]) + session + struct.pack("<IIIII", request, next_request, revision, now, sequence) + bytes([0, effect, flags, 0]))
    values = dict(secret=secret, device=device, boot=boot, connection=connection, client=client, session=session,
        hello=bytes([2, 1])+client,
        challenge=signed("challenge", bytes([2, 0x81])+context),
        proof=bytes([2, 2])+tag("proof", context),
        granted=status(0x82, 0, 1, 1002, 1, 0, 5),
        command_start_twinkle=command(1, 0, 4002, 1, 1),
        result_start=result(1, 2, 1, 1011, 2, 1, 1),
        status_twinkle=status(0x84, 1, 2, 1250, 3, 1, 1),
        command_stop=command(2, 1, 4250, 2, 0),
        result_stop=result(2, 3, 2, 1252, 4, 0, 5),
        granted_wake_unknown=status(0x82, 0, 1, 1002, 1, 0, 5, 0x80),
        granted_wake_all=status(0x82, 0, 1, 1002, 1, 0, 5, 0xcf),
        status_wake_1_3=status(0x84, 0, 1, 1250, 2, 0, 5, 0xc5),
        granted_matrix_unknown=status(0x82, 0, 1, 1002, 1, 0, 5, 0xc5, (0x80, 0)),
        granted_matrix_corners=status(0x82, 0, 1, 1002, 1, 0, 5, 0xc5, (0xc0, (1 << 80) | 1)),
        status_matrix_rectangle=status(0x84, 0, 1, 1250, 2, 0, 5, 0xc5, (0xe0, (1 << 10) | (1 << 9) | (1 << 1) | 1)))
    def nfc_signed(domain, body):
        return body + hmac.new(secret, ("LARP-NFC/2/" + domain).encode("ascii") + body, hashlib.sha256).digest()
    uid = bytes.fromhex("04010203040506")
    def nfc_command(request, deadline, change):
        return nfc_signed("command", bytes([2, 5]) + session + struct.pack("<II", request, deadline) +
            bytes([2 if change else 1, 7 if change else 0]) + (uid if change else b"").ljust(10, b"\0"))
    def nfc_result(request, sequence, now, outcome, payload):
        return nfc_signed("result", bytes([2, 0x86]) + session + struct.pack("<IIII", request, request + 1, sequence, now) +
            bytes([outcome, 2, 7]) + uid.ljust(10, b"\0") + bytes([25]) + bytes.fromhex(payload))
    values.update(nfc_read=nfc_command(1, 4002, False), nfc_change=nfc_command(2, 4250, True),
        nfc_read_result=nfc_result(1, 1, 1010, 0, "019728350101000000000000000000010001010000F4617B57"),
        nfc_write_result=nfc_result(2, 2, 1252, 8, "01972835010200000000000000000001000101000006D5B37E"))
    audio = struct.pack("<IHBB", 0, 0, 5, 9)
    values["granted_audio"] = status(0x82, 0, 1, 1002, 1, 0, 5, audio=audio)
    values["command_audio_100"] = signed("audio-command", bytes([2, 4]) + session +
        struct.pack("<IIIBHB", 1, 0, 4002, 1, 100, 5))
    values["result_audio_100"] = signed("audio-result", bytes([2, 0x85]) + session +
        struct.pack("<IIIII", 1, 2, 0, 1011, 2) + bytes([0, 0, 5, 0]) +
        struct.pack("<IHBB", 1, 100, 5, 13))
    values["status_all"] = status(0x84, 0, 1, 1250, 2, 0, 5, 0xe5, (0xc0, (1 << 80) | 1), audio)
    return {"warning": "Public deterministic test secret. NEVER provision on a real device.",
            **{key: value.hex() for key, value in values.items()}}

if __name__ == "__main__":
    destination = Path(__file__).resolve().parents[2] / "docs/led/fixtures/wire-v2.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(generate(), indent=2) + "\n", encoding="utf-8")
