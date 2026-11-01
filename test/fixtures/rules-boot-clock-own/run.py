"""Actual native boot-clock sources with SDK call seams, never entropy/hardware evidence."""
import argparse
import os
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(); p.add_argument('--binary'); p.add_argument('--compiler'); a=p.parse_args()
    here=Path(__file__).resolve().parent; root=here.parents[2]; env=dict(os.environ)
    if a.binary: binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve(); env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','')
        build=root/'private/rules-boot-clock-own';build.mkdir(parents=True,exist_ok=True);binary=build/'boot-clock.exe'
        subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(here/'stubs'),'-I'+str(root/'include'),'-I'+str(root/'lib/Esp32RulesBootClock'),'-I'+str(root/'lib/Esp32DeviceIdentityStore'),str(here/'main.cpp'),str(root/'lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp'),str(root/'lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp'),str(root/'src/Companion/Identity.cpp'),'-o',str(binary)],env=env,check=True,timeout=90)
    # Separate real processes deliberately prove that the product has no reset seam.
    for case in ['happy','zero','negative','nested','regress','negative-late']:
        subprocess.run([str(binary),case],env=env,check=True,timeout=10)
if __name__=='__main__':main()
