"""Actual counter/NVS sources with explicit SDK seams; no first provision or hardware proof."""
import argparse,os,subprocess
from pathlib import Path
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary');p.add_argument('--compiler');a=p.parse_args();here=Path(__file__).resolve().parent;root=here.parents[2];env=dict(os.environ)
 if a.binary:exe=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');d=root/'private/local-generation-own';d.mkdir(parents=True,exist_ok=True);exe=d/'own.exe'
  includes=[here/'stubs',root/'test/fixtures/rules-boot-clock-own/stubs',root/'include',root/'lib/Esp32LocalGeneration',root/'lib/Esp32RulesBootClock',root/'lib/Esp32DeviceIdentityStore'];sources=[here/'main.cpp',root/'src/DeviceRuntime/LocalGeneration.cpp',root/'src/Companion/Identity.cpp',root/'lib/Esp32LocalGeneration/Esp32LocalGeneration.cpp',root/'lib/Esp32LocalGeneration/Esp32LocalGenerationStore.cpp',root/'lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp',root/'lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp']
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror',*['-I'+str(x) for x in includes],*[str(x) for x in sources],'-o',str(exe)],env=env,check=True,timeout=90)
 for case in ['good','max','missing','corrupt','overflow','partition','locked','initfail','open1','open2','open3','readfail','conflict','setfail','commitfail','mismatch']:
  subprocess.run([str(exe),case],env=env,check=True,timeout=10)
if __name__=='__main__':main()
