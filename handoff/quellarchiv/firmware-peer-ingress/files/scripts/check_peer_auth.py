#!/usr/bin/env python3
"""Independent peer-auth client codec and actual native SDK observation check."""
import copy
import json
import re
import struct
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from check_peer_proof import value, verify, challenge_valid
from check_peer_challenge_issue import encode_issue

ROOT = Path(__file__).resolve().parents[1]

def frame(message, proof, offset, size=12):
    if type(message) is not int or not 0 < message <= 65535 or type(proof) is not bytes or len(proof) != 72:
        raise ValueError("proof frame identity/extent")
    if type(offset) is not int or type(size) is not int or not 0 <= offset < 72 or not 0 < size <= 120 or offset+size > 72:
        raise ValueError("proof frame bounds")
    return struct.pack("<BBHHH", 1, 10, message, 72, offset)+proof[offset:offset+size]

def assemble(frames):
    if type(frames) is not list or not frames:
        raise ValueError("proof frames required")
    out = bytearray(); message = None
    for text in frames:
        if type(text) is not str or not re.fullmatch(r"(?:[0-9a-f]{2}){9,128}", text):
            raise ValueError("proof frame encoding")
        b = bytes.fromhex(text)
        version, kind, msg, total, offset = struct.unpack("<BBHHH", b[:8])
        if version != 1 or kind != 10 or not msg or total != 72 or offset != len(out) or offset+len(b)-8 > 72:
            raise ValueError("proof frame order/binding")
        if message is None: message = msg
        if message != msg: raise ValueError("mixed proof message")
        out.extend(b[8:])
    if len(out) != 72: raise ValueError("incomplete proof")
    return message, bytes(out)

def result(b):
    if type(b) is not bytes or len(b) != 32:
        raise ValueError("result extent")
    version, kind, state, reason, message, received, sequence, principal = struct.unpack("<BBBBHHII", b[:16])
    peer = b[16:]
    if version != 1 or kind != 12 or not message or not sequence or not any(peer) or received > 72:
        raise ValueError("result identity/header")
    valid = ((state == 1 and 0 < received < 72 and reason == 0 and principal == 0) or
             (state == 2 and reason in (1, 2, 3) and principal == 0) or
             (state in (3, 4) and received == 72 and reason == 0 and principal == 0) or
             (state == 5 and received == 72 and reason == 0 and principal != 0))
    if not valid: raise ValueError("result state/reason/authority")
    return state, reason, message, received, sequence, principal, peer

def accept(actual):
    transcript, proof = value("challenge-v1"), value("proof-v1")
    sequence = challenge_valid(transcript); peer = transcript[60:76]
    expected_result = struct.pack("<BBBBHHII", 1, 12, 5, 0, 9, 72, sequence, 7)+peer
    expected = dict(scope="original_sdk_peer_auth_model_v1", issueHex=encode_issue(peer).hex(),
                    challengeHex=transcript.hex(), proofFrames=[frame(9, proof, offset).hex() for offset in range(0, 72, 12)],
                    resultHex=expected_result.hex(), blobHex=expected_result[22:].hex(),
                    readConnection=257, resultTransaction=9, blobTransaction=10,
                    resultAttribute=71, blobOffset=22, nonceCalls=1, rfMeasured=False)
    if type(actual) is not dict or set(actual) != set(expected): raise ValueError("observation fields")
    if any(type(actual[k]) is not type(v) or actual[k] != v for k, v in expected.items()):
        raise ValueError("original SDK bytes/binding/types")
    message, assembled = assemble(actual["proofFrames"])
    verify(value("public-key-v1"), bytes.fromhex(actual["challengeHex"]), assembled)
    state, reason, msg, received, seq, principal, selected = result(bytes.fromhex(actual["resultHex"]))
    if (state, reason, msg, received, seq, principal, selected) != (5, 0, message, 72, sequence, 7, peer):
        raise ValueError("uncorrelated result")
    if bytes.fromhex(actual["blobHex"]) != bytes.fromhex(actual["resultHex"])[actual["blobOffset"]:]:
        raise ValueError("mixed blob snapshot")

def rejected(check, values):
    for bad in values:
        try: check(bad)
        except ValueError: continue
        raise ValueError("invalid client input accepted")
    return len(values)

def main():
    actual = json.loads((ROOT/"docs/companion/fixtures/peer-auth/observation.json").read_text(encoding="utf-8"))
    accept(actual); altered = []
    for key, expected in actual.items():
        for wrong in (not expected if type(expected) is bool else expected+1 if type(expected) is int else "changed", None):
            bad=copy.deepcopy(actual); bad[key]=wrong; altered.append(bad)
    bad=copy.deepcopy(actual); del bad["nonceCalls"]; altered.append(bad)
    bad=copy.deepcopy(actual); bad["delivered"]=True; altered.append(bad)
    for key in ("issueHex", "challengeHex", "resultHex", "blobHex"):
        for index in range(len(actual[key])//2):
            bad=copy.deepcopy(actual); b=bytearray.fromhex(bad[key]); b[index]^=1; bad[key]=b.hex(); altered.append(bad)
    frames=actual["proofFrames"]
    for index in range(len(frames)):
        for byte in range(len(bytes.fromhex(frames[index]))):
            bad=copy.deepcopy(actual); b=bytearray.fromhex(frames[index]); b[byte]^=1; bad["proofFrames"][index]=b.hex(); altered.append(bad)
    observations=rejected(accept, altered)
    frame_bads=[None, [], frames[:-1], [*frames,frames[-1]], [frames[0],*frames], [frames[1],frames[0],*frames[2:]], ["GG",*frames[1:]], [True,*frames[1:]]]
    mutated=frames.copy(); b=bytearray.fromhex(mutated[1]); b[2]=10; mutated[1]=b.hex(); frame_bads.append(mutated)
    framing=rejected(assemble, frame_bads)
    raw=bytes.fromhex(actual["resultHex"])
    result_bads=[None, raw.hex(), raw[:-1], raw+b"\x00"]
    for offset,new in ((0,2),(1,10),(2,0),(3,1),(4,0),(6,73),(8,0),(12,0)):
        b=bytearray(raw); b[offset]=new; result_bads.append(bytes(b))
    result_bads.append(raw[:16]+bytes(16))
    results=rejected(result, result_bads)
    # Every supported state must parse with its own authority and progress constraints.
    for state in range(1,6):
        for reason in range(4):
            for received in (0,12,72,73):
                for principal in (0,7):
                    b=struct.pack("<BBBBHHII",1,12,state,reason,9,received,1,principal)+raw[16:]
                    expected=((state==1 and 0<received<72 and reason==0 and principal==0) or
                              (state==2 and received<=72 and reason in (1,2,3) and principal==0) or
                              (state in (3,4) and received==72 and reason==0 and principal==0) or
                              (state==5 and received==72 and reason==0 and principal==7))
                    try: result(b)
                    except ValueError:
                        if expected: raise
                    else:
                        if not expected: raise ValueError("contradictory authority/progress accepted")
    print(f"Original SDK issue/read/proof/ACK/result/blob + real P256: {observations} altered observations, {framing} invalid fragment streams, {results} invalid results rejected; 160 state combinations checked; no RF measurement.")

if __name__ == "__main__": main()
