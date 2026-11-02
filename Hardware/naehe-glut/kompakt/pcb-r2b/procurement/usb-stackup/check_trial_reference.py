from pathlib import Path
import os,json,math,collections,hashlib
P=Path(__file__).resolve().parent;D=P/'host-pair-trial';B=P.parents[1];os.environ['KICAD_CONFIG_HOME']=str(D/'kicad-config')
import pcbnew as p
src=p.LoadBoard(str(B/'naehe-glut-r2b.kicad_pcb'));b=p.LoadBoard(str(D/'host-pair.kicad_pcb'))
def sig(t):
 a=t.GetStart();z=t.GetEnd();return (t.GetNetname(),type(t).__name__,t.GetLayer(),t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth(),tuple(sorted(((a.x,a.y),(z.x,z.y)))),t.GetDrillValue() if isinstance(t,p.PCB_VIA) else 0)
a=collections.Counter(sig(t) for t in src.GetTracks());q=collections.Counter(sig(t) for t in b.GetTracks())
start=(8.238366,34.092769);end=(9.083564,33.111737);dx=end[0]-start[0];dy=end[1]-start[1];length=math.hypot(dx,dy);nx=-dy/length;ny=dx/length
checks=[]
for layer in [p.In1_Cu,p.In4_Cu]:
 polys=[z.GetFilledPolysList(layer) for z in b.Zones() if not z.GetIsRuleArea() and z.GetNetname()=='/GND' and z.GetLayerSet().Contains(layer)];miss=[];count=0
 for i in range(131):
  for off in [-.375,-.30,-.225,-.15,-.075,0,.075]:
   x=start[0]+dx*i/130+nx*off;y=start[1]+dy*i/130+ny*off;v=p.VECTOR2I(p.FromMM(x),p.FromMM(y));count+=1
   if not any(poly.Contains(v) for poly in polys):miss.append([round(x,6),round(y,6)])
 checks.append({'reference_layer':b.GetLayerName(layer),'samples':count,'uncovered_count':len(miss),'uncovered_examples':miss[:10]})
r={'status':'isolated geometry screen, not impedance approval','pcb_sha256':hashlib.sha256((D/'host-pair.kicad_pcb').read_bytes()).hexdigest(),'removed_copper_objects':list((a-q).elements()),'added_copper_objects':list((q-a).elements()),'paired_middle_length_mm':length,'reference_checks':checks,'method':'131 longitudinal x7 transverse points across entire paired-copper-width envelope, frozen filled GND polygons, about10um longitudinal sampling. Not an exact polygon containment proof; vias/fanouts not covered by this middle-section screen.'}
(D/'GEOMETRY-REFERENCE.json').write_text(json.dumps(r,indent=2));print(json.dumps(checks));print('delta',sum((a-q).values()),sum((q-a).values()))
