"""Actual production boot clock + entropy adapter against narrow SDK stubs."""
import argparse,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
CASES=['zero-time','basic','wide','maximum','last-byte-only','zero-identity','unwritten-entropy','negative-start','negative-read','micro-regression','cross-port-regression','reenter-fill','reenter-disable','reenter-start-timer','reenter-sample']
def main():
 p=argparse.ArgumentParser();m=p.add_mutually_exclusive_group(required=True);m.add_argument('--compiler');m.add_argument('--binary');a=p.parse_args();env=dict(os.environ)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=ROOT/'private/esp32-rules-boot-clock-review';build.mkdir(parents=True,exist_ok=True);binary=build/('review.exe' if os.name=='nt' else 'review')
  subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE/'stubs'),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'lib/Esp32RulesBootClock'),'-I'+str(ROOT/'lib/Esp32DeviceIdentityStore'),str(HERE/'main.cpp'),str(ROOT/'lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp'),str(ROOT/'lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp'),str(ROOT/'src/Companion/Identity.cpp'),'-o',str(binary)],env=env,check=True)
 total=0
 for case in CASES:
  result=subprocess.run([str(binary),case],capture_output=True,text=True,env=env,timeout=30);assert result.returncode==0,(case,result.stdout,result.stderr);fields=result.stdout.split();assert fields[0]==case;total+=int(fields[1])
 print(len(CASES),'fresh-process actual production boot-clock/entropy scenarios;',total,'SDK seam assertions; compile-time singleton/port ABI checks; NO_HARDWARE_ENTROPY_SLEEP_CLAIM')
if __name__=='__main__':main()
