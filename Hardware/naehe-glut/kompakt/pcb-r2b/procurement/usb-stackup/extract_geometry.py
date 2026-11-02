from pathlib import Path
import os,json,math,collections,hashlib
P=Path(__file__).resolve().parent;B=P.parents[1];cfg=P/'kicad-config';cfg.mkdir(exist_ok=True);os.environ['KICAD_CONFIG_HOME']=str(cfg)
import pcbnew as p
b=p.LoadBoard(str(B/'naehe-glut-r2b.kicad_pcb'));nets={}
for t in b.GetTracks():
 if not t.GetNetname().startswith('/USB_D') or isinstance(t,p.PCB_VIA):continue
 a=t.GetStart();z=t.GetEnd();nets.setdefault(t.GetNetname(),[]).append({'a':[p.ToMM(a.x),p.ToMM(a.y)],'b':[p.ToMM(z.x),p.ToMM(z.y)],'w':p.ToMM(t.GetWidth()),'layer':b.GetLayerName(t.GetLayer()),'length':p.ToMM(t.GetLength())})
summary={}
for n,ts in nets.items():
 d=collections.defaultdict(float)
 for t in ts:d[t['layer']+' width '+str(t['w'])]+=t['length']
 summary[n]=dict(d)
pairs=[]
for suffix in ['MCU','ISO','HOST']:
 for a in nets['/USB_DM_'+suffix]:
  for z in nets['/USB_DP_'+suffix]:
   if a['layer']!=z['layer']:continue
   ux=a['b'][0]-a['a'][0];uy=a['b'][1]-a['a'][1];ln=math.hypot(ux,uy);ux/=ln;uy/=ln
   vx=z['b'][0]-z['a'][0];vy=z['b'][1]-z['a'][1]
   if abs(ux*vy-uy*vx)>1e-4:continue
   proj=[(q[0]-a['a'][0])*ux+(q[1]-a['a'][1])*uy for q in [z['a'],z['b']]];overlap=min(ln,max(proj))-max(0,min(proj));center=abs((z['a'][0]-a['a'][0])*uy-(z['a'][1]-a['a'][1])*ux)
   if overlap>.1 and center<2:pairs.append({'section':suffix,'layer':a['layer'],'widths':[a['w'],z['w']],'edge_gap_mm':round(center-(a['w']+z['w'])/2,5),'overlap_mm':round(overlap,5),'dm':a['a'],'dp':z['a']})
r={'pcb_sha256':hashlib.sha256((B/'naehe-glut-r2b.kicad_pcb').read_bytes()).hexdigest(),'segments':nets,'width_length_summary':summary,'parallel_pair_screen':pairs,'limit':'Parallel overlap screen only; corners, pad necks, vias and stubs not impedance-qualified'};(P/'usb-geometries.json').write_text(json.dumps(r,indent=2));print(json.dumps(summary));print(json.dumps(pairs))
