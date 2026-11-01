"""Actual runtime campaign; --build-only is NOT a positive runtime proof."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def source_sha(raw):return hashlib.sha256(raw.replace(b"\r\n",b"\n")).hexdigest()
def verify_packet(row):
 from cryptography.hazmat.primitives import hashes
 from cryptography.hazmat.primitives.asymmetric import ec,utils
 spec=importlib.util.spec_from_file_location('independent_cbor',HERE.parent/'signed-rules-evidence-independent/Oracle.py');oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 packet=bytes.fromhex(row['cose']);xy=bytes.fromhex(row['xy']);assert len(xy)==64 and packet[0]==0xd2
 cose=oracle.decode(packet[1:]);assert len(cose)==4 and oracle.encode(cose)==packet[1:] and cose[1]=={}
 protected=bytes.fromhex(cose[0]['b']);payload=bytes.fromhex(cose[2]['b']);signature=bytes.fromhex(cose[3]['b']);assert len(signature)==64
 cosekey=b"\xa5\x01\x02\x03\x26\x20\x01\x21\x58\x20"+xy[:32]+b"\x22\x58\x20"+xy[32:];assert len(cosekey)==77
 assert protected==b"\xa2\x01\x26\x04\x58\x20"+hashlib.sha256(cosekey).digest()
 fact=oracle.decode(payload);assert oracle.encode(fact)==payload and fact['9'][2]==row['expectedType'] and fact['9'][4] is True
 signed=oracle.encode(['Signature1',{'b':protected.hex()},{'b':b'larp.event.v1'.hex()},{'b':payload.hex()}])
 key=ec.EllipticCurvePublicNumbers(int.from_bytes(xy[:32],'big'),int.from_bytes(xy[32:],'big'),ec.SECP256R1()).public_key();key.verify(utils.encode_dss_signature(int.from_bytes(signature[:32],'big'),int.from_bytes(signature[32:],'big')),signed,ec.ECDSA(hashes.SHA256()))
 return fact,payload
def oracle_module():
 spec=importlib.util.spec_from_file_location('owner_full_bytes',HERE.parent/'signed-rules-evidence-independent/Oracle.py');out=importlib.util.module_from_spec(spec);spec.loader.exec_module(out);return out

def verify_observation(row):
 oracle=oracle_module();raw=bytes.fromhex(row['transaction']);catalog=bytes.fromhex(row['catalog']);tx=oracle.decode(raw);cat=oracle.decode(catalog)
 assert oracle.encode(tx)==raw and oracle.encode(cat)==catalog and tx['0']=='larp.rules.transaction.v1' and set(tx)==set(map(str,range(6)))
 appraw=bytes.fromhex(tx['4']['b']);app=oracle.decode(appraw);assert oracle.encode(app)==appraw and app['0']=='larp.rules.application.v1' and set(app)==set(map(str,range(8)))
 assert tx['1']==app['1'] and tx['2']==app['2'] and tx['3']==app['3'] and app['3']['b']==hashlib.sha256(catalog).hexdigest() and app['4'][0]==cat['1']
 for ref,xy,cose in app['5']+app['6']:
  fact,payload=verify_packet(dict(cose=cose['b'],xy=xy['b'],expectedType=ref[-1]));body=fact['9'];assert ref[0]==[fact['2'],fact['4'],body[0],body[1]] and ref[1]['b']==hashlib.sha256(payload).hexdigest();assert ref[2:]==[fact['8'],fact['7'],fact['3'],fact['5'],body[3],body[2]]
 authraw=bytes.fromhex(app['7']['b']);auth=oracle.decode(authraw);assert oracle.encode(auth)==authraw and auth['0']=='larp.rules.evidence.v1' and auth['2']=={'b':'a10001'} and auth['3']=={'b':'a10003'}
 return len(raw),len(appraw),len(app['5'])+len(app['6'])

def check_artifacts(binary,fixture,no_release,env):
 spec=importlib.util.spec_from_file_location('owner_actual_artifact_protocol',HERE.parent/'manual-override-v2-review/run.py');protocol=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol)
 cases=json.loads((HERE.parent/'manual-override-v2-review/vectors.json').read_text(encoding='utf8'));cases=[next(c for c in cases if c['name']==fixture['sourceArtifactFixture']),json.loads((HERE/'norelease-artifact.json').read_text(encoding='utf8'))]
 result=subprocess.run([str(binary)],input=''.join(protocol.protocol(c) for c in cases),text=True,capture_output=True,env=env,check=True,timeout=180);lines=iter(result.stdout.splitlines());actual=[];oracle=oracle_module()
 for c in cases:
  status=next(lines).split();next(lines);row=json.loads(next(lines));assert status[:2]==['A','None'],(c['name'],status);assert oracle.encode(row['model']).hex()==row['catalog'];actual.append(row['catalog'])
 assert next(lines,None) is None and actual==[fixture['canonicalCatalog'],no_release['canonicalCatalog']]

def main():
 p=argparse.ArgumentParser();m=p.add_mutually_exclusive_group(required=True);m.add_argument('--compiler');m.add_argument('--binary');p.add_argument('--build-only',action='store_true');p.add_argument('--artifact-binary');p.add_argument('--update-goldens',action='store_true');a=p.parse_args();env=dict(os.environ)
 if a.binary:
  assert not a.build_only;assert a.artifact_binary,'--binary requires actual --artifact-binary';binary=Path(a.binary).resolve();artifact=Path(a.artifact_binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=ROOT/'private/rules-progress-owner-review-quality';build.mkdir(exist_ok=True);binary=build/('review.exe' if os.name=='nt' else 'review')
  names=['GameConfiguration/CanonicalPackageJson.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/PackageModules.cpp','GameExecution/RulesPlanner.cpp','GameExecution/ManualOverrideOwner.cpp','GameExecution/RulesProgressCodec.cpp','DeviceRuntime/ProgressJournal.cpp','DeviceRuntime/RulesBundleJournal.cpp','DeviceRuntime/RulesProgressControl.cpp','DeviceRuntime/RulesProgressEffect.cpp','DeviceRuntime/RulesProgressTransaction.cpp','DeviceRuntime/RulesCommitOrdering.cpp','DeviceRuntime/RulesProgressOwner.cpp','Companion/D0Fact.cpp','Companion/D0FactWriter.cpp','CompanionIntegration/RulesSignedEvidence.cpp']
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'main.cpp'),*[str(ROOT/'src'/s) for s in names],'-lbcrypt','-o',str(binary)],env=env,check=True)
  artifact=build/'catalog.exe';names=['GameConfiguration/CanonicalPackageJson.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/PackageModules.cpp','GameConfiguration/PackageMetadata.cpp','GameConfiguration/ProfileReader.cpp','GameConfiguration/ProfileValidation.cpp','GameExecution/RulesPlanner.cpp','GameExecution/PackageArtifacts.cpp','GameExecution/RulesProgressCodec.cpp']
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE.parent/'rules-progress-independent-review/cli.cpp'),*[str(ROOT/'src'/s) for s in names],'-lbcrypt','-o',str(artifact)],env=env,check=True)
 if a.build_only:print('Actual full runtime harness LINKED ONLY; no execution / no Owner qualification');return
 fixture=json.loads((HERE/'catalog.json').read_text(encoding='utf8'));goldens=HERE.parent/'rules-progress-independent-review/catalog-goldens.json';sources=HERE.parent/'manual-override-v2-review/vectors.json'
 assert source_sha(goldens.read_bytes())==fixture['sourceCatalogGoldensSHA256'];assert source_sha(sources.read_bytes())==fixture['sourceArtifactsSHA256'];assert hashlib.sha256(bytes.fromhex(fixture['canonicalCatalog'])).hexdigest()==fixture['sha256']
 original=json.loads(goldens.read_text(encoding='utf8'))[fixture['sourceArtifactFixture']];assert original['canonical']==fixture['canonicalCatalog']
 no_release=json.loads((HERE/'norelease-catalog.json').read_text(encoding='utf8'));assert source_sha((HERE/'norelease-artifact.json').read_bytes())==no_release['sourceArtifactSHA256'];assert hashlib.sha256(bytes.fromhex(no_release['canonicalCatalog'])).hexdigest()==no_release['sha256']
 check_artifacts(artifact,fixture,no_release,env)
 result=subprocess.run([str(binary)],input=fixture['canonicalCatalog']+'\n'+no_release['canonicalCatalog']+'\n',text=True,capture_output=True,env=env,timeout=180)
 print(result.stderr,end='');(binary.parent/'owner-last-output.jsonl').write_text(result.stdout,encoding='utf8',newline='\n');assert result.returncode==0,(result.returncode,result.stderr)
 signed=0;observations=[];sizes=[]
 for line in result.stdout.splitlines():
  v=json.loads(line)
  if 'cose' in v:verify_packet(v);signed+=1
  elif 'observation' in v:sizes.append(verify_observation(v));observations.append(v)
  else:print(v)
 assert signed and observations
 path=HERE/'owner-goldens.json'
 if a.update_goldens:path.write_text(json.dumps(observations,indent=2)+'\n',encoding='utf8',newline='\n')
 preserved=json.loads(path.read_text(encoding='utf8'))
 for v in preserved:verify_observation(v)
 assert [v['observation'] for v in preserved]==[v['observation'] for v in observations]
 print(signed,'direct COSE +',sum(v[2] for v in sizes),'envelope proof/outbox occurrences independently verified;',len(observations),'full original envelope/catalog byte oracles;2 actual complete artifacts; transaction',min(v[0] for v in sizes),'..',max(v[0] for v in sizes),'B;application',min(v[1] for v in sizes),'..',max(v[1] for v in sizes),'B;SYNTHETIC_AUTHORITY_PORTS_SIMULATED_DURABLE_MEDIUM')
if __name__=='__main__':main()
