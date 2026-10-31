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
    p=argparse.ArgumentParser(__doc__);b=p.add_mutually_exclusive_group(required=True);b.add_argument('--compiler');b.add_argument('--binary');p.add_argument('--build-dir',default='private/manual-override-v2-review');p.add_argument('--no-faults',action='store_true');a=p.parse_args();env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/'cli.exe';sources=[ROOT/'src'/f for f in ['GameConfiguration/CanonicalPackageJson.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/PackageModules.cpp','GameConfiguration/PackageMetadata.cpp','GameConfiguration/ProfileReader.cpp','GameConfiguration/ProfileValidation.cpp','GameExecution/RulesPlanner.cpp','GameExecution/PackageArtifacts.cpp']];subprocess.run([str(cc),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'cli.cpp'),*map(str,sources),'-lbcrypt','-o',str(binary)],check=True,env=env)
    cases=json.loads((HERE/'vectors.json').read_text(encoding='utf8'));payload=''.join(protocol(c) for c in cases);faults=[];reference=cases[0]
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
    for c in cases:
        line=next(lines);check(c,line);program=json.loads(next(lines))
        if c['expected']=='IntegrityCheckedUnauthenticated' and line.startswith('A '):
            expected=c['expectedProgram'];rules={r['name']:r for r in program['rules']}
            if program['language']!=expected['language'] or program['manualOverride']!=expected['manualOverride'] or program['bindingGeneration']!=expected.get('bindingGeneration',1):fail.append((c['name'],'typed program version',program))
            for group in expected['groups']:
                original=rules.get(group['original'],{});replacement=rules.get(group['replacement'],{})
                if original.get('group')!=group['original'] or replacement.get('group')!=group['original'] or original.get('fallback')!=group['replacement'] or replacement.get('fallback') or original.get('stable')!=group['stable'] or replacement.get('stable')!=group['stable'] or original.get('generation')!=group['originalGeneration'] or replacement.get('generation')!=group['replacementGeneration']:fail.append((c['name'],'derived group/stable/generation',program,group))
            kernel=program['kernelMisuseOnly']
            if not kernel['defaultAdmissionRejected'] or kernel['emptyEpochAttempts']!=kernel['emptyEpochRejects'] or (c['name']=='valid-empty-unselected-epoch-list' and kernel['emptyEpochRejects']!=4):fail.append((c['name'],'negative kernel membership/admission',kernel))
            if 'permissions' in expected and program['permissions']!=expected['permissions']:fail.append((c['name'],'actual CBOR permission masks',program,expected['permissions']))
    assert next(lines,None) is None
    print(len(cases),'complete raw artifact vectors;',len(faults),'source-failure/healthy-reuse pairs;',len(fail),'mismatches')
    for f in fail[:15]:print(f)
    assert not fail
if __name__=='__main__':main()
