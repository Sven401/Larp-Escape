"""Independent writer review: real wrong-key signatures, copied input and cleanup failure seams."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec,utils
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def head(major,n):
    if n<24:return bytes([(major<<5)|n])
    for size,tag in [(1,24),(2,25),(4,26),(8,27)]:
        if n<1<<(8*size):return bytes([(major<<5)|tag])+n.to_bytes(size,'big')
    raise ValueError('uint64')
def blob(packet,i):
    first=packet[i];assert first>>5==2;i+=1;n=first&31
    if n>=24:size=1<<(n-24);n=int.from_bytes(packet[i:i+size],'big');i+=size
    return packet[i:i+n],i+n

def verify_success(line,c):
    x=line.split();assert x[:3]==['A','None','0'],line;p=bytes.fromhex(x[3]);assert p[:2]==b'\xd2\x84';protected,i=blob(p,2);assert p[i]==0xa0;payload,i=blob(p,i+1);sig,i=blob(p,i);assert i==len(p) and len(sig)==64;assert payload.hex()==x[4]==c['payloadHex'];assert hashlib.sha256(payload).hexdigest()==x[5]==c['payloadDigest'];numbers=ec.derive_private_key(1,ec.SECP256R1()).public_key().public_numbers();cosekey=b'\xa5\x01\x02\x03\x26\x20\x01\x21\x58\x20'+numbers.x.to_bytes(32,'big')+b'\x22\x58\x20'+numbers.y.to_bytes(32,'big');kid=hashlib.sha256(cosekey).digest();assert x[6]==kid.hex();assert protected==b'\xa2\x01\x26\x04\x58\x20'+kid;message=b'\x84\x6aSignature1'+head(2,len(protected))+protected+b'\x4dlarp.event.v1'+head(2,len(payload))+payload;key=ec.derive_private_key(1,ec.SECP256R1()).public_key();assert x[13:15]==[numbers.x.to_bytes(32,'big').hex(),numbers.y.to_bytes(32,'big').hex()];key.verify(utils.encode_dss_signature(int.from_bytes(sig[:32],'big'),int.from_bytes(sig[32:],'big')),message,ec.ECDSA(hashes.SHA256()))
def main():
    a=argparse.ArgumentParser(__doc__);g=a.add_mutually_exclusive_group(required=True);g.add_argument('--compiler');g.add_argument('--binary');args=a.parse_args();env=dict(os.environ)
    if args.binary:binary=Path(args.binary).resolve()
    else:
        cc=Path(args.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');binary=ROOT/'private/manual-override-v2-review/writer-faults.exe';binary.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(cc),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'WriterFaults.cpp'),str(ROOT/'src/Companion/D0Fact.cpp'),str(ROOT/'src/Companion/D0FactWriter.cpp'),'-lbcrypt','-o',str(binary)],check=True,env=env)
    cases=[c for c in json.loads((ROOT/'test/fixtures/d0-fact-review/vectors.json').read_text())['cases'] if c['expected']=='SignatureCheckedUnauthenticated'];rows=[];reference=cases[0]
    def add(c,kind=0,mode=0,target=1):rows.append((c,kind,mode,target))
    for c in cases:
        for kind in [0,2,3]:add(c,kind)
    for kind,mode,target in [(1,0,1),*[(4,mode,target) for target in range(1,6) for mode in range(1,6)],*[(5,0,target) for target in range(1,4)],*[(6,mode,1) for mode in range(1,6)],(7,0,11),(7,0,12)]:add(reference,kind,mode,target);add(reference)
    data='\n'.join(f"{c['key']['x']} {c['key']['y']} {c['coseHex']} {kind} {mode} {target}" for c,kind,mode,target in rows)+'\n';r=subprocess.run([str(binary)],input=data,text=True,capture_output=True,check=True,env=env);lines=r.stdout.splitlines();assert len(lines)==len(rows)
    negatives=0
    for line,(c,kind,mode,target) in zip(lines,rows):
        x=line.split()
        if kind in [0,2,3]:verify_success(line,c)
        else:
            negatives+=1;assert x[0]=='R' and x[1]!='None' and x[2]=='1' and x[3:5]==['-','-'] and x[5:7]==['0'*64]*2 and x[13:15]==['0'*64]*2,(kind,mode,target,line)
            if kind==1:
                assert x[1]=='VerificationFailure' and x[7]=='1' and x[10]=='1' and x[11]==c['signatureHash'],line;signature=bytes.fromhex(x[12]);der=utils.encode_dss_signature(int.from_bytes(signature[:32],'big'),int.from_bytes(signature[32:],'big'));digest=bytes.fromhex(x[11]);ec.derive_private_key(2,ec.SECP256R1()).public_key().verify(der,digest,ec.ECDSA(utils.Prehashed(hashes.SHA256())))
                try:ec.derive_private_key(1,ec.SECP256R1()).public_key().verify(der,digest,ec.ECDSA(utils.Prehashed(hashes.SHA256())));raise AssertionError('wrong-key signature unexpectedly valid under original key')
                except InvalidSignature:pass
            elif kind in [4,7]:assert int(x[8])>0,line
            else:assert int(x[9])>0,line
    print(len(cases),'actual signed roundtrips +',len(cases)*2,'copied/aliased-input roundtrips;',negatives,'wrong-key/hash/verifier-cleanup rejection/reuse pairs;0mismatches')
if __name__=='__main__':main()
