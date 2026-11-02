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
 out.append({'stackup':name,'case':case,'model':mark,'parameters_mil_and_Er':a,'result':r});(P/'calculator-results.json').write_text(json.dumps(out,indent=2));print(name,case,r.get('impedance_calc_result',{}).get('dImpedance'),flush=True)
for n in names:
 x=next(x for x in data if x.get('appointName')=='JLC061011-'+n);ls=x['basicDataList'];core=next(l for l in ls if l['layerName']=='L4/L5');topcore=next(l for l in ls if l['layerName']=='L2/L3');i=ls.index(topcore);j=ls.index(core);middle=ls[i+1:j];lower_h=core['dielectricThick'];middle_h=sum(l['dielectricThick'] for l in middle);outer=ls[1:i];outer_h=sum(l['dielectricThick'] for l in outer);outer_er=sum(l['dielectricConstant'] for l in outer)/len(outer)
 for gap in [.15,.25,.35,.55]:
  calc(n,'outer_gap_'+str(gap),'DiffEdgeCoupledCoatedMicrostrip1B',{'H1':outer_h/.0254,'Er1':outer_er,'W1':.15/.0254,'W2':round(.15/.0254-.5,2),'S1':gap/.0254,'T1':1.6,'C1':1,'C2':.6,'C3':1,'CEr':3.8,'W2LinkW1Incr':.5})
 for upper in ['L3_hypothetical_plane','L2_ignore_intervening_signal_copper']:
  uh=middle_h if upper.startswith('L3') else middle_h+topcore['dielectricThick']+topcore['botConductorThick'];ers=[l['dielectricConstant'] for l in middle]+([] if upper.startswith('L3') else [topcore['dielectricConstant']]);er=sum(ers)/len(ers)
  calc(n,upper,'DiffOffsetStripline1B1A',{'H1':lower_h/.0254,'Er1':core['dielectricConstant'],'H2':(uh+core['topConductorThick'])/.0254,'Er2':round(er,2),'W1':.15/.0254,'W2':round(.15/.0254-.8,2),'S1':.15/.0254,'T1':1.2,'W2LinkW1Incr':.8})
ws.close()
