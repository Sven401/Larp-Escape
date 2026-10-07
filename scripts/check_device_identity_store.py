"""Independent struct/zlib counterpart for actual public-ID storage fixtures."""
from pathlib import Path
import json
import re
import struct
import zlib


def decode_record(value: bytes) -> bytes:
    if len(value) != 28:
        raise ValueError("identity record length")
    magic, version, reserved, identity, crc = struct.unpack("<4sB3s16sI", value)
    if magic != b"LDID" or version != 1 or reserved != bytes(3) or not any(identity):
        raise ValueError("identity record semantics")
    if zlib.crc32(value[:24]) != crc:
        raise ValueError("identity record CRC")
    return identity


def check_device_identity_store(base: Path, metadata_validator) -> tuple[int, int]:
    folder = base / "fixtures" / "device-identity-store"
    text = (folder / "record-v1.txt").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"[0-9A-F]{56}", text):
        raise ValueError("identity record hex")
    record = bytes.fromhex(text)
    identity = decode_record(record)
    if identity != bytes(range(1, 17)):
        raise ValueError("identity record/model correlation")
    metadata = json.loads((folder / "metadata-v1.json").read_text(encoding="utf-8"))
    metadata_validator.validate(metadata)
    if metadata["deviceId"] != identity.hex().upper() or metadata["scope"] != "display_identity":
        raise ValueError("identity record/public metadata mismatch")
    negatives = []
    for at in range(28):
        changed = bytearray(record); changed[at] ^= 1; negatives.append(bytes(changed))
    negatives.extend((record[:27], record + b"x"))
    for at in range(8):
        changed = bytearray(record); changed[at] ^= 1
        changed[24:] = struct.pack("<I", zlib.crc32(changed[:24])); negatives.append(bytes(changed))
    changed = bytearray(record); changed[8:24] = bytes(16)
    changed[24:] = struct.pack("<I", zlib.crc32(changed[:24])); negatives.append(bytes(changed))
    for changed in negatives:
        try:
            decode_record(changed)
        except ValueError:
            continue
        raise ValueError("accepted a malformed identity record")
    expected = {"scope": "host_nvs_model", "recordBytes": 28, "setCalls": 1, "commitCalls": 1,
                "entropyFills": 1, "sameAfterRestart": True, "secretsStored": False,
                "grantsGameMaster": False, "radioInitialized": False, "physicalDurabilityMeasured": False}
    def accept(value):
        if value != expected:
            raise ValueError("identity store model/security/measurement scope")
    accept(json.loads((folder / "observation.json").read_text(encoding="utf-8")))
    wrong_observations = []
    for key, value in (("scope", "device"), ("recordBytes", 27), ("setCalls", 2), ("commitCalls", 0),
                       ("entropyFills", 2), ("sameAfterRestart", False), ("secretsStored", True),
                       ("grantsGameMaster", True), ("radioInitialized", True), ("physicalDurabilityMeasured", True)):
        wrong = dict(expected); wrong[key] = value; wrong_observations.append(wrong)
    for wrong in wrong_observations:
        try:
            accept(wrong)
        except ValueError:
            continue
        raise ValueError("accepted a wrong identity store observation")
    return len(negatives), len(wrong_observations)
