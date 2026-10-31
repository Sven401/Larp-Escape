"""Run independent canonical module vectors through real BCrypt or production PSA SHA-256."""
import argparse,json,os,subprocess
from pathlib import Path

def protocol(c, content_failure=-1, module_failure=-1, hash_failure=0, hash_document=1, sink_failure=0):
    if hash_failure==0:hash_failure=c.get('hash_failure',0);hash_document=c.get('hash_document',hash_document)
    lines=[f"{len(c['sources'])} {content_failure} {module_failure} {hash_failure} {hash_document} {sink_failure}"]
    for s in c['sources']:lines.extend([s['moduleId'],s['version'],s['digest'],s['bytes']])
    return '\n'.join(lines+[c['content']])+'\n'

def main():
    p=argparse.ArgumentParser(__doc__)
    backend=p.add_mutually_exclusive_group(required=True)
    backend.add_argument('--compiler', help='Windows C++11 GCC/Clang compiler for the BCrypt backend')
    backend.add_argument('--binary', help='Externally built CLI, including the real production PSA backend')
    p.add_argument('--build-dir',default='private/package-modules-review')
    a=p.parse_args()
    here=Path(__file__).resolve().parent
    root=here.parents[2]
    env=dict(os.environ)
    if a.binary:
        binary=Path(a.binary).resolve()
    else:
        build=Path(a.build_dir).resolve()
        build.mkdir(parents=True,exist_ok=True)
        compiler=Path(a.compiler).resolve()
        env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
        binary=build/'cli.exe'
        sources=[str(root/'src/GameConfiguration'/name) for name in
                 ['CanonicalPackageJson.cpp','PackageDeclarations.cpp','PackageModules.cpp']]
        subprocess.run([str(compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic',
                        '-Werror','-I'+str(root/'include'),str(here/'cli.cpp'),
                        *sources,'-lbcrypt','-o',str(binary)],env=env,check=True)
    cases=json.loads((here/'vectors.json').read_text(encoding='utf8'));r=subprocess.run([str(binary)],input=''.join(protocol(c) for c in cases),capture_output=True,text=True,encoding='utf8',env=env,check=True);lines=iter(r.stdout.splitlines());fail=[]
    for c in cases:
        status=next(lines);count=int(next(lines));documents=[]
        for _ in range(count):station=next(lines);documents.append(next(lines))
        parts=status.split()
        if 'error' in c:assert parts[1]==c['error'],(c['name'],status)
        if c['name'] in ['parameter-out-of-range','input-wrong-direction']:
            assert parts[3]=='0' and status.endswith('/stations/0/instances/0'),(c['name'],status)
        if c['name']=='constraint-false-bound-instance':
            assert parts[3]=='1' and status.endswith('/constraints/0'),status
        if c['name']=='unused-negative-fact-guard':
            assert parts[3]=='2' and status.endswith('/rules/0'),status
        if c['name']=='module-source-changed-label':
            assert parts[1]=='DigestMismatch',status
        if status[0]!=c['expected']:fail.append((c['name'],'status',c['expected'],status))
        if c['expected']=='A' and documents!=c['stations']:fail.append((c['name'],'expanded bytes',documents,c['stations']))
    assert next(lines,None) is None
    print(len(cases),'module vectors;',len(fail),'mismatches')
    for f in fail:print(f)
    assert not fail
    reference=next(c for c in cases if c['name']=='threshold-reference')
    inputs=[];expected=[]
    for source in ['content','module']:
        data=reference['content'] if source=='content' else reference['sources'][0]['bytes']
        for offset in range(len(data.encode('utf8'))+1):
            inputs.append(protocol(reference, offset if source=='content' else -1, offset if source=='module' else -1))
            expected.append((source,offset))
    r=subprocess.run([str(binary)],input=''.join(inputs),capture_output=True,text=True,encoding='utf8',env=env,check=True)
    lines=iter(r.stdout.splitlines())
    for source,offset in expected:
        status=next(lines);count=int(next(lines));parts=status.split()
        assert parts[0]=='R' and count==0,(source,offset,status,count)
        assert parts[2]=='2' and int(parts[3])==(0 if source=='content' else 1) and int(parts[4])==offset,(source,offset,status)
    assert next(lines,None) is None
    print(len(expected),'every-byte content/module source-failure checks passed')
    inputs=[];expected=[]
    for document in [1,2]:
        for failure in range(1,6):
            inputs.extend([protocol(reference,hash_failure=failure,hash_document=document),protocol(reference)])
            expected.extend(['CryptoFailure','healthy'])
            inputs.append(protocol(reference,content_failure=len(reference['content']) if document==1 else -1,module_failure=len(reference['sources'][0]['bytes']) if document==2 else -1,hash_failure=failure,hash_document=document));expected.append('CanonicalInput')
    r=subprocess.run([str(binary)],input=''.join(inputs),capture_output=True,text=True,encoding='utf8',env=env,check=True)
    lines=iter(r.stdout.splitlines())
    for want in expected:
        status=next(lines);count=int(next(lines));parts=status.split()
        for _ in range(count):next(lines);next(lines)
        if want=='healthy':assert parts[0]=='A',status
        else:assert parts[0]=='R' and parts[1]==want and count==0,(want,status,count)
    assert next(lines,None) is None
    print(len(expected),'hash-fault, source-priority and healthy-reuse cases passed')
    inputs=[];expected=[]
    for failure in range(1,6):
        inputs.extend([protocol(reference,sink_failure=failure),protocol(reference)])
        expected.extend(['SinkFailure','healthy'])
    r=subprocess.run([str(binary)],input=''.join(inputs),capture_output=True,text=True,encoding='utf8',env=env,check=True)
    lines=iter(r.stdout.splitlines())
    for want in expected:
        status=next(lines);count=int(next(lines));parts=status.split()
        for _ in range(count):next(lines);next(lines)
        if want=='healthy':assert parts[0]=='A',status
        else:assert parts[0]=='R' and parts[1]==want and count==0,(want,status,count)
    assert next(lines,None) is None
    print(len(expected),'sink-fault/abort and healthy-reuse cases passed')
    inputs=[];expected=[]
    for source in ['content','module']:
        data=reference['content'] if source=='content' else reference['sources'][0]['bytes']
        for offset in range(len(data)):
            c=json.loads(json.dumps(reference))
            if source=='content':c['content']=data[:offset]
            else:c['sources'][0]['bytes']=data[:offset]
            inputs.append(protocol(c));expected.append((source,offset))
    # A schema failure must not mask a later failure at the source EOF boundary.
    semantic=next(c for c in cases if c['name']=='unused-rule-extra-field')
    inputs.append(protocol(semantic,module_failure=len(semantic['sources'][0]['bytes'])))
    expected.append(('module',len(semantic['sources'][0]['bytes'])))
    r=subprocess.run([str(binary)],input=''.join(inputs),capture_output=True,text=True,encoding='utf8',env=env,check=True)
    lines=iter(r.stdout.splitlines())
    for source,offset in expected:
        status=next(lines);count=int(next(lines));parts=status.split()
        assert parts[0]=='R' and parts[1]=='CanonicalInput' and count==0,(source,offset,status,count)
        assert int(parts[3])==(0 if source=='content' else 1) and int(parts[4])==offset,(source,offset,status)
    assert next(lines,None) is None
    print(len(expected),'truncation/late-source-priority checks passed')
if __name__=='__main__':main()
