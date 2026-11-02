"""Read-only extraction; writes only procurement evidence, never native CAD."""
from pathlib import Path
import re,json,csv,hashlib,collections
P=Path(__file__).resolve().parent;B=P.parent
pcb=B/'naehe-glut-r2b.kicad_pcb';s=pcb.read_text(encoding='utf8')
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
vias=[]
for m in re.finditer(r'^\t\(via\b.*?^\t\)',s,re.M|re.S):
 t=m.group();at=re.search(r'\(at ([\d.\-]+) ([\d.\-]+)\)',t);di=re.search(r'\(drill ([\d.]+)\)',t);sz=re.search(r'\(size ([\d.]+)\)',t);uid=re.search(r'\(uuid "([^"]+)"\)',t)
 vias.append({'uuid':uid[1],'x_mm':float(at[1]),'y_mm':float(at[2]),'drill_mm':float(di[1]),'copper_diameter_mm':float(sz[1]),'requirement':'epoxy-filled and copper-capped, all through vias'})
with (P/'VIA-FILL-INVENTORY.csv').open('w',newline='',encoding='utf8') as f:
 w=csv.DictWriter(f,fieldnames=list(vias[0]));w.writeheader();w.writerows(vias)
a=B/'pruefung/bestueckung-draft/7af39dbe807e-jq7zjlqp';rows=list(csv.DictReader((a/'CPL-DRAFT.csv').open(encoding='utf-8-sig')))
drc=json.loads((B/'pruefung/drc.json').read_text(encoding='utf8'));errors=[v for v in drc['violations'] if v['severity']=='error']
r={'status':'RFQ preparation only, not submitted or released','date':'2026-11-02','context_commit':'909e25653edafb2cd5e60ed6349356c0f905f916','input_sha256':{f.relative_to(B).as_posix():sha(f) for f in [pcb,B/'circuit.json',B/'bom.csv',a/'CPL-DRAFT.csv',a/'BOM-DRAFT.csv']},'via_count':len(vias),'via_sizes':dict(collections.Counter(str(v['drill_mm'])+'/'+str(v['copper_diameter_mm']) for v in vias)),'placement_count':len(rows),'placement_side_counts':dict(collections.Counter(r['Layer'] for r in rows)),'native_process_metadata':{'capping':re.search(r'\(capping (yes|no)\)',s)[1],'filling':re.search(r'\(filling (yes|no)\)',s)[1],'tenting_front_back':re.search(r'\(tenting\s+\(front (yes|no)\)\s+\(back (yes|no)\)',s).groups(),'material_contract':'Defaults filling/capping yes now corrected; all vias require NONCONDUCTIVE epoxy and copper cap, not merely soldermask tenting'},'open_usb_errors':errors,'outputs_sha256':{'VIA-FILL-INVENTORY.csv':sha(P/'VIA-FILL-INVENTORY.csv')}}
(P/'RFQ-INPUTS.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n',encoding='utf8');print(json.dumps({k:r[k] for k in ['via_count','via_sizes','placement_count','placement_side_counts']}))


