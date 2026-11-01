"""Independent concrete single-worker ordering audit; portable prebuilt mode."""
import argparse,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def main():
 p=argparse.ArgumentParser();m=p.add_mutually_exclusive_group(required=True);m.add_argument('--compiler');m.add_argument('--binary');a=p.parse_args();env=dict(os.environ)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=ROOT/'private/rules-commit-ordering-review-quality';build.mkdir(parents=True,exist_ok=True);binary=build/('review.exe' if os.name=='nt' else 'review')
  sources=['DeviceRuntime/RulesCommitOrdering.cpp','GameExecution/RulesPlanner.cpp','GameConfiguration/PackageModules.cpp','GameConfiguration/PackageDeclarations.cpp','GameConfiguration/CanonicalPackageJson.cpp']
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'main.cpp'),*[str(ROOT/'src'/s) for s in sources],'-o',str(binary)],env=env,check=True)
 subprocess.run([str(binary)],env=env,check=True,timeout=60)
if __name__=='__main__':main()
