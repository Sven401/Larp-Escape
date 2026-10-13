"""Generate public deterministic TEST vectors; never use this known key in deployment."""
from pathlib import Path
import json
import struct
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
import cryptography
if cryptography.__version__ != "45.0.7":
 raise RuntimeError("Pinned independent vector generator requires cryptography45.0.7")
ROOT=Path(__file__).resolve().parents[1]
fixture=ROOT/"docs/companion/fixtures/peer-proof"
fixture.mkdir(parents=True,exist_ok=True)
def label(i): return bytes((i,))+bytes(14)+bytes((i+1,))
# Scalar1 is public test data (curve generator), never a credential/private key.
key=ec.derive_private_key(1,ec.SECP256R1())
public=key.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)
transcript=(b"LARP-PEER-P256-1"+bytes((1,9,1,0))+label(1)+struct.pack("<IIIIHBBI",0x12345678,0xabcdef01,0x01020304,2,257,1,0,1)+label(2)+bytes(range(1,33)))
assert len(transcript)==108
order=int("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551",16)
der=key.sign(transcript,ec.ECDSA(hashes.SHA256(),deterministic_signing=True))
r,s=utils.decode_dss_signature(der);s=min(s,order-s)
signature=r.to_bytes(32,"big")+s.to_bytes(32,"big")
proof=bytes((1,10,1,0))+struct.pack("<I",1)+signature
key.public_key().verify(utils.encode_dss_signature(r,s),transcript,ec.ECDSA(hashes.SHA256()))
for name,value in (("public-key-v1",public),("challenge-v1",transcript),("proof-v1",proof)):
 (fixture/(name+".txt")).write_text(value.hex().upper()+"\n",encoding="utf-8")
def frames(body,kind):
 return [ (struct.pack("<BBHHH",1,kind,1,len(body),offset)+body[offset:offset+12]).hex().upper() for offset in range(0,len(body),12)]
for name,body,kind in (("challenge-frames20-v1",transcript,9),("proof-frames20-v1",proof,10)):
 (fixture/(name+".txt")).write_text("\n".join(frames(body,kind))+"\n",encoding="utf-8")
metadata={"version":1,"algorithm":"ECDSA_P256_SHA256","encoding":"SEC1_uncompressed65_raw_r_s64_low_s","keyScope":"public_test_generator_never_deploy","challengeBytes":108,"proofBytes":72,"challengeKind":9,"proofKind":10,"maxAgeMilliseconds":30000,"cryptoLibrary":"cryptography","cryptoVersion":cryptography.__version__,"transportSecurityMeasured":False,"deviceMeasured":False}
(fixture/"test-vector-v1.json").write_text(json.dumps(metadata,indent=2)+"\n",encoding="utf-8")
header="#pragma once\n// Public scalar1 test curve generator; never a production credential.\n"
for name,value in (("peerTestPublicKey",public),("peerTestTranscript",transcript),("peerTestProofBytes",proof)):
 header+=f"static const std::uint8_t {name}[{len(value)}] = {{"+",".join(f"0x{b:02x}" for b in value)+"};\n"
(ROOT/"test/test_configured_control/PeerProofVectors.h").write_text(header,encoding="utf-8")
print("Independent deterministic P-256/SHA256 TEST vectors generated; public key only, nine/six 20-byte frames.")
