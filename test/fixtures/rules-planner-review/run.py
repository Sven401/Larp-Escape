"""Independent module-to-RAM execution of the six unchanged P0 reference traces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]

def protocol(package,trace):
    m=package['module']
    lines=[package['content'],m['moduleId'],m['version'],m['digest'],m['bytes'],f"{package.get('proofCapacity',8192)} {package.get('receiptCapacity',1024)} {int(package.get('expectBuilderRejected',False))}",str(len(package['ruleBindings']))]
    for index,name in enumerate(package['ruleBindings']):lines.extend([name,str(package.get('ruleIdentities',{}).get(name,index+1))])
    lines.append(str(len(package['subjects'])))
    for subject in package['subjects']:lines.extend([subject['name'],'1' if subject['local'] else '0'])
    content=json.loads(package['content'])
    ports=[p for station in content['stations'] if station['id']=='s' for p in station['ports'] if p['direction']=='output']
    lines.append(str(len(ports)))
    for index,port in enumerate(ports):
        resources=[r['id'] for r in content['resources']]
        lines.extend([port['id'],f"{package.get('exclusiveKeys',{}).get(port['id'],index+1)} {len(resources)}",*resources])
    lines.append(str(len(trace['steps'])))
    operations=sorted({s['input']['event']['id'] for s in trace['steps'] if 'event' in s['input']})
    for step in trace['steps']:
        raw=step['input'];event=raw.get('event');time=raw['at']
        if event:
            kind=1 if event['kind']=='local' else 2
            sequence=event.get('sequence',operations.index(event['id'])+1)
            value=event.get('value');known=int(value is not None)
            lines.extend([f"{time} {kind} {sequence} {known} {int(value or 0)} {int(event.get('accepted',True))} {int(event.get('fault',0))}",event.get('port',event.get('subject','')),event['id']])
        else:lines.extend([f'{time} 0 0 0 0 1','',''])
    return '\n'.join(lines)+'\n'

def main():
    parser=argparse.ArgumentParser(__doc__)
    backend=parser.add_mutually_exclusive_group(required=True)
    backend.add_argument('--compiler')
    backend.add_argument('--binary')
    parser.add_argument('--budget-binary')
    parser.add_argument('--build-dir',default='private/rules-planner-review')
    args=parser.parse_args();env=dict(os.environ)
    if args.binary:binary=Path(args.binary).resolve()
    else:
        compiler=Path(args.compiler).resolve();env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
        build=Path(args.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/'cli.exe'
        sources=[ROOT/'src/GameConfiguration'/n for n in ['CanonicalPackageJson.cpp','PackageDeclarations.cpp','PackageModules.cpp']]+[ROOT/'src/GameExecution/RulesPlanner.cpp']
        subprocess.run([str(compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'cli.cpp'),*[str(s) for s in sources],'-lbcrypt','-o',str(binary)],env=env,check=True)
    budget_binary=Path(args.budget_binary).resolve() if args.budget_binary else None
    if args.compiler:
        budget_binary=build/'budget.exe'
        subprocess.run([str(compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'budget.cpp'),*[str(s) for s in sources],'-lbcrypt','-o',str(budget_binary)],env=env,check=True)
    if budget_binary:subprocess.run([str(budget_binary)],env=env,check=True)
    source_lf=(ROOT/'docs/spielpakete/fixtures/p0-v1.json').read_bytes().replace(b'\r\n',b'\n')
    source_sha256=hashlib.sha256(source_lf).hexdigest()
    assert source_sha256=='58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605',source_sha256
    metadata=json.loads((HERE/'reference-traces.json').read_text(encoding='utf8'))
    assert metadata['sourceSha256']==source_sha256 and metadata['sourceHashNormalization']=='CRLF-to-LF only'
    packages=json.loads((HERE/'reference-packages.json').read_text(encoding='utf8'))
    goldens=json.loads((HERE/'reference-traces.json').read_text(encoding='utf8'))['traces']
    assert [p['id'] for p in packages]==[t['id'] for t in goldens]
    result=subprocess.run([str(binary)],input=''.join(protocol(p,t) for p,t in zip(packages,goldens)),capture_output=True,text=True,encoding='utf8',env=env)
    assert result.returncode==0,(result.returncode,result.stderr,result.stdout[-2000:])
    lines=iter(result.stdout.splitlines());count=0
    for trace in goldens:
        for i,step in enumerate(trace['steps']):
            actual=json.loads(next(lines));actual.pop('audit',None);expected=dict(step['state'],status=step['status'])
            assert actual==expected,(trace['id'],i,actual,expected)
            count+=1
    assert next(lines,None) is None
    print(len(goldens),'unchanged reference traces /',count,'full state transitions passed through real module compiler, owned program, planner and RAM commit')

    scenarios=json.loads((HERE/'proof-scenarios.json').read_text(encoding='utf8'))
    result=subprocess.run([str(binary)],input=''.join(protocol(c['package'],c['trace']) for c in scenarios),capture_output=True,text=True,encoding='utf8',env=env)
    assert result.returncode==0,(result.returncode,result.stderr,result.stdout[-2000:])
    lines=iter(result.stdout.splitlines())
    for case in scenarios:
        records=[json.loads(next(lines)) for _ in case['trace']['steps']]
        final=records[-1]
        expected=case['expected'];error=final['audit']['error']
        if expected=='BuilderRejected':
            assert final['status']=='builder-rejected' and error!=0,(case['name'],final)
            continue
        if expected in ['FactBudget','Overflow','Conflict','Rejected','Budget','Stale','InvalidSnapshot']:
            assert final['status']=='rejected',(case['name'],final)
            if expected!='Rejected':assert error=={'FactBudget':8,'Overflow':5,'Conflict':4,'Budget':6,'Stale':11,'InvalidSnapshot':10}[expected],(case['name'],final)
        elif expected=='Deferred':assert final['status']=='deferred' and error in [7,13],(case['name'],final)
        else:
            assert final['status'] in ['applied','advanced'] and error==0,(case['name'],final)
            if 'expectedProof' in case:
                key='dependencies' if case['purpose']=='emit' else 'intentEvidence' if case['purpose']=='effect' else 'latchEvidence'
                assert final['audit'][key][-1]==case['expectedProof'],(case['name'],final['audit'],case['expectedProof'])
        if expected=='ReadyReceiptOnly':assert final['emissions']==[] and final['latches']==[],(case['name'],final)
        if 'emissionProofs' in case:assert final['audit']['dependencies']==case['emissionProofs'],(case['name'],final)
        if 'unionRefs' in case:assert len(set().union(*map(set,final['audit']['dependencies'])))==case['unionRefs'],case['name']
        for field in ['intents','latches','emissions','values','timers','operations']:
            if field in case:assert final[field]==case[field],(case['name'],field,final)
        if 'ownKnown' in case:assert final['facts']==case['ownKnown'],(case['name'],final)
        if 'receipt' in case:assert final['operations'][-1]==case['receipt'],(case['name'],final)
    assert next(lines,None) is None
    print(len(scenarios),'independent proof/limit/fence/security scenarios passed through compiler/program/planner/RAM chain')

if __name__=='__main__':main()
