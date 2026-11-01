"""Actual codec/singleton/NVS sources; SDK software seam, not flash hardware."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
PRODUCTS=['include/DeviceRuntime/LocalGeneration.h','src/DeviceRuntime/LocalGeneration.cpp','lib/Esp32LocalGeneration/Esp32LocalGeneration.h','lib/Esp32LocalGeneration/Esp32LocalGeneration.cpp','lib/Esp32LocalGeneration/Esp32LocalGenerationStore.h','lib/Esp32LocalGeneration/Esp32LocalGenerationStore.cpp']
def hashes():return {n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in PRODUCTS}
def main():
 p=argparse.ArgumentParser();m=p.add_mutually_exclusive_group(required=True);m.add_argument('--compiler');m.add_argument('--binary');p.add_argument('--build-dir',default='private/local-generation-review');a=p.parse_args();env=dict(os.environ);build=ROOT/a.build_dir;build.mkdir(parents=True,exist_ok=True);before=hashes();print('SOURCE_FREEZE',json.dumps(before),flush=True)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');binary=build/('review.exe' if os.name=='nt' else 'review');names=['src/DeviceRuntime/LocalGeneration.cpp','lib/Esp32LocalGeneration/Esp32LocalGeneration.cpp','lib/Esp32LocalGeneration/Esp32LocalGenerationStore.cpp','lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp','lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp','src/Companion/Identity.cpp'];includes=['include','lib/Esp32LocalGeneration','lib/Esp32RulesBootClock','lib/Esp32DeviceIdentityStore'];subprocess.run([str(cc),'-std=c++11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE/'stubs'),*['-I'+str(ROOT/n) for n in includes],str(HERE/'main.cpp'),*[str(ROOT/n) for n in names],'-o',str(binary)],env=env,check=True)
 rows=json.loads((HERE/'records.json').read_text(encoding='utf8'));r=subprocess.run([str(binary),'codec'],input=''.join(x['raw']+' '+str(int(x['valid']))+' '+x.get('generation','0')+'\n' for x in rows),text=True,capture_output=True,env=env,timeout=30,check=True);print(r.stdout.strip());total=int(r.stdout.split()[-1])
 spec=importlib.util.spec_from_file_location('independent_physical_record',HERE/'generate.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);assert rows==g.build()
 scenarios=[('happy','Reserved'),('reentry','Reserved'),('clock-unstarted','Unavailable'),('mutex-create','Unavailable'),('mutex-take','Unavailable'),('init','Unavailable'),('missing-namespace','Missing'),('cas-change','Conflict'),('set','Uncertain'),('set-persist','Uncertain'),('commit','Uncertain'),('commit-no-effect','Uncertain'),('verify-mismatch','Uncertain'),('clock-after-write','Unavailable')]
 scenarios += [(n,'Unavailable') for n in ['partition-missing','partition-type','partition-subtype','partition-label','partition-address','partition-size','partition-erase','partition-encrypted','partition-readonly']]
 scenarios += [('open'+str(i),'Uncertain' if i==3 else 'Unavailable') for i in range(1,4)]
 for i in range(1,7):
  scenarios += [('get'+str(i),'Uncertain' if i>=5 else 'Unavailable'),('missing'+str(i),'Uncertain' if i>=5 else ('Missing' if i%2 else 'Unavailable')),('short'+str(i),'Uncertain' if i>=5 else 'Corrupt'),('long'+str(i),'Uncertain' if i>=5 else 'Corrupt')]
 observations=[];counter=2**32+5
 def run(mode,status,disk,expected):
  nonlocal total
  r=subprocess.run([str(binary),mode,str(disk),status,str(expected)],text=True,capture_output=True,env=env,timeout=30);assert r.returncode==0,(mode,status,r.stdout,r.stderr);parts=r.stdout.split();assert parts[:2]==[mode,status];total+=int(parts[2]);observations.append(r.stdout.strip());return bytes.fromhex(parts[3])
 for index,(mode,status) in enumerate(scenarios):
  disk=build/('medium-'+str(index)+'.bin');disk.write_bytes(g.record(counter));raw=run(mode,status,disk,counter+1 if status=='Reserved' else 0)
  if status=='Reserved':assert raw==g.record(counter+1),mode
 # No product provisioning: the harness pre-seeds explicit existing records.
 for gen,status in [(1,'Reserved'),(2**32-1,'Reserved'),(2**32,'Reserved'),(2**63,'Reserved'),(2**64-2,'Reserved'),(2**64-1,'Overflow'),(0,'Corrupt')]:
  disk=build/('boundary-'+str(gen)+'.bin');disk.write_bytes(g.record(gen));raw=run('happy',status,disk,gen+1 if status=='Reserved' else 0);assert raw==(g.record(gen+1) if status=='Reserved' else g.record(gen))
 disk=build/'bad-crc.bin';bad=bytearray(g.record(1));bad[60]^=1;disk.write_bytes(bad);assert run('happy','Corrupt',disk,0)==bytes(bad)
 disk=build/'bad-length.bin';disk.write_bytes(g.record(1)[:-1]);assert run('happy','Corrupt',disk,0)==g.record(1)[:-1]
 # Persist exact bytes across NEW processes with deliberately identical random Boot128.
 disk=build/'same-boot-restart.bin';disk.write_bytes(g.record(41))
 for expected in [42,43,44]:assert run('happy','Reserved',disk,expected)==g.record(expected)
 # Uncertain may have persisted or not: no process-local retry, next fresh process
 # reads whichever exact durable record the explicit software seam committed.
 for mode,start in [('set-persist',71),('commit',81),('commit-no-effect',91)]:
  disk=build/(mode+'-reopen.bin');disk.write_bytes(g.record(start));raw=run(mode,'Uncertain',disk,0);persisted=start if mode=='commit-no-effect' else start+1;assert raw==g.record(persisted);assert run('happy','Reserved',disk,persisted+1)==g.record(persisted+1)
 assert hashes()==before,'Product source changed during audit';(build/'observations.txt').write_text('\n'.join(observations)+'\n',encoding='utf8',newline='\n');print('PASS',len(rows),'fixed record vectors;',len(observations),'fresh processes;',total,'assertions; SDK_NVS_SOFTWARE_MODEL_NO_POWERLOSS_AUTHORITY_DEVICE_BINDING')
if __name__=='__main__':main()
