"""Run real independently signed D0 COSE bytes through CNG/OpenSSL or production PSA."""
import argparse,copy,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def protocol(c,failure=-1,hash_mode=0,hash_document=1,crypto_mode=0,reset_failure=0):return '\n'.join([f'{failure} {hash_mode} {hash_document} {crypto_mode} {reset_failure}',c['key']['x'],c['key']['y'],c['coseHex']])+'\n'
def expected_fact(f):
    values=[f[k] for k in ['event','run','origin','station','generation','binding','package','epoch','sequence','type','subject']]+[len(f['dependencies']),len(f['ranges'])]
    for i in range(8):
        d=f['dependencies'][i] if i<len(f['dependencies']) else dict(event='0'*32,origin='0'*32,epoch='0'*32,sequence=0)
        values += [d[k] for k in ['event','origin','epoch','sequence']]+(f['ranges'][i] if i<len(f['ranges']) else [0,0])
    return ','.join(map(str,values))
def main():
    p=argparse.ArgumentParser(__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--compiler');g.add_argument('--binary');p.add_argument('--build-dir',default='private/d0-fact-review');p.add_argument('--no-faults',action='store_true');a=p.parse_args();env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/('cli.exe' if os.name=='nt' else 'cli');subprocess.run([str(cc),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'cli.cpp'),str(ROOT/'src/Companion/D0Fact.cpp'),'-lbcrypt' if os.name=='nt' else '-lcrypto','-o',str(binary)],env=env,check=True)
    corpus=json.loads((HERE/'vectors.json').read_text(encoding='utf8'));cases=corpus['cases'];payload=''.join(protocol(c) for c in cases);prefixes=[];sourcefaults=[];hashfaults=[];cryptofaults=[];limits=[]
    reference=cases[0]
    if not a.no_faults:
        for c in [reference,next(c for c in cases if c['name']=='maximal-wide-eight-dependency-fact')]:
            n=len(bytes.fromhex(c['coseHex']))
            for size in range(n):q=copy.deepcopy(c);q['coseHex']=q['coseHex'][:size*2];payload+=protocol(q)+protocol(c);prefixes.append((c,size))
        for c in [reference,next(c for c in cases if c['name']=='maximal-wide-eight-dependency-fact')]:
            for offset in range(len(bytes.fromhex(c['coseHex']))+1):payload+=protocol(c,offset)+protocol(c);sourcefaults.append((c,offset))
        for n,offset,error in [(2048,2048,'SourceFailure'),(2049,2048,'SourceFailure'),(2049,2049,'LengthLimit')]:
            c=next(c for c in cases if c['name']=='read-boundary-'+str(n));payload+=protocol(c,offset)+protocol(reference);limits.append((n,offset,error))
        for doc in range(1,4):
            for mode in range(1,6):payload+=protocol(reference,hash_mode=mode,hash_document=doc)+protocol(reference);hashfaults.append((doc,mode))
        for mode in range(1,6):payload+=protocol(reference,crypto_mode=mode)+protocol(reference);cryptofaults.append(('verify',mode))
        for call in [1,2]:payload+=protocol(reference,reset_failure=call)+protocol(reference);cryptofaults.append(('reset',call))
    r=subprocess.run([str(binary)],input=payload,capture_output=True,text=True,encoding='utf8',env=env,check=True);lines=iter(r.stdout.splitlines());fails=[]
    def rejected(label,line,error=None):
        x=line.split()
        if x[0]!='R' or x[4]!='1' or x[10]!='0' or x[11:15]!=['-']*4 or x[15:17]!=['0'*64]*2 or (error and x[1] not in (error if isinstance(error,list) else [error])):fails.append((label,line,error))
    def check(c,line):
        x=line.split()
        if c['expected']=='SignatureCheckedUnauthenticated':
            expected=[str(len(bytes.fromhex(c['coseHex']))),c['coseHex'],c['protectedHex'],c['payloadHex'],c['signatureHex'],c['kid'],c['payloadDigest'],c['signatureHash'],expected_fact(c['fact'])]
            if x[0]!='A' or x[1]!='None' or x[4]!='0' or x[7]!='1' or x[10:19]!=expected or int(x[3])!=len(bytes.fromhex(c['coseHex'])):fails.append((c['name'],'positive exact bytes/fact/hash',line,expected))
        else:rejected(c['name'],line,c.get('error'))
    for c in cases:check(c,next(lines))
    for c,size in prefixes:rejected(('prefix',c['name'],size),next(lines));check(c,next(lines))
    for c,offset in sourcefaults:
        line=next(lines);rejected(('source',c['name'],offset),line,'SourceFailure');x=line.split()
        if int(x[2])!=offset or int(x[3])!=offset or x[7]!='0':fails.append(('source offset/crypto-before-EOF',c['name'],offset,line))
        check(c,next(lines))
    for n,offset,error in limits:
        line=next(lines);rejected(('limit-EOF',n,offset),line,error)
        if int(line.split()[3])!=offset or line.split()[7]!='0':fails.append(('limit-bound/read-before-crypto',n,offset,line))
        check(reference,next(lines))
    for doc,mode in hashfaults:
        line=next(lines);rejected(('hash',doc,mode),line);x=line.split()
        if x[1] not in ['HashFailure','CleanupFailure'] or int(x[6])==0 or x[7]!='0':fails.append(('hash result/hit/noverify',doc,mode,line))
        check(reference,next(lines))
    for kind,mode in cryptofaults:
        line=next(lines);expected={'verify':{1:'InvalidSignature',2:'InvalidKey',3:'CryptoUnavailable',4:'CryptoFailure',5:'CleanupFailure'},'reset':{1:'CleanupFailure',2:'CleanupFailure'}}[kind][mode];rejected(('crypto',kind,mode),line,expected)
        if int(line.split()[9])==0:fails.append(('crypto fault not hit',kind,mode,line))
        check(reference,next(lines))
    assert next(lines,None) is None
    print(len(cases),'COSE byte vectors (positive signatures real);',len(prefixes),'prefix/reuse;',len(sourcefaults),'everybyte source/reuse;',len(hashfaults),'hash/reuse;',len(cryptofaults),'crypto/reset/reuse;',len(limits),'limit-EOF/reuse;',len(fails),'mismatches')
    for f in fails[:12]:print(f)
    assert not fails
if __name__=='__main__':main()
