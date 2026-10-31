"""Independent Effect byte/header audit, portable --binary mode."""
import argparse,json,os,subprocess
from pathlib import Path
from generate import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler');p.add_argument('--binary');a=p.parse_args();env=dict(os.environ)
 rows=json.loads((HERE/'vectors.json').read_text(encoding='utf8'));assert rows==build(),'fixed independent vectors changed'
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');builddir=ROOT/'private/rules-effect-independent-review';builddir.mkdir(exist_ok=True);binary=builddir/'effect.exe'
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'main.cpp'),str(ROOT/'src/DeviceRuntime/RulesProgressEffect.cpp'),'-o',str(binary)],env=env,check=True)
 data=''.join(f"{int(v['valid'])} {v.get('phase',0)} {v.get('result',0)} {v.get('index',0)} {v['hex']}\n" for v in rows)
 subprocess.run([str(binary)],input=data,text=True,env=env,check=True,timeout=60)
if __name__=='__main__':main()
