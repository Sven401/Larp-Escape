from pathlib import Path
import os,sys,json,uuid,hashlib,urllib.request
P=Path(__file__).resolve().parent;import websocket  # pip install websocket-client==1.9.2 in an isolated environment
u=str(uuid.uuid4());ws=websocket.create_connection('wss://tools.jlc.com/jlcTools/webSocket/'+u,timeout=30,origin='https://jlcpcb.com')
data=json.loads((P/'jlc-stackups.json').read_text(encoding='utf8'))['body']['list'];names=['2116','2116A','1080A','1080','7628','2313','3313A'];out=[]
def calc(name,case,mark,a):
 a.update(dCalculateMode=3,isLinkComputingMode=False,ZoTol=.5)
 b={'accessId':str(uuid.uuid4()),'impedance_calc_mark':mark,'paramMd5':hashlib.md5(json.dumps(a,separators=(',',':')).encode()).hexdigest(),'impedance_calc_arg':a,'uuid':u}
 ack=json.loads(urllib.request.urlopen(urllib.request.Request('https://jlcpcb.com/api/jlcTools/impedance/calc',data=json.dumps(b).encode(),headers={'Content-Type':'application/json','langType':'en'})).read());assert ack['result']=='success'
 while True:
  r=json.loads(ws.recv())
  if r.get('accessId')==b['accessId']:break
 out.append({'stackup':name,'case':case,'model':mark,'parameters_mil_and_Er':a,'result':r});(P/'calculator-host-extra.json').write_text(json.dumps(out,indent=2));print(name,case,r.get('impedance_calc_result',{}).get('dImpedance'),flush=True)

for gap in [.25,.35,.55,.782]:
 calc('3313A','L4_host_gap_'+str(gap),'DiffOffsetStripline1B1A',{'H1':.2/.0254,'Er1':4.36,'H2':(.184+.2+.03+.03)/.0254,'Er2':4.19,'W1':.15/.0254,'W2':round(.15/.0254-.8,2),'S1':gap/.0254,'T1':1.2,'W2LinkW1Incr':.8})
ws.close()
