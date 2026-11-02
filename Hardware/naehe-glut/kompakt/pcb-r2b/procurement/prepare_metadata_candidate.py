from pathlib import Path
import os,json,hashlib,collections,re,difflib
P=Path(__file__).resolve().parent;D=P/'clearance-screen';os.environ['KICAD_CONFIG_HOME']=str(D/'kicad-config')
import pcbnew as p
src=P.parent/'naehe-glut-r2b.kicad_pcb';b=p.LoadBoard(str(src));vias=[t for t in b.GetTracks() if isinstance(t,p.PCB_VIA)]
settings=b.GetDesignSettings();settings.m_FillVias=True;settings.m_CapVias=True
out=D/'metadata-candidate.kicad_pcb';p.SaveBoard(str(out),b)
report={'status':'separate candidate only; not promoted','source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'verification':'serialized textual diff below; SWIG optional bool getters not used','change':'board m_FillVias=True and m_CapVias=True only; tent/plug/cover unchanged','material':'Nonconductive epoxy must remain explicit manufacturing instruction; booleans do not identify resin'}
report['text_diff']=list(difflib.unified_diff(src.read_text().splitlines(),out.read_text().splitlines(),fromfile='frozen',tofile='candidate'))
(P/'VIA-METADATA-CANDIDATE.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

