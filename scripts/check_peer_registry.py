"""Independent public registry bytes/curve/host observation counterpart."""
from pathlib import Path
import argparse
import json
import re
import struct
import subprocess
import zlib
import cryptography
from cryptography.hazmat.primitives.asymmetric import ec
ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'docs/companion/fixtures/peer-registry'
def decode(raw,curve=True):
    if len(raw)!=404 or raw[:4]!=b'LPRG' or raw[4]!=1 or raw[5]>4 or any(raw[6:8]) or any(raw[28:32]): raise ValueError('header/extent')
    if struct.unpack('<I',raw[400:])[0]!=zlib.crc32(raw[:400]): raise ValueError('CRC')
    generation=struct.unpack('<I',raw[8:12])[0]; device=raw[12:28]
    if not generation or not any(device): raise ValueError('device/generation')
    entries=[]
    for i in range(4):
        entry=raw[32+i*92:32+(i+1)*92]
        if i>=raw[5]:
            if any(entry): raise ValueError('unused entry')
            continue
        peer,key=entry[:16],entry[16:81]
        credential,principal=struct.unpack('<II',entry[81:89]); gm=entry[89]
        if not any(peer) or not credential or key[0]!=4 or gm>1 or any(entry[90:]) or (bool(principal)!=bool(gm)): raise ValueError('entry fields')
        if entries and peer<=entries[-1]['peer']: raise ValueError('peer order')
        if any(old['key']==key or principal and old['principal']==principal for old in entries): raise ValueError('duplicate key/principal')
        if curve: ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),key)
        entries.append(dict(peer=peer,key=key,credential=credential,principal=principal,gm=bool(gm)))
    return dict(device=device,generation=generation,entries=entries)
def recalc(raw):
    result=bytearray(raw); result[400:]=struct.pack('<I',zlib.crc32(result[:400])); return bytes(result)
def inspect(raw,tool,valid):
    result=subprocess.run([str(tool),'--inspect-peer-registry-record',raw.hex().upper()],capture_output=True,text=True,check=False)
    if not valid:
        if result.returncode==0 or result.stdout: raise ValueError('tool accepted corrupt record')
        return
    actual=json.loads(result.stdout); selected=decode(raw,False)
    expected=dict(scope='inspect_public_peer_registry',device=selected['device'].hex().upper(),generation=selected['generation'],peers=len(selected['entries']),gameMasters=sum(x['gm'] for x in selected['entries']),recordBytes=404,cryptoValidated=False,stored=False)
    if result.returncode or actual!=expected or any(type(actual[k]) is not type(v) for k,v in expected.items()): raise ValueError('tool inspection scope')
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--tool',type=Path); args=parser.parse_args()
    if cryptography.__version__!='45.0.7': raise ValueError('curve counterpart version')
    records=[]
    for name,generation,gm in [('key-only',1,False),('game-master',2,True),('revoked',3,False)]:
        text=(FOLDER/(name+'.hex')).read_text(encoding='utf-8').strip()
        if not re.fullmatch('[0-9A-F]{808}',text): raise ValueError('fixture hex')
        raw=bytes.fromhex(text); actual=decode(raw); records.append(raw)
        if actual['device']!=bytes((1,))+bytes(14)+bytes((2,)) or actual['generation']!=generation or len(actual['entries'])!=1: raise ValueError('record correlation')
        entry=actual['entries'][0]
        if entry['peer']!=bytes((2,))+bytes(14)+bytes((3,)) or entry['credential']!=1 or entry['gm']!=gm or entry['principal']!=(7 if gm else 0): raise ValueError('explicit role correlation')
        key=bytes.fromhex((ROOT/'docs/companion/fixtures/peer-proof/public-key-v1.txt').read_text().strip())
        if entry['key']!=key: raise ValueError('proof key/registry correlation')
        if args.tool: inspect(raw,args.tool,True)
    negatives=[]; original=records[1]
    for at in range(404):
        raw=bytearray(original); raw[at]^=1; negatives.append(bytes(raw))
    negatives.extend([original[:-1],original+b'x'])
    for at in [0,1,2,3,4,5,6,7,28,29,30,31,121,122,123,124,399]:
        raw=bytearray(original);raw[at]=5;negatives.append(recalc(raw))
    for start,end in [(8,12),(12,28),(32,48),(113,117),(117,121)]:
        raw=bytearray(original);raw[start:end]=bytes(end-start);negatives.append(recalc(raw))
    header=(ROOT/'test/test_configured_control/PeerRegistryVectors.h').read_text()
    match=re.search(r'peerRegistrySecondKey\[65\]\s*=\s*\{([^}]+)\}',header)
    if not match: raise ValueError('second public vector header')
    point=bytes(int(x.strip(),16) for x in match.group(1).split(',') if x.strip())
    ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),point)
    two=bytearray(original);two[5]=2;two[124:216]=two[32:124]
    two[124:140]=bytes((3,))+bytes(14)+bytes((4,));two[140:205]=point;two[209:213]=struct.pack('<I',8)
    two=recalc(two);decode(two)
    if args.tool: inspect(two,args.tool,True)
    for start,end,source_start,source_end in [(124,140,32,48),(140,205,48,113),(209,213,117,121)]:
        raw=bytearray(two);raw[start:end]=raw[source_start:source_end];negatives.append(recalc(raw))
    raw=bytearray(two);raw[32:124],raw[124:216]=raw[124:216],raw[32:124];negatives.append(recalc(raw))
    for raw in negatives:
        try: decode(raw)
        except ValueError: pass
        else: raise ValueError('invalid record accepted')
        if args.tool: inspect(raw,args.tool,False)
    if args.tool:
        for text in ['',original.hex(),original.hex().upper()[:-1],original.hex().upper()+'00','GG'+original.hex().upper()[2:]]:
            result=subprocess.run([str(args.tool),'--inspect-peer-registry-record',text],capture_output=True,text=True,check=False)
            if result.returncode!=2 or result.stdout: raise ValueError('tool accepted invalid hex extent/encoding')
    wrong=bytearray(original);wrong[49]^=1;wrong=recalc(wrong)
    try: decode(wrong)
    except ValueError: pass
    else: raise ValueError('invalid curve point accepted')
    # Structural inspection deliberately cannot authenticate a stored key.
    if args.tool: inspect(wrong,args.tool,True)
    observation=json.loads((FOLDER/'observation.json').read_text())
    expected=dict(scope='host_actual_crypto_nvs_model',recordBytes=404,peers=1,setCalls=3,commitCalls=3,restoredExplicitRole=True,proofFrames=6,commandFrames=13,runningAfterRevocation=True,oldProofRejected=True,transportSecurityModeled=True,physicalDurabilityMeasured=False,rfInitialized=False)
    def accept_observation(value):
        if value!=expected or any(type(value[k]) is not type(v) for k,v in expected.items()): raise ValueError('observation scope')
    accept_observation(observation)
    wrong_observations=[]
    for key,value in expected.items():
        wrong=dict(expected);wrong[key]=not value if isinstance(value,bool) else value+1 if isinstance(value,int) else 'device_measured';wrong_observations.append(wrong)
    wrong_observations.extend([dict(expected,unexpected=True),{k:v for k,v in expected.items() if k!='scope'}])
    for wrong in wrong_observations:
        try: accept_observation(wrong)
        except ValueError: continue
        raise ValueError('invalid observation accepted')
    print(f'Peer registry counterpart: 3 actual C++ records, P256 point and explicit role verified; {len(negatives)} corrupt records and 1 invalid curve point rejected; {len(wrong_observations)} wrong observations rejected; NVS/security modeled')
if __name__=='__main__': main()
