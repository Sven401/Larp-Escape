"""Independent challenge read/ReadBlob counterpart; public native-generated SDK observations.
No BLE/RF/Android/SDK runtime measurement. Strictly pins each original response and signature.
"""
import copy
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
import cryptography
from check_peer_proof import value, verify
ROOT = Path(__file__).resolve().parents[1]
META = dict(scope="actual_crypto_original_sdk_read_model_v1", mtuModel=23,
            queueMilliseconds=1000, proofMilliseconds=30000, sdkReadCallbackType=1,
            attribute=68, nonceCalls=1, actualSignatureVerified=True,
            actorAckGameMaster=True, actorAckPrincipal=7, peerDelivered=False, rfMeasured=False)


def accept(observation):
    if not isinstance(observation, dict) or set(observation) != set(META) | {"reads"}:
        raise ValueError("read observation keys")
    if any(type(observation[k]) is not type(v) or observation[k] != v for k, v in META.items()):
        raise ValueError("read scope/types")
    reads = observation["reads"]
    if not isinstance(reads, list) or len(reads) != 5:
        raise ValueError("read count")
    transcript = value("challenge-v1")
    assembled = bytearray()
    for index, read in enumerate(reads):
        expected = dict(packedConnection=257, transaction=index + 1,
                        offset=index * 22, sdkBytes=108 - index * 22,
                        remainingHex=transcript[index * 22:].hex())
        if not isinstance(read, dict) or set(read) != set(expected):
            raise ValueError("read keys")
        if any(type(read[k]) is not type(v) or read[k] != v for k, v in expected.items()):
            raise ValueError("original response binding/bytes")
        if not re.fullmatch(r"(?:[0-9a-f]{2})+", read["remainingHex"]):
            raise ValueError("response encoding")
        # Official attp_build_value_cmd sends <=MTU-1 value bytes. Raw SDK value
        # is remaining transcript; this is a size model, no measured air trace.
        fragment = bytes.fromhex(read["remainingHex"])[:observation["mtuModel"] - 1]
        if read["offset"] != len(assembled):
            raise ValueError("mixed/skipped snapshot")
        assembled.extend(fragment)
    if bytes(assembled) != transcript:
        raise ValueError("incomplete challenge")
    verify(value("public-key-v1"), bytes(assembled), value("proof-v1"))
    return bytes(assembled)


def main():
    if cryptography.__version__ != "45.0.7":
        raise ValueError("independent crypto version")
    path = ROOT / "docs/companion/fixtures/peer-challenge-read/observation.json"
    observation = json.loads(path.read_text(encoding="utf-8"))
    accept(observation)
    bads = []
    for key, expected in META.items():
        for wrong in (not expected if type(expected) is bool else expected + 1 if type(expected) is int else "measured",
                      int(expected) if type(expected) is bool else str(expected) if type(expected) is int else None):
            changed = copy.deepcopy(observation); changed[key] = wrong; bads.append(changed)
    for index, read in enumerate(observation["reads"]):
        for key, expected in read.items():
            for wrong in (expected + 1 if type(expected) is int else "00", str(expected) if type(expected) is int else None):
                changed = copy.deepcopy(observation); changed["reads"][index][key] = wrong; bads.append(changed)
        changed = copy.deepcopy(observation); changed["reads"][index]["gm"] = True; bads.append(changed)
    for reads in (observation["reads"][:-1], list(reversed(observation["reads"])),
                  [observation["reads"][0], *observation["reads"]]):
        changed = copy.deepcopy(observation); changed["reads"] = reads; bads.append(changed)
    changed = copy.deepcopy(observation); changed["sdkDelivered"] = True; bads.append(changed)
    for bad in bads:
        try: accept(bad)
        except ValueError: continue
        raise ValueError("invalid challenge read accepted")
    print(f"Original challenge read counterpart: five raw SDK remaining values -> 108 canonical bytes -> actual P256 signature; {len(bads)} altered observations rejected; MTU size model, no peer delivery")


if __name__ == "__main__":
    main()
