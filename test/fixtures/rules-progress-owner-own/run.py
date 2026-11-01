"""Own bounded runtime smoke; synthetic authorization, real SHA/storage, no T2 signature claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary');p.add_argument('--compiler');p.add_argument('--build-dir',default='private/rules-progress-owner-own');a=p.parse_args();env=dict(os.environ)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=(ROOT/a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/'owner.exe'
  sources=['DeviceRuntime/RulesCommitOrdering.cpp','DeviceRuntime/RulesProgressOwner.cpp','DeviceRuntime/RulesBundleJournal.cpp','DeviceRuntime/RulesProgressControl.cpp','DeviceRuntime/RulesProgressTransaction.cpp','DeviceRuntime/RulesProgressEffect.cpp','DeviceRuntime/ProgressJournal.cpp','GameExecution/ManualOverrideOwner.cpp','GameExecution/RulesPlanner.cpp','GameExecution/RulesProgressCodec.cpp','GameConfiguration/PackageModules.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/CanonicalPackageJson.cpp']
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'main.cpp'),*[str(ROOT/'src'/s) for s in sources],'-lbcrypt','-o',str(binary)],env=env,check=True,timeout=180)
 source=json.loads((HERE.parent/'manual-owner-review/catalog.json').read_text(encoding='utf-8'))
 assert hashlib.sha256(bytes.fromhex(source['canonicalCatalog'])).hexdigest()==source['sha256']
 subprocess.run([str(binary)],input=source['canonicalCatalog']+'\n',text=True,env=env,check=True,timeout=120)
if __name__=='__main__':main()
