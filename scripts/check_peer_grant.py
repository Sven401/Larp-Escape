#!/usr/bin/env python3
"""Independent correlation of actual native grant execution and public P-256 bytes.
SDK/tasks/NVS are deterministic models; this is no physical transport measurement.
"""
import copy
import json
import struct
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from check_peer_proof import value as proof_value, verify
from check_peer_registry import decode

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = dict(
    scope="host_actual_crypto_original_sdk_proof_grant_model",
    grantSlots=1, grantBytes=188, grantSdkPosts=1, sdkPosts=24,
    queueMilliseconds=5000, proofMilliseconds=30000,
    sourceActor=1, cryptoWorker=3, registryWorker=2,
    proofFrames=6, commandFrames=13, setCalls=3, commitCalls=3,
    originalSmpCallbacks=3, gattReads=1, flagReads=1, stateReads=1, procedureReads=2,
    bootHigh=0x12345678, bootLow=0xABCDEF01, registration=0x01020304, lifecycle=2,
    connectionToken=2, packedConnection=257, interface=1, proofSequence=1, grantSequence=1,
    ackEpoch=3, ackPrincipal=7, storedExplicitRole=True, immutableBootContext=True,
    ackBeforeWorkerConfirmation=True, actorReadsMutableProofOwner=False,
    oldProofRejected=True, runningAfterRevocation=True,
    rfInitialized=False, smpMeasured=False, physicalDurabilityMeasured=False,
)


def accept(observation, records, metadata, challenge, proof, public):
    if not isinstance(observation, dict) or observation != EXPECTED:
        raise ValueError("grant observation")
    if any(type(observation[k]) is not type(v) for k, v in EXPECTED.items()):
        raise ValueError("grant field type")
    if len(records) != 3 or [r["generation"] for r in records] != [1, 2, 3]:
        raise ValueError("registry generations")
    if any(len(r["entries"]) != 1 for r in records):
        raise ValueError("registry extent")
    first = records[0]["entries"][0]
    if [r["entries"][0]["principal"] for r in records] != [0, 7, 0]:
        raise ValueError("explicit role sequence")
    if [r["entries"][0]["gm"] for r in records] != [False, True, False]:
        raise ValueError("explicit GM flags")
    if any(r["device"] != challenge[20:36] or r["entries"][0]["peer"] != challenge[60:76]
           or r["entries"][0]["key"] != public or r["entries"][0]["credential"] != 1
           for r in records):
        raise ValueError("original registered credential")
    verify(public, challenge, proof)  # Independent cryptography, actual signature bytes.
    fields = struct.unpack("<IIIIHBBI", challenge[36:60])
    if fields != tuple(observation[k] for k in ("bootHigh", "bootLow", "registration",
                                              "connectionToken", "packedConnection", "interface")) + (0, observation["proofSequence"]):
        raise ValueError("immutable original challenge binding")
    if metadata["proofBytes"] != len(proof) or observation["proofFrames"] * 12 != len(proof):
        raise ValueError("proof frame extent")
    if metadata["maxAgeMilliseconds"] != observation["proofMilliseconds"]:
        raise ValueError("absolute proof deadline")
    if metadata["deviceMeasured"] is not False or metadata["transportSecurityMeasured"] is not False:
        raise ValueError("model scope")
    if observation["ackEpoch"] <= observation["connectionToken"] or observation["ackPrincipal"] != records[1]["entries"][0]["principal"]:
        raise ValueError("closed original and explicit grant ACK")
    if first["principal"] or first["gm"]:
        raise ValueError("key possession is not a role")


def main():
    folder = ROOT / "docs/companion/fixtures"
    observation = json.loads((folder / "peer-grant/observation.json").read_text(encoding="utf-8"))
    records = [decode(bytes.fromhex((folder / ("peer-registry/" + name + ".hex"))
                                  .read_text(encoding="utf-8").strip()))
               for name in ("key-only", "game-master", "revoked")]
    metadata = json.loads((folder / "peer-proof/test-vector-v1.json").read_text(encoding="utf-8"))
    challenge, proof, public = [proof_value(name) for name in ("challenge-v1", "proof-v1", "public-key-v1")]
    inputs = (observation, records, metadata, challenge, proof, public)
    accept(*inputs)
    wrong = []
    for key, expected in observation.items():
        changed = dict(observation)
        changed[key] = not expected if type(expected) is bool else expected + 1 if type(expected) is int else "device_measured"
        wrong.append((changed, *inputs[1:]))
        changed = dict(observation)
        changed[key] = int(expected) if type(expected) is bool else str(expected) if type(expected) is int else None
        wrong.append((changed, *inputs[1:]))
    wrong.extend(((dict(observation, peerDelivered=True), *inputs[1:]),
                  ({k: v for k, v in observation.items() if k != "scope"}, *inputs[1:])))
    for variant in range(10):
        changed_records = copy.deepcopy(records)
        changed_metadata = dict(metadata)
        changed_challenge, changed_proof = bytearray(challenge), bytearray(proof)
        if variant == 0: changed_records[1]["entries"][0]["principal"] = 0
        if variant == 1: changed_records[2]["entries"][0]["principal"] = 7
        if variant == 2: changed_records[2]["entries"][0]["key"] = bytes(65)
        if variant == 3: changed_records[1]["entries"][0]["gm"] = False
        if variant == 4: changed_records[2]["entries"][0]["credential"] = 2
        if variant == 5: changed_metadata["maxAgeMilliseconds"] = 5000
        if variant == 6: changed_metadata["deviceMeasured"] = True
        if variant == 7: changed_challenge[36] ^= 1
        if variant == 8: changed_proof[8] ^= 1
        if variant == 9: changed_records[1]["device"] = bytes(16)
        wrong.append((observation, changed_records, changed_metadata,
                      bytes(changed_challenge), bytes(changed_proof), public))
    for item in wrong:
        try:
            accept(*item)
        except ValueError:
            continue
        raise ValueError("invalid grant correlation accepted")
    print(f"Grant counterpart: actual P-2566 frames, original copied188B slot/SDK actor ACK, stored GM/game/revoke; {len(wrong)} malformed observations/correlations rejected. SDK/task/NVS model only.")


if __name__ == "__main__":
    main()
