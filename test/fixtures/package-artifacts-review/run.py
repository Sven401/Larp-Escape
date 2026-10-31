"""Independent complete raw artifact sets through the actual production validator."""
import argparse,copy,hashlib,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]

def protocol(c,path='',failure=-1,crypto=(0,0,1),seam=0):
    if not path and 'sourceFailure' in c:path=c['sourceFailure']['path'];failure=c['sourceFailure']['offset']
    bypath={r['path']:r for r in c['sources']}
    lines=[f"{len(c['modulePins'])} {len(c['resourcePins'])} {len(c['sources'])} {failure} {crypto[0]} {crypto[1]} {crypto[2]} {seam}",c['selectedStation'],c['owner']['event'],c['owner']['box'],c['owner']['epoch'],c['deploymentPath'],c['contentPath'],path]
    for m in c['modulePins']:lines.extend([m['moduleId'],m['version'],m['digest'],bypath[m['path']]['hex']])
    for p in c['resourcePins']:lines.extend([p['digest'],str(p['bytes']),p['path']])
    for r in c['sources']:lines.extend([r['path'],r['hex']])
    return '\n'.join(lines)+'\n'

def main():
    p=argparse.ArgumentParser(__doc__);b=p.add_mutually_exclusive_group(required=True);b.add_argument('--compiler');b.add_argument('--binary');p.add_argument('--build-dir',default='private/package-artifacts-review');p.add_argument('--no-faults',action='store_true');a=p.parse_args();env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/'cli.exe';sources=[ROOT/'src'/f for f in ['GameConfiguration/CanonicalPackageJson.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/PackageModules.cpp','GameConfiguration/PackageMetadata.cpp','GameConfiguration/ProfileReader.cpp','GameConfiguration/ProfileValidation.cpp','GameExecution/RulesPlanner.cpp','GameExecution/PackageArtifacts.cpp']];subprocess.run([str(cc),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'cli.cpp'),*map(str,sources),'-lbcrypt','-o',str(binary)],check=True,env=env)
    cases=json.loads((HERE/'vectors.json').read_text(encoding='utf8'));payload=''.join(protocol(c) for c in cases);faults=[];reference=cases[0]
    if not a.no_faults:
        for r in reference['sources']:
            for offset in sorted({0,len(bytes.fromhex(r['hex']))//2,len(bytes.fromhex(r['hex']))}):
                faults.append((r['path'],offset));payload+=protocol(reference,r['path'],offset)+protocol(reference)
    crypto_cases=[];priority_cases=[];binding_prefixes=[];binding_faults=[]
    if not a.no_faults:
        probe=subprocess.run([str(binary)],input=protocol(reference),capture_output=True,text=True,encoding='utf8',env=env,check=True).stdout.split()
        counts=[int(probe[14]),int(probe[15])]
        paths=[['content.json','modules/emitter.json','modules/gate.json'],['deployment.json','bindings/s0.cbor','bindings/s1.cbor','resources/unmapped.preset','profiles/s0.json','profiles/s1.json','resources/s0.preset','resources/s1.preset']]
        for side,count in enumerate(counts,1):
            assert count==len(paths[side-1]),('audit hash-context map changed',side,count)
            for document in range(1,count+1):
                for mode in range(1,6):
                    crypto_cases.append((side,mode,document));payload+=protocol(reference,crypto=(side,mode,document))+protocol(reference)
                    path=paths[side-1][document-1];size=len(bytes.fromhex(next(r['hex'] for r in reference['sources'] if r['path']==path)))
                    priority_cases.append((side,mode,document,path));
        for side,mode,document,path in priority_cases:
            size=len(bytes.fromhex(next(r['hex'] for r in reference['sources'] if r['path']==path)))
            payload+=protocol(reference,path,size,crypto=(side,mode,document))+protocol(reference)
        for seam in [1,2,3]:payload+=protocol(reference,seam=seam)+protocol(reference)
        prefix_payload='';binding_fault_payload=''
        for c in [reference,next(c for c in cases if c['name']=='binding-generation-18446744073709551615')]:
            path='bindings/s0.cbor';raw=bytes.fromhex(next(r['hex'] for r in c['sources'] if r['path']==path))
            for size in range(len(raw)):
                q=copy.deepcopy(c);prefix=raw[:size];next(r for r in q['sources'] if r['path']==path)['hex']=prefix.hex();d=json.loads(bytes.fromhex(next(r['hex'] for r in q['sources'] if r['path']==q['deploymentPath'])));d['stations'][0]['bindingDigest']=hashlib.sha256(prefix).hexdigest();next(r for r in q['sources'] if r['path']==q['deploymentPath'])['hex']=json.dumps(d,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode().hex();prefix_payload+=protocol(q)+protocol(c);binding_prefixes.append((c,size))
            for offset in range(len(raw)+1):
                for crypto in [(0,0,1),(2,1,2)]:
                    binding_fault_payload+=protocol(c,path,offset,crypto=crypto)+protocol(c);binding_faults.append((c,offset,crypto))
        payload+=prefix_payload+binding_fault_payload
    r=subprocess.run([str(binary)],input=payload,capture_output=True,text=True,encoding='utf8',env=env,check=True);lines=iter(r.stdout.splitlines());fail=[]
    def check(c,line):
        x=line.split();accepted=c['expected']=='IntegrityCheckedUnauthenticated'
        if (x[0]=='A')!=accepted:fail.append((c['name'],'status',line))
        if accepted and x[0]=='A':
            expected=[c['expectedSelected'][k] for k in ['event','run','station','box','epoch']]
            content=json.loads(bytes.fromhex(next(r['hex'] for r in c['sources'] if r['path']==c['deploymentPath'])))['contentDigest']
            if x[1]!='None' or x[2]!='1' or int(x[3])!=c['sourceMetadataBytes'] or x[4:6]!=[c['deploymentDigest'],content] or x[6:11]!=expected:fail.append((c['name'],'derived program/digests/counter',line,expected))
        elif not accepted:
            if 'expectedError' in c and x[1]!=c['expectedError']:fail.append((c['name'],'error classification',line))
            if x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64:fail.append((c['name'],'partial publication',line))
            if c['expected']=='OperatingProfile' and x[1] not in ['OperatingProfile','MetadataLimit']:fail.append((c['name'],'profile classification',line))
    def source_diagnostic(path,offset,line):
        x=line.split();source=0 if path=='deployment.json' else 1 if path=='content.json' else 2 if path.startswith('modules/') else 4 if path=='resources/unmapped.preset' else 3
        module=1 if path=='modules/emitter.json' else 2 if path=='modules/gate.json' else 0
        expected=[str(source),str(module),path if source==3 else '-', 'unused' if source==4 else '-']
        if int(x[13])!=offset or x[18:22]!=expected:fail.append(('rich-source-diagnostic',path,offset,line,expected))
    for c in cases:check(c,next(lines))
    for path,offset in faults:
        line=next(lines);x=line.split()
        if x[0]!='R' or x[1] not in ['SourceFailure','Compiler'] or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64:fail.append(('source-failure',path,offset,line))
        source_diagnostic(path,offset,line)
        check(reference,next(lines))
    for side,mode,document in crypto_cases:
        line=next(lines);x=line.split()
        if x[0]!='R' or x[1]!='CryptoFailure' or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64 or int(x[15+side])==0:fail.append(('crypto',side,mode,document,line))
        check(reference,next(lines))
    for side,mode,document,path in priority_cases:
        line=next(lines);x=line.split()
        if x[0]!='R' or x[1]!='SourceFailure' or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64:fail.append(('source-priority',side,mode,document,path,line))
        size=len(bytes.fromhex(next(r['hex'] for r in reference['sources'] if r['path']==path)))
        source_diagnostic(path,size,line)
        check(reference,next(lines))
    if not a.no_faults:
        for seam in [1,2,3]:
            line=next(lines);x=line.split();expected='InvalidArgument' if seam==1 else 'SourceFailure' if seam==2 else 'Compiler'
            if x[0]!='R' or x[1]!=expected or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64:fail.append(('alias/openfalse-early-ready',seam,line))
            check(reference,next(lines))
    for c,size in binding_prefixes:
        line=next(lines);x=line.split()
        if x[0]!='R' or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64:fail.append(('CBOR-prefix',c['name'],size,line))
        check(c,next(lines))
    for c,offset,crypto in binding_faults:
        line=next(lines);x=line.split()
        if x[0]!='R' or x[1]!='SourceFailure' or x[2]!='0' or x[4]!='0'*64 or x[5]!='0'*64 or (crypto[0] and int(x[17])==0):fail.append(('CBOR-every-byte',c['name'],offset,crypto,line))
        source_diagnostic('bindings/s0.cbor',offset,line);check(c,next(lines))
    assert next(lines,None) is None
    print(len(cases),'complete raw artifact vectors;',len(faults),'source-failure/healthy-reuse pairs;',len(crypto_cases),'crypto/reuse;',len(priority_cases),'EOF-priority/reuse;',len(binding_prefixes),'CBOR-prefix/reuse;',len(binding_faults),'CBOR-everybyte-priority/reuse;',len(fail),'mismatches')
    for f in fail[:15]:print(f)
    assert not fail
if __name__=='__main__':main()
