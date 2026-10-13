"""Independent actual ECDSA peer-proof counterpart (cryptography45.0.7)."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec,utils
from cryptography.exceptions import InvalidSignature
import cryptography
ORDER=int("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551",16)
ROOT=Path(__file__).resolve().parents[1]/"docs/companion/fixtures/peer-proof"
def value(name):
 text=(ROOT/(name+".txt")).read_text(encoding="utf-8").strip()
 if not re.fullmatch(r"(?:[0-9A-F]{2})+",text): raise ValueError("invalid test hex")
 return bytes.fromhex(text)
def challenge_valid(b):
 if len(b)!=108 or b[:16]!=b"LARP-PEER-P256-1" or b[16:20]!=bytes((1,9,1,0)) or b[55]: raise ValueError("invalid challenge header")
 boot_hi,boot_lo,reg,token,handle,interface,reserved,sequence=struct.unpack("<IIIIHBBI",b[36:60])
 if not any(b[20:36]) or not(boot_hi or boot_lo) or not reg or not token or not 1<=interface<255 or handle&255!=interface or not sequence or not any(b[60:76]) or not any(b[76:]): raise ValueError("invalid original challenge binding")
 return sequence
def proof_valid(p,sequence):
 if len(p)!=72 or p[:4]!=bytes((1,10,1,0)) or int.from_bytes(p[4:8],"little")!=sequence: raise ValueError("invalid proof header/sequence")
 r=int.from_bytes(p[8:40],"big");s=int.from_bytes(p[40:],"big")
 if not 0<r<ORDER or not 0<s<=ORDER//2: raise ValueError("noncanonical proof scalar")
 return utils.encode_dss_signature(r,s)
def verify(public,b,p):
 seq=challenge_valid(b);der=proof_valid(p,seq)
 if len(public)!=65 or public[0]!=4: raise ValueError("invalid public key extent/encoding")
 key=ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),public)
 try: key.verify(der,b,ec.ECDSA(hashes.SHA256()))
 except InvalidSignature as e: raise ValueError("signature rejected") from e
 return True
def assemble(lines,kind,total):
 out=bytearray();identity=None
 for line in lines:
  if not re.fullmatch(r"(?:[0-9A-F]{2}){9,20}",line): raise ValueError("invalid frame extent/encoding")
  b=bytes.fromhex(line);version,k,msg,length,offset=struct.unpack("<BBHHH",b[:8])
  if version!=1 or k!=kind or not msg or length!=total or offset!=len(out) or offset+len(b)-8>total: raise ValueError("invalid frame binding/order")
  if identity is None: identity=msg
  if identity!=msg: raise ValueError("mixed frame identity")
  out.extend(b[8:])
 if len(out)!=total: raise ValueError("incomplete document")
 return bytes(out)
def verify_provenance(public,b,p):
 metadata=json.loads((ROOT/"test-vector-v1.json").read_text(encoding="utf-8"))
 expected={"version":1,"algorithm":"ECDSA_P256_SHA256","encoding":"SEC1_uncompressed65_raw_r_s64_low_s","keyScope":"public_test_generator_never_deploy","challengeBytes":108,"proofBytes":72,"challengeKind":9,"proofKind":10,"maxAgeMilliseconds":30000,"cryptoLibrary":"cryptography","cryptoVersion":"45.0.7","transportSecurityMeasured":False,"deviceMeasured":False}
 if set(metadata)!=set(expected) or any(type(metadata[k]) is not type(v) or metadata[k]!=v for k,v in expected.items()): raise ValueError("test vector provenance differs")
 header=(ROOT.parents[3]/"test/test_configured_control/PeerProofVectors.h").read_text(encoding="utf-8")
 for name,expected_bytes in (("peerTestPublicKey",public),("peerTestTranscript",b),("peerTestProofBytes",p)):
  match=re.search(r"static const std::uint8_t "+name+r"\[(\d+)\]\s*=\s*\{([^}]+)\};",header)
  if match is None or int(match.group(1))!=len(expected_bytes): raise ValueError("native test vector header extent differs")
  pieces=[piece.strip() for piece in match.group(2).split(",") if piece.strip()]
  if any(not re.fullmatch(r"0x[0-9a-f]{2}",piece) for piece in pieces) or bytes(int(piece,16) for piece in pieces)!=expected_bytes: raise ValueError("actual native vectors differ from independently checked fixture")

def main():
 parser=argparse.ArgumentParser();parser.add_argument("--without-observation",action="store_true");args=parser.parse_args()
 if cryptography.__version__!="45.0.7": raise ValueError("independent crypto version differs")
 public,b,p=value("public-key-v1"),value("challenge-v1"),value("proof-v1")
 verify(public,b,p);verify_provenance(public,b,p);negatives=0
 for which,data in (("challenge",b),("proof",p),("key",public)):
  for i in range(len(data)):
   bad=bytearray(data);bad[i]^=1
   try: verify(bytes(bad) if which=="key" else public,bytes(bad) if which=="challenge" else b,bytes(bad) if which=="proof" else p)
   except ValueError: negatives+=1;continue
   raise ValueError("incorrect cryptographic vector accepted")
 high=bytearray(p);high[40:]=(ORDER-int.from_bytes(p[40:],"big")).to_bytes(32,"big")
 try: verify(public,b,bytes(high))
 except ValueError: negatives+=1
 else: raise ValueError("high-S twin accepted")
 frame_negatives=0
 for name,kind,total,expected in (("challenge-frames20-v1",9,108,b),("proof-frames20-v1",10,72,p)):
  lines=(ROOT/(name+".txt")).read_text(encoding="utf-8").splitlines()
  if assemble(lines,kind,total)!=expected: raise ValueError("frames differ from actual canonical vector")
  bads=[[],lines[:-1],[lines[0],*lines],[lines[1],lines[0],*lines[2:]],[*lines,lines[-1]],["GG",*lines[1:]]]
  for bad in bads:
   try: assemble(bad,kind,total)
   except ValueError: frame_negatives+=1;continue
   raise ValueError("invalid frame sequence accepted")
 if not args.without_observation:
  actual=json.loads((ROOT/"verification-observation.json").read_text(encoding="utf-8"))
  expected={"scope":"host_actual_peer_crypto","challengeBytes":108,"proofBytes":72,"challengeFrames20":9,"proofFrames20":6,"queueCopies":6,"keyProofVerified":True,"keyProofAloneGrantsGM":False,"explicitRoleAssigned":True,"commandFrames":13,"running":True,"transportSecurityModeled":True,"rfInitialized":False,"deviceMeasured":False}
  if set(actual)!=set(expected) or any(type(actual[k]) is not type(v) or actual[k]!=v for k,v in expected.items()): raise ValueError("actual peer proof observation scope/correlation differs")
 print(f"Actual P256/SHA256 counterpart: canonical signature verified, {negatives} corrupt key/transcript/proofs and {frame_negatives} frame sequences rejected; transport modeled, no device measurement")
 print("Canonical challenge SHA256: "+hashlib.sha256(b).hexdigest())
if __name__=="__main__": main()
