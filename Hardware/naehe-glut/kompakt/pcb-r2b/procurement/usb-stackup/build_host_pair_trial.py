from pathlib import Path
import os,json,hashlib,shutil,collections
P=Path(__file__).resolve().parent;B=P.parents[1];D=P/'host-pair-trial';cfg=D/'kicad-config';cfg.mkdir(parents=True,exist_ok=True);os.environ['KICAD_CONFIG_HOME']=str(cfg)
import pcbnew as p
src=B/'procurement/clearance-screen/screen.kicad_pcb'
assert hashlib.sha256(src.read_bytes()).hexdigest()=='7af39dbe807ed82f905622cce0c6b93f2cd650c61eafb224970e9f49eec15868','Historical source changed'
b=p.LoadBoard(str(src));removed=[]
for t in list(b.GetTracks()):
 if not isinstance(t,p.PCB_VIA) and t.GetNetname()=='/USB_DP_HOST' and t.GetLayer()==p.In3_Cu and p.ToMM(t.GetLength())>2:
  removed.append(t);b.Remove(t)
assert len(removed)==1
pts=[(8.25,34.8),(8.238366,34.092769),(9.083564,33.111737),(9.85,32.575)]
for a,z in zip(pts,pts[1:]):
 t=p.PCB_TRACK(b);t.SetStart(p.VECTOR2I(p.FromMM(a[0]),p.FromMM(a[1])));t.SetEnd(p.VECTOR2I(p.FromMM(z[0]),p.FromMM(z[1])));t.SetWidth(p.FromMM(.15));t.SetLayer(p.In3_Cu);t.SetNet(b.FindNet('/USB_DP_HOST'));t.SetLocked(True);b.Add(t)
out=D/'host-pair.kicad_pcb';p.SaveBoard(str(out),b);shutil.copyfile(B/'naehe-glut-r2b.kicad_pro',D/'host-pair.kicad_pro')
# Resolve local libraries for isolated DRC without altering the bound table.
s=(B/'fp-lib-table').read_text(encoding='utf-8-sig').replace('${KIPRJMOD}',str(B).replace('\\','/'));(D/'fp-lib-table').write_text(s,encoding='utf-8')
r={'status':'isolated proposal, not promoted','source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'trial_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'change':'replace exactly one DP_HOST In3 segment with three .15mm segments; DM and all vias unchanged','dp_vertices_mm':pts,'source_dp_segment_length_mm':p.ToMM(removed[0].GetLength()),'new_dp_segment_length_mm':sum(((z[0]-a[0])**2+(z[1]-a[1])**2)**.5 for a,z in zip(pts,pts[1:])),'paired_middle_center_spacing_mm':.3,'paired_middle_edge_gap_mm':.15,'stackup_candidate':'JLC061011-3313A; not a stackup/USB approval'}
(D/'TRIAL.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
