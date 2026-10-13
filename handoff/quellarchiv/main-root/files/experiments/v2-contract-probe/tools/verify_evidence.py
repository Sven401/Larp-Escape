"""Verify actual captured lab replies against independent ESP serial values."""
import pathlib, json, re, hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1];PRODUCT=ROOT.parents[1];OUT=ROOT/'evidence'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
manifest=json.loads((ROOT/'firmware-source-manifest.json').read_text(encoding='utf-8-sig'))
for item in manifest['files']:
    path=item['path']
    local=ROOT/'include'/pathlib.Path(path).name if path.startswith('test/') else ROOT/'firmware'/path
    assert sha(local)==item['sha256'],f'Frozen source changed: {path}'
host=(OUT/'host-contract.txt').read_text(encoding='utf-8-sig')
assert host.count('PASS:')==2 and 'FAIL:' not in host
log=(OUT/'handy-v2.txt').read_text(encoding='utf-8')
pids=re.findall(r'I/LarpEspDiagnostic\(\s*(\d+)\)',log);assert pids
pid=pids[-1]
lines=[line for line in log.splitlines() if re.search(r'I/LarpEspDiagnostic\(\s*'+pid+r'\)',line)]
log='\n'.join(lines)+'\n';(OUT/'handy-v2-final.txt').write_text(log,encoding='utf-8')
assert 'V2 VERTRAG PASS' in log and 'V2 RECONNECT PASS' in log and 'V2 VERTRAG FAIL' not in log
replies=[json.loads(line.split('V2 REPLY ',1)[1]) for line in lines if 'V2 REPLY {' in line]
statuses=[json.loads(line.split('V2 STATUS ',1)[1]) for line in lines if 'V2 STATUS {' in line]
expected=['executed','executed','request_conflict','stale_configuration','wrong_boot','out_of_order','stale_state','executed','unauthorized']
assert [r['outcome'] for r in replies]==expected
assert [r['request'] for r in replies]==[1,1,1,2,3,4,3,4,1]
assert [r['nextRequest'] for r in replies]==[2,2,2,3,3,3,4,5,1]
assert [r['consumed'] for r in replies]==[True,True,False,True,False,False,True,True,False]
assert [r['duplicate'] for r in replies]==[False,True,False,False,False,False,False,False,False]
assert dict(replies[0],duplicate=True)==replies[1]
assert len({r['boot'] for r in replies})==1
assert any(s['control']['nextRequest']==5 and not s['game']['running'] for s in statuses)
assert any(s['game']['running'] for s in statuses)
assert statuses[0]['boot']==statuses[-1]['boot']==replies[0]['boot']
assert statuses[0]['control']['lease']!=statuses[-1]['control']['lease']
assert replies[-1]['lease']==replies[0]['lease']
for s in statuses:
    assert set(s)=={'schemaVersion','capabilities','boot','sample','sampledMilliseconds','application','control','game','devices'}
    assert s['schemaVersion']==2 and s['capabilities']==['status.configured.read','game.control.core']
    assert len(s['devices'])==7
serial='\n'.join((OUT/name).read_text(encoding='utf-8') for name in ['esp-v2-final.txt','esp-v2-reconnect.txt'])
measured=[tuple(m) for m in re.findall(r'V2 REPLY lease=(\d+) request=(\d+) outcome=(\w+) duplicate=(\d) consumed=(\d) next=(\d+) cfg=(\d+) state=(\d+)',serial)]
phone=[tuple(map(str,[r['lease'],r['request'],r['outcome'],int(r['duplicate']),int(r['consumed']),r['nextRequest'],r['configurationRevision'],r['stateRevision']])) for r in replies]
assert measured==phone,('Independent ESP/phone correlation failed',measured,phone)
assert not any(x in serial for x in ['Guru Meditation','Brownout detector','LAB INIT FAILED'])
source_paths=list(ROOT.rglob('*'))+list((PRODUCT/'experiments/handy-esp-diagnostic').rglob('*'))
sources={str(p.relative_to(PRODUCT)).replace('\\','/'):sha(p) for p in source_paths if p.is_file() and not set(p.parts)&{'.pio','bin','obj','evidence','artifacts','.local-backup'}}
firmware=ROOT/'artifacts/firmware.bin';apk=PRODUCT/'experiments/handy-esp-diagnostic/bin/Debug/net10.0-android/de.larpescape.hardwarediagnostic-Signed.apk'
backup=PRODUCT/'experiments/esp-ble-diagnostic/.local-backup/20261010-original-app0.bin'
assert len(firmware.read_bytes())<=0x140000
assert len(backup.read_bytes())==0x140000 and sha(backup)=='39ccf18a0079de78085ab1b09defd05f918d82dabdf8aeccd3743dd3c45442ac'
result={'date':'2026-10-10','status':'Measured basic v2 contract PASS; lab fixtures only','source_head':manifest['source_head'],
    'sources_sha256':sources,'firmware_sha256':sha(firmware),'firmware_bytes':firmware.stat().st_size,'apk_sha256':sha(apk),
    'original_app0_sha256':sha(backup),'phone':'Moto G14 Android14 API34','boot':replies[0]['boot'],
    'status_samples':[{'sample':s['sample'],'sampledMilliseconds':s['sampledMilliseconds'],'lease':s['control']['lease'],'nextRequest':s['control']['nextRequest'],'running':s['game']['running'],'stateRevision':s['game']['stateRevision']} for s in statuses],
    'replies':replies,'independent_serial_reply_count':len(measured),'host_checks':'actual core + actual outbound frames + fragment/parser negatives',
    'limits':['synthetic device owners','injected lab authority; no peer authentication','RAM journal; no durable storage','own BLE service; no canonical SDK/GATT validation','no endurance or supply-voltage measurement'],
    'evidence_sha256':{p.name:sha(p) for p in OUT.iterdir() if p.is_file() and p.name!='v2-contract-manifest.json'}}
(OUT/'v2-contract-manifest.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(f'PASS: {len(replies)} real BLE replies correlated with ESP; {len(statuses)} full statuses; {len(manifest["files"])} frozen sources verified')
