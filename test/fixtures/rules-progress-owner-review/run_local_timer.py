"""Full actual artifact -> runtime source-identity correlation probes."""
import argparse,importlib.util,json,os,subprocess,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
def main():
 p=argparse.ArgumentParser();m=p.add_mutually_exclusive_group(required=True);m.add_argument('--compiler');m.add_argument('--binary');p.add_argument('--owner-source');p.add_argument('--artifact-binary',required=True);p.add_argument('--case',choices=['local','timer']);p.add_argument('--build-dir',default='private/rules-progress-owner-local-timer');a=p.parse_args();env=dict(os.environ);build=ROOT/a.build_dir;build.mkdir(parents=True,exist_ok=True);binary=build/'review.exe'
 names=['GameConfiguration/CanonicalPackageJson.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/PackageModules.cpp','GameExecution/RulesPlanner.cpp','GameExecution/ManualOverrideOwner.cpp','GameExecution/RulesProgressCodec.cpp','DeviceRuntime/ProgressJournal.cpp','DeviceRuntime/RulesBundleJournal.cpp','DeviceRuntime/RulesProgressControl.cpp','DeviceRuntime/RulesProgressEffect.cpp','DeviceRuntime/RulesProgressTransaction.cpp','DeviceRuntime/RulesCommitOrdering.cpp','Companion/D0Fact.cpp','Companion/D0FactWriter.cpp','CompanionIntegration/RulesSignedEvidence.cpp']
 owner=Path(a.owner_source).resolve() if a.owner_source else ROOT/'src/DeviceRuntime/RulesProgressOwner.cpp';print('ACTUAL_OWNER_SHA',hashlib.sha256(owner.read_bytes()).hexdigest(),flush=True)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','')
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'LocalTimer.cpp'),*[str(ROOT/'src'/n) for n in names],str(owner),'-lbcrypt','-o',str(binary)],env=env,check=True)
 spec=importlib.util.spec_from_file_location('actual_artifact_protocol',HERE.parent/'manual-override-v2-review/run.py');protocol=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol);cases=json.loads((HERE/'local-timer-artifacts.json').read_text(encoding='utf8'));r=subprocess.run([str(Path(a.artifact_binary).resolve())],input=''.join(protocol.protocol(c) for c in cases),text=True,capture_output=True,check=True,env=env);lines=iter(r.stdout.splitlines());catalogs=[]
 spec=importlib.util.spec_from_file_location('canonical_oracle',HERE.parent/'signed-rules-evidence-independent/Oracle.py');oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 for case in cases:
  status=next(lines).split();next(lines);row=json.loads(next(lines));assert status[:2]==['A','None'],(case['name'],status);assert oracle.encode(row['model']).hex()==row['catalog'];catalogs.append(row['catalog'])
 assert next(lines,None) is None
 preserved=json.loads((HERE/'local-timer-catalogs.json').read_text(encoding='utf8'));assert hashlib.sha256((HERE/'local-timer-artifacts.json').read_bytes().replace(b'\r\n',b'\n')).hexdigest()==preserved['sourceArtifactsLF_SHA256'];assert catalogs==preserved['catalogs']
 outcomes=[]
 for kind,catalog in zip(['local','timer'],catalogs):
  if a.case and a.case!=kind:continue
  r=subprocess.run([str(binary),kind],input=catalog+'\n',text=True,capture_output=True,env=env);(build/(kind+'-output.txt')).write_text(r.stdout+r.stderr,encoding='utf8',newline='\n');print(kind,r.returncode,r.stdout,r.stderr);outcomes.append(r.returncode)
 assert not any(outcomes),outcomes
if __name__=='__main__':main()
