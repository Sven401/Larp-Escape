#!/usr/bin/env python3
"""Independent client encoder and native SDK observation checker; no RF/Android measurement."""
import copy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from check_peer_proof import value, verify

ROOT = Path(__file__).resolve().parents[1]

def encode_issue(peer):
    if type(peer) is not bytes or len(peer) != 16 or not any(peer):
        raise ValueError("nonzero 16-byte public selector required")
    return bytes((1, 11)) + peer

def decode_issue(frame):
    if type(frame) is not bytes or len(frame) != 18 or frame[:2] != bytes((1, 11)):
        raise ValueError("issue version/kind/length")
    encode_issue(frame[2:])
    return frame[2:]

def accept(observation):
    transcript = value("challenge-v1")
    expected = dict(scope="original_sdk_issue_model_v1",
                    issueHex=encode_issue(transcript[60:76]).hex(),
                    challengeHex=transcript.hex(), readTransaction=2,
                    readConnection=257, readAttribute=68, readStatus=0,
                    nonceCalls=1, actorAckPrincipal=7, actorAckGameMaster=True,
                    rfMeasured=False)
    if type(observation) is not dict or set(observation) != set(expected):
        raise ValueError("original SDK observation fields")
    if any(type(observation[k]) is not type(v) or observation[k] != v for k, v in expected.items()):
        raise ValueError("original SDK binding/types/bytes")
    if decode_issue(bytes.fromhex(observation["issueHex"])) != transcript[60:76]:
        raise ValueError("registry selector mismatch")
    verify(value("public-key-v1"), bytes.fromhex(observation["challengeHex"]), value("proof-v1"))

def main():
    path = ROOT / "docs/companion/fixtures/peer-challenge-issue/observation.json"
    observation = json.loads(path.read_text(encoding="utf-8"))
    accept(observation)
    invalid = []
    for key, expected in observation.items():
        for wrong in (not expected if type(expected) is bool else expected+1 if type(expected) is int else "changed",
                      int(expected) if type(expected) is bool else str(expected) if type(expected) is int else None):
            candidate = copy.deepcopy(observation); candidate[key] = wrong; invalid.append(candidate)
    candidate = copy.deepcopy(observation); candidate["delivered"] = True; invalid.append(candidate)
    candidate = copy.deepcopy(observation); del candidate["readTransaction"]; invalid.append(candidate)
    for candidate in invalid:
        try: accept(candidate)
        except ValueError: continue
        raise ValueError("altered issue observation accepted")
    peer = value("challenge-v1")[60:76]
    frame = encode_issue(peer)
    bad_frames = [b"", frame[:-1], frame+b"\x00", b"\x02"+frame[1:], frame[:1]+b"\x0a"+frame[2:],
                  bytes((1,11))+bytes(16), None, frame.hex()]
    for bad in bad_frames:
        try: decode_issue(bad)
        except ValueError: continue
        raise ValueError("invalid issue frame accepted")
    print(f"Original SDK issue -> stored credential -> canonical challenge -> real P256 proof; {len(invalid)} altered observations and {len(bad_frames)} invalid client frames rejected; SDK/tasks/nonce are models, no RF.")

if __name__ == "__main__":
    main()
