"""Independent deterministic CBOR/COSE fixtures signed by public synthetic P-256 keys."""
import copy,hashlib,json
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ec,utils
from cryptography.hazmat.primitives import hashes
HERE=Path(__file__).resolve().parent
N=int('ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551',16)
AAD=b'larp.event.v1'
class Raw:
    def __init__(self,data):self.data=data

def head(major,n):
    if n<24:return bytes([(major<<5)|n])
    for size,tag in [(1,24),(2,25),(4,26),(8,27)]:
        if n<1<<(8*size):return bytes([(major<<5)|tag])+n.to_bytes(size,'big')
    raise ValueError('uint64')
def cbor(v):
    if isinstance(v,Raw):return v.data
    if v is None:return b'\xf6'
    if type(v) is bool:return b'\xf5' if v else b'\xf4'
    if type(v) is int:return head(0,v) if v>=0 else head(1,-1-v)
    if isinstance(v,bytes):return head(2,len(v))+v
    if isinstance(v,str):return head(3,len(v.encode()))+v.encode()
    if isinstance(v,list):return head(4,len(v))+b''.join(cbor(x) for x in v)
    if isinstance(v,dict):return head(5,len(v))+b''.join(cbor(k)+cbor(v[k]) for k in sorted(v,key=lambda k:(len(cbor(k)),cbor(k))))
    raise TypeError(type(v))
def identity(n):return n.to_bytes(16,'big')
def public(scalar=1):
    p=ec.derive_private_key(scalar,ec.SECP256R1()).public_key().public_numbers();return dict(x=p.x.to_bytes(32,'big').hex(),y=p.y.to_bytes(32,'big').hex())
def keybytes(key):return cbor({1:2,3:-7,-1:1,-2:bytes.fromhex(key['x']),-3:bytes.fromhex(key['y'])})
def payload(type=1):
    origin=identity(3)
    return {0:1,1:1,2:identity(1),3:identity(2),4:origin,5:identity(4),6:1,7:bytes(range(32)),8:bytes(range(32,64)),9:[identity(5),7,type,origin if type==3 else identity(6),True,[],[[1,6]] if type==3 else []]}
def sig_structure(protected,raw,aad=AAD):return cbor(['Signature1',protected,aad,raw])
def sign(raw,protected,scalar=1,aad=AAD):
    der=ec.derive_private_key(scalar,ec.SECP256R1()).sign(sig_structure(protected,raw,aad),ec.ECDSA(hashes.SHA256(),deterministic_signing=True));r,s=utils.decode_dss_signature(der);return r.to_bytes(32,'big')+s.to_bytes(32,'big')
def pack(raw,protected,signature,unprotected=None,tag=True):return (b'\xd2' if tag else b'')+cbor([protected,{} if unprotected is None else unprotected,raw,signature])
def add(name,p=None,*,valid=True,raw=None,key=None,protected=None,aad=AAD,signature=None,scalar=1,unprotected=None,error=None):
    p=payload() if p is None else p;raw=cbor(p) if raw is None else raw;key=public(scalar) if key is None else key;protected=cbor({1:-7,4:hashlib.sha256(keybytes(key)).digest()}) if protected is None else protected;signature=sign(raw,protected,scalar,aad) if signature is None else signature
    value=dict(name=name,key=key,coseHex=pack(raw,protected,signature,unprotected).hex(),expected='SignatureCheckedUnauthenticated' if valid else 'Rejected')
    if error:value['error']=error
    if valid:
        b=p[9];value.update(payloadHex=raw.hex(),protectedHex=protected.hex(),signatureHex=signature.hex(),kid=hashlib.sha256(keybytes(key)).hexdigest(),payloadDigest=hashlib.sha256(raw).hexdigest(),signatureHash=hashlib.sha256(sig_structure(protected,raw)).hexdigest(),fact=dict(event=p[2].hex(),run=p[3].hex(),origin=p[4].hex(),station=p[5].hex(),generation=p[6],binding=p[7].hex(),package=p[8].hex(),epoch=b[0].hex(),sequence=b[1],type=b[2],subject=b[3].hex(),dependencies=[dict(event=d[0].hex(),origin=d[1].hex(),epoch=d[2].hex(),sequence=d[3]) for d in b[5]],ranges=b[6]))
    CASES.append(value);return value
CASES=[]
assert len(keybytes(public()))==77 and len(AAD)==13
for t in [1,2,3]:add('valid-type-'+str(t),payload(t))
for n in [1,23,24,255,256,65535,65536,2**32-1,2**32,2**64-1]:
    p=payload();p[6]=p[9][1]=n;add('uint64-generation-sequence-'+str(n),p)
p=payload();p[7]=p[8]=bytes(32);add('zero-digest-bytes-allowed',p)
p=payload();p[9][5]=[[identity(1),identity(90-i),identity(30+i),2**64-1-i] for i in range(8)];add('eight-unsorted-distinct-dependencies',p)
p=payload(3);p[9][1]=2**64-1;p[9][6]=[[1,1],[2,2],[24,255],[256,65535],[65536,2**32-1],[2**32,2**32],[2**32+1,2**64-3],[2**64-2,2**64-2]];add('eight-ranges-full-width-adjacent-disjoint',p)
base=CASES[0];raw=bytes.fromhex(base['payloadHex']);protected=bytes.fromhex(base['protectedHex']);signature=bytes.fromhex(base['signatureHex']);r=int.from_bytes(signature[:32],'big');s=int.from_bytes(signature[32:],'big')
for name,sv in [('low-s',min(s,N-s)),('high-s',max(s,N-s))]:add('same-payload-'+name,signature=r.to_bytes(32,'big')+sv.to_bytes(32,'big'))
# An additional random valid signature is preserved as fixed bytes after its first generation.
ALT_SIGNATURE='4bcfa473859a0058ad21ce7a25d8d6b3e6ac320e5197424cbe33c7328bc60941ce15ade12a201ac75a41b28d4b698b8147cc7ed4954084bd289395d86b9c94ce'
if ALT_SIGNATURE=='ALT_PLACEHOLDER':
    der=ec.derive_private_key(1,ec.SECP256R1()).sign(sig_structure(protected,raw),ec.ECDSA(hashes.SHA256()));ar,ass=utils.decode_dss_signature(der);ALT_SIGNATURE=(ar.to_bytes(32,'big')+ass.to_bytes(32,'big')).hex()
ec.derive_private_key(1,ec.SECP256R1()).public_key().verify(utils.encode_dss_signature(int(ALT_SIGNATURE[:64],16),int(ALT_SIGNATURE[64:],16)),sig_structure(protected,raw),ec.ECDSA(hashes.SHA256()))
add('same-payload-alternative-valid-signature',signature=bytes.fromhex(ALT_SIGNATURE))
add('valid-second-public-key',scalar=2)
for label in [2,3,4,5]:
    for invalid in [bytes(16),bytes(15),None,False]:
        p=payload();p[label]=invalid;add('bad-identity-'+str(label)+'-'+str(invalid),p,valid=False)
for label in [6]:
    for invalid in [0,-1,None,False,Raw(b'\x1b'+b'\xff'*8+b'\x00')]:
        p=payload();p[label]=invalid;add('bad-generation-'+str(invalid),p,valid=False)
for label in [7,8]:
    for invalid in [bytes(31),bytes(33),None,False]:
        p=payload();p[label]=invalid;add('bad-digest-'+str(label)+'-'+str(invalid),p,valid=False)
for at in [0,3]:
    for invalid in [bytes(16),bytes(15),None,False]:
        p=payload();p[9][at]=invalid;add('bad-body-id-'+str(at)+'-'+str(invalid),p,valid=False)
for at,values in [(1,[0,-1,None,False]),(2,[0,4,2**64-1,None,True]),(4,[False,1,None])]:
    for invalid in values:
        p=payload();p[9][at]=invalid;add('bad-body-field-'+str(at)+'-'+str(invalid),p,valid=False)
p=payload();p[9][5]=[[p[2],p[4],p[9][0],p[9][1]]];add('self-dependency',p,valid=False,error='SelfDependency')
p=payload();p[9][5]=[[p[2],identity(9),identity(8),1]]*2;add('duplicate-dependency',p,valid=False,error='DuplicateDependency')
p=payload();p[9][5]=[[p[2],identity(90-i),identity(30+i),1] for i in range(9)];add('nine-dependencies',p,valid=False)
for at in range(4):
    p=payload();d=[p[2],identity(9),identity(8),1];d[at]=0 if at==3 else bytes(16);p[9][5]=[d];add('bad-dependency-'+str(at),p,valid=False)
for ranges in [[],[[0,1]],[[3,2]],[[1,7]],[[3,3],[1,1]],[[1,3],[3,4]],[[i,i] for i in range(1,10)]]:
    p=payload(3);p[9][6]=ranges;add('bad-ranges-'+str(ranges),p,valid=False)
p=payload(3);p[9][3]=identity(6);add('unused-range-subject-not-origin',p,valid=False)
p=payload(3);p[9][5]=[[p[2],identity(9),identity(8),1]];add('unused-range-with-dependency',p,valid=False)
for t in [1,2]:
    p=payload(t);p[9][6]=[[1,2]];add('solved-override-with-range-'+str(t),p,valid=False)
for kind in [2,3,4,5,0,2**64-1]:
    p=payload();p[1]=kind;add('unsupported-kind-'+str(kind),p,valid=False,error='UnsupportedKind')
p=payload();p[0]=2;add('wrong-version',p,valid=False,error='Version')
# Every malformed semantic/CBOR payload below still has a real signature of those exact bytes.
raw=cbor(payload())
for name,mutated in [('payload-unknown-field',cbor(dict(payload(),**{}))[:-0] if False else cbor({**payload(),10:0})),('payload-missing-field',cbor({k:v for k,v in payload().items() if k!=8})),('payload-duplicate-key',b'\xaa\x00\x01\x00\x01'+raw[5:] ),('payload-reordered-keys',b'\xaa\x01\x01\x00\x01'+raw[5:]),('payload-nonminimal-map',b'\xb8\x0a'+raw[1:]),('payload-nonminimal-version',raw[:2]+b'\x18\x01'+raw[3:]),('payload-indefinite-map',b'\xbf'+raw[1:]+b'\xff'),('payload-trailing-byte',raw+b'\x00'),('payload-extra-tag',b'\xc0'+raw),('payload-null',b'\xf6'),('payload-bool',b'\xf5'),('payload-float',b'\xf9\x3c\x00')]:add(name,raw=mutated,valid=False)
for name,ph in [('header-unknown',cbor({1:-7,4:bytes.fromhex(base['kid']),5:0})),('header-missing-kid',cbor({1:-7})),('header-wrong-alg',cbor({1:-8,4:bytes.fromhex(base['kid'])})),('header-duplicate',b'\xa2\x01\x26\x01\x26'),('header-order',b'\xa2'+cbor(4)+cbor(bytes.fromhex(base['kid']))+cbor(1)+cbor(-7)),('header-nonminimal-alg',b'\xa2\x01\x38\x06\x04'+cbor(bytes.fromhex(base['kid']))),('header-nonminimal-length',b'\xb8\x02'+protected[1:]),('header-kid-width31',cbor({1:-7,4:bytes(31)})),('header-empty',b''),('header-extra-tag',b'\xc0'+protected)]:add(name,protected=ph,valid=False)
add('nonempty-unprotected',unprotected={4:bytes.fromhex(base['kid'])},valid=False)
add('wrong-external-aad',aad=b'larp.event.v2',valid=False,error='InvalidSignature')
add('empty-external-aad',aad=b'',valid=False,error='InvalidSignature')
add('signature-of-payload-only',signature=(lambda rs:rs[0].to_bytes(32,'big')+rs[1].to_bytes(32,'big'))(utils.decode_dss_signature(ec.derive_private_key(1,ec.SECP256R1()).sign(raw,ec.ECDSA(hashes.SHA256(),deterministic_signing=True)))),valid=False,error='InvalidSignature')
for name,key in [('invalid-zero-curve-point',dict(x='00'*32,y='00'*32)),('wrong-candidate-key',public(2))]:add(name,key=key,valid=False)
for name,signature in [('signature-zero',bytes(64)),('signature-short',bytes(63)),('signature-long',bytes(65)),('signature-r-n',N.to_bytes(32,'big')+b'\x01'*32),('signature-s-n',b'\x01'*32+N.to_bytes(32,'big'))]:add(name,signature=signature,valid=False)
for name,mutated in [('missing-tag',bytes.fromhex(base['coseHex'])[1:]),('wrong-tag',b'\xd3'+bytes.fromhex(base['coseHex'])[1:]),('nonminimal-tag',b'\xd8\x12'+bytes.fromhex(base['coseHex'])[1:]),('trailing-root',bytes.fromhex(base['coseHex'])+b'\x00'),('extra-root-tag',b'\xc0'+bytes.fromhex(base['coseHex'])),('nonminimal-array',b'\xd2\x98\x04'+bytes.fromhex(base['coseHex'])[2:]),('indefinite-array',b'\xd2\x9f'+bytes.fromhex(base['coseHex'])[2:]+b'\xff'),('detached-payload',b'\xd2'+cbor([protected,{},None,signature]))]:
    value=add(name,valid=False);value['coseHex']=mutated.hex()
for n in [2048,2049]:
    v=add('read-boundary-'+str(n),valid=False);v['coseHex']=(bytes.fromhex(base['coseHex'])+bytes(n-len(bytes.fromhex(base['coseHex'])))).hex();v['error']='Cbor' if n==2048 else 'LengthLimit'
# Largest fixed Fact shapes without invented padding or extra fields.
p=payload();p[6]=p[9][1]=2**64-1;p[9][5]=[[identity(1),identity(90-i),identity(30+i),2**64-1] for i in range(8)];add('maximal-wide-eight-dependency-fact',p)
p=payload(3);p[6]=p[9][1]=2**64-1;p[9][6]=[[2**56+2*i,2**56+2*i+1] for i in range(8)];add('maximal-wide-eight-range-fact',p)
v=add('candidate-kid-does-not-match-message',valid=False,error='KeyIdMismatch');v['coseHex']=base['coseHex'];v['key']=public(2)
# The separate DER encoding must never be mistaken for COSE raw R||S.
r0=int(base['signatureHex'][:64],16);s0=int(base['signatureHex'][64:],16);add('signature-asn1-der',signature=utils.encode_dss_signature(r0,s0),valid=False)
# Each fixed payload key is independently required, ordered and minimally encoded.
def map_pairs(pairs):return head(5,len(pairs))+b''.join(cbor(k)+cbor(v) for k,v in pairs)
for label in range(10):
    original=list(payload().items())
    for mode in ['missing','duplicate','unknown-replacement','nonminimal-key']:
        pairs=[]
        for k,v in original:
            if k==label:
                if mode=='missing':continue
                k=(label-1 if label else 1) if mode=='duplicate' else 10 if mode=='unknown-replacement' else Raw(bytes([0x18,label]))
            pairs.append((k,v))
        add('payload-key-'+str(label)+'-'+mode,raw=map_pairs(pairs),valid=False)
for name,index,invalid in [('epoch-bool',0,True),('sequence-bool',1,True),('type-float',2,Raw(b'\xf9\x3c\x00')),('dependencies-null',5,None),('dependencies-bool',5,False),('ranges-null',6,None),('ranges-bool',6,False)]:
    p=payload();p[9][index]=invalid;add('body-'+name,p,valid=False)
for body in [payload()[9][:-1],payload()[9]+[0],None,True]:
    p=payload();p[9]=body;add('body-shape-'+str(body),p,valid=False)
for invalid in [[1], [1,2,3], [True,2], [1,False], [None,2]]:
    p=payload(3);p[9][6]=[invalid];add('range-shape-'+str(invalid),p,valid=False)
for invalid in [[identity(1),identity(2),identity(3)], [identity(1),identity(2),identity(3),1,0],None,True]:
    p=payload();p[9][5]=[invalid];add('dependency-shape-'+str(invalid),p,valid=False)
for alg in [None,False,True,0,-8,'-7']:add('protected-alg-'+str(alg),protected=cbor({1:alg,4:bytes.fromhex(base['kid'])}),valid=False)
for kid in [None,False,'0'*64,bytes(33)]:add('protected-kid-'+str(kid),protected=cbor({1:-7,4:kid}),valid=False)
raw=bytes.fromhex(base['payloadHex']);protected=bytes.fromhex(base['protectedHex']);sig=bytes.fromhex(base['signatureHex'])
for name,ph,unprotected,payloadraw,signature in [('protected-map-not-bstr',{1:-7,4:bytes.fromhex(base['kid'])},{},raw,sig),('unprotected-null',protected,None,raw,sig),('unprotected-array',protected,[],raw,sig),('payload-map-not-bstr',protected,{},payload(),sig),('signature-null',protected,{},raw,None),('protected-bstr-nonminimal',Raw(b'\x59'+len(protected).to_bytes(2,'big')+protected),{},raw,sig),('signature-bstr-nonminimal',protected,{},raw,Raw(b'\x59\x00\x40'+sig))]:
    value=add(name,valid=False);value['coseHex']=(b'\xd2'+cbor([ph,unprotected,payloadraw,signature])).hex()
for name,key in [('curve-secp256k1-point',(lambda p:dict(x=p.x.to_bytes(32,'big').hex(),y=p.y.to_bytes(32,'big').hex()))(ec.derive_private_key(1,ec.SECP256K1()).public_key().public_numbers())),('curve-x-outside-field',dict(x='ff'*32,y=public()['y']))]:add(name,key=key,valid=False,error=['InvalidKey','InvalidSignature'])
add('external-aad-extra-nul',aad=AAD+b'\x00',valid=False,error='InvalidSignature')
for name,signature in [('r-zero',bytes(32)+sig[32:]),('s-zero',sig[:32]+bytes(32)),('r-uint256-max',b'\xff'*32+sig[32:]),('s-uint256-max',sig[:32]+b'\xff'*32)]:add(name,signature=signature,valid=False,error='InvalidSignature')
HERE.joinpath('vectors.json').write_text(json.dumps(dict(version=1,keyScalarPublicSynthetic=1,keyEncodingHex=keybytes(public()).hex(),externalAAD=AAD.hex(),cases=CASES),separators=(',',':')),encoding='utf8')
print(len(CASES),'independent signed COSE vectors;',sum(c['expected']=='SignatureCheckedUnauthenticated' for c in CASES),'positive')
