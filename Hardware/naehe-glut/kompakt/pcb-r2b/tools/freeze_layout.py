from pathlib import Path
from cad_environment import configure
configure()
import pcbnew as p,json,hashlib,collections
B=Path(__file__).resolve().parents[1]
def sha(f):return hashlib.sha256((B/f).read_bytes()).hexdigest()
def write(f,o):(B/f).write_text(json.dumps(o,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
def sig(t):
 a=t.GetStart();z=t.GetEnd();ends=tuple(sorted(((a.x,a.y),(z.x,z.y))))
 return (t.GetNetname(),type(t).__name__,t.GetLayer(),(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth()),ends,t.GetDrillValue() if isinstance(t,p.PCB_VIA) else 0)
def copper(board):return collections.Counter(sig(t) for t in board.GetTracks())
b=p.LoadBoard(str(B/'naehe-glut-r2b.kicad_pcb'));base=p.LoadBoard(str(B/'sources/layout-evidence/route-base.kicad_pcb'));pair=p.LoadBoard(str(B/'sources/layout-evidence/usb-paired.kicad_pcb'))
a=copper(base);z=copper(b);u=copper(pair)
def group(c):
 d=collections.Counter()
 for k,v in c.items():d[k[0]]+=v
 return dict(sorted(d.items()))
usb=lambda c:collections.Counter({k:v for k,v in c.items() if k[0].startswith('/USB_D')})
assert usb(z)==usb(u),'USB geometry changed'
drc=json.loads((B/'pruefung/drc.json').read_text());assert not drc['unconnected_items']
errors=[v for v in drc['violations'] if v['severity']=='error'];assert len(errors)==4 and all(v['type']=='hole_clearance' for v in errors)
report={'status':'routed_engineering_draft_NOT_manufacturing_release','date':'2026-11-02','hashes':{f:sha(f) for f in ['naehe-glut-r2b.kicad_pcb','circuit.json','naehe-glut-r2b.kicad_sch','bom.csv','pruefung/drc.json']},'copper_layers':b.GetCopperLayerCount(),'aux_origin_mm':[p.ToMM(b.GetDesignSettings().GetAuxOrigin().x),p.ToMM(b.GetDesignSettings().GetAuxOrigin().y)],'unconnected_items':0,'drc_counts':dict(collections.Counter(v['severity']+':'+v['type'] for v in drc['violations'])),'manual_usb_geometry_unchanged':True,'manual_usb_source_sha256':sha('sources/layout-evidence/usb-paired.kicad_pcb'),'baseline_geometry_delta':{'source':'sources/layout-evidence/route-base.kicad_pcb','source_sha256':sha('sources/layout-evidence/route-base.kicad_pcb'),'removed_by_net':group(a-z),'added_by_net':group(z-a)},'q1':{'type':'NPN MMBT3904WT1G','layer':'F.Cu','xy_mm':[26.7,14.5],'rotation_deg':0,'nets_unchanged':True},'gates':['Four original USB connector internal NPTH clearances 0.1944mm versus 0.20mm require supplier disposition','USB power-off/brownout/backfeed and host ramps require physical qualification','Confirmed stackup and USB impedance/eye qualification remain open','Filled and copper-capped through vias required','Final independent mechanical binding and circuit/pad/BOM checks required']}
write('pruefung/layout-freeze.json',report)
c=json.loads((B/'circuit.json').read_text());write('pruefung/build-state.json',{'status':report['status'],'part_count':len(c['parts']),'unrouted_new_service_nets':False,'baseline_routing_preserved':False,'baseline_preservation_scope':'See exact per-net track/via delta in layout-freeze.json; selected control routes and Q1 placement deliberately changed. No blanket preservation claim.','layout_freeze_sha256':sha('pruefung/layout-freeze.json'),'pcb_sha256':report['hashes']['naehe-glut-r2b.kicad_pcb']})
write('pruefung/routing-progress.json',{'status':'connected_and_deadends_removed','unconnected_items':0,'drc_counts':report['drc_counts'],'pcb_sha256':report['hashes']['naehe-glut-r2b.kicad_pcb']})
m=json.loads((B/'mechanical-interface.json').read_text());m['pcb_sha256']=report['hashes']['naehe-glut-r2b.kicad_pcb'];m['status']='Final routed geometry binding; independent fit report remains authoritative';write('mechanical-interface.json',m)
print(json.dumps(report,indent=2))



