from pathlib import Path
from cad_environment import configure
configure()
import pcbnew as p,json,sys,collections,hashlib
source=Path(sys.argv[1]);dest=Path(sys.argv[2]);b=p.LoadBoard(str(source))
rows={}
for t in b.GetTracks():
 n=t.GetNetname()
 if not n.startswith('/USB_D'):continue
 q=rows.setdefault(n,{'track_length_mm':0.,'vias':0,'layers':{},'segments':0})
 if isinstance(t,p.PCB_VIA):q['vias']+=1
 else:
  length=p.ToMM(t.GetLength());q['track_length_mm']+=length;q['segments']+=1;layer=b.GetLayerName(t.GetLayer());q['layers'][layer]=q['layers'].get(layer,0)+length
for q in rows.values():
 q['track_length_mm']=round(q['track_length_mm'],4);q['layers']={k:round(v,4) for k,v in q['layers'].items()}
report={'status':'geometric screen only; total copper length includes connector merges/ESD branches, not propagation delay or impedance','pcb':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'nets':rows,'reference_layers':'In1.Cu and In4.Cu GND; final local return continuity and stackup require review','limitations':['Manual coupled geometry is documented in layout-freeze.json; no field-solver qualification','No impedance or eye-diagram qualification','Branch/stub and return-via geometry require explicit review']}
dest.write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(rows,indent=2))

