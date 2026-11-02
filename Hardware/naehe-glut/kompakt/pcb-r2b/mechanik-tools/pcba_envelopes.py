"""Placement-bound PCBA collision screening; explicit evidence classes, no stock STEP approval."""
from pathlib import Path
import json,csv,hashlib
import FreeCAD as A,Part
BASE=Path(__file__).resolve().parents[1];OUT=BASE/'mechanik';V=A.Vector

def build():
 data=json.loads((OUT/'pcb-placement.json').read_text());pcb=BASE/'naehe-glut-r2b.kicad_pcb'
 assert hashlib.sha256(pcb.read_bytes()).hexdigest()==data['pcb_sha256'],'Re-extract changed PCB first'
 bom={r['Reference']:r for r in csv.DictReader((BASE/'bom.csv').open(encoding='utf-8-sig'))}
 refs={};rows=[];access={}
 sources={
 'C16':{'url':'https://search.kemet.com/download/specsheet/T530X477M006ATE010','revision':'generated2026-10-15','max_body_mm':[7.6,4.6,4.3]},
 'L1':{'url':'https://www.coilcraft.com/getmedia/8245f050-f190-4295-8c41-7c03d662ee3d/xel4030.pdf','revision':'1321-3,2026-02-19','max_body_mm':[4.3,4.3,3.1]},
 'L2':{'url':'https://www.coilcraft.com/getmedia/50632d43-da1b-4cdb-8ab4-3029cab51df3/xfl4020.pdf','revision':'745-3,2026-03-10','max_body_mm':[4.3,4.3,2.1]},
 'U1':{'url':'https://documentation.espressif.com/esp32-s3-mini-1_mini-1u_datasheet_en.html','revision':'v1.7','max_body_mm':[15.6,20.7,2.55]},
 'J1':{'url':'https://www.jst-mfg.com/product/pdf/eng/ePH.pdf','revision':'PH catalog p4','nominal_body_mm':[7.95,5,6.6],'note':'No catalog outside tolerance; use full library Fab bounding rectangle plus0.3mm and7mm height as engineering allowance.'},
 'SW1_SW2':{'url':'https://www.ckswitches.com/media/1479/kmr2.pdf','revision':'VL2026-01-28','max_body_mm':[4.4,3.0,2.1],'mpn':'KMR221NGLFS'},
 'JP1':{'url':'https://suddendocs.samtec.com/prints/tsw-xxx-xx-xxx-x-xx-xxx-mkt.pdf','revision':'DS','note':'TSW-102-07-G-S,2.54REFbase,5.842post;6.4x3.2mm XY engineering allowance includes cut flash.'},
 'JP1_cap':{'url':'https://suddendocs.samtec.com/prints/snt-100-xx-x-x-mkt.pdf','revision':'S','mpn':'SNT-100-BK-T','nominal_REF_mm':[2.54,5.08,6.10],'maximum_breakoff_vestige_mm':0.46,'engineering_bound_mm':[3.4,6.0,7.0]}}
 for key,digest in {'C16': '2e5eeedb4b4e0d96776893dd1b193c469cfd6c6e09dce2f2b975550763e34678', 'L1': '9764cf540f92fc19453469e99eb9d1a3d2ff1757c25c97e17be82e9a4854626e', 'L2': '6dca915ac1daa47210f7708a7540221ffef4f34a88a8621c94bd50cdf48647e6', 'J1': '447624f4f2f7d37c58c1eaa7ee314ad757fe7aff48f6186491ef6f69fbc00b96', 'JP1': '047ecedcc921fb0aed7127f08b9ba0fc1200d92d33fb7ded1b311d0ed8f42063', 'JP1_cap': '04b812db965b8acb0f031f0b10a77f18371c8836cdd27e10da5a947c97cbd34f'}.items():sources[key]['sha256']=digest
 for r in data['footprints']:
  n=r['ref'];x,y=r['position_mm'];rot=r['rotation_deg'];h=2.;kind='library_Fab_plus_0.3mm_XY_and_assumed_2mm_height_SCREEN_ONLY'
  if n in ['J4','J5']:continue
  if 'fab_aabb_mm' not in r:rows.append({'ref':n,'status':'no_body_geometry_testpad_or_graphic','mpn':bom.get(n,{}).get('MPN')});continue
  x0,y0,x1,y1=r['fab_aabb_mm'];x0-=.3;y0-=.3;x1+=.3;y1+=.3
  dims={'C16':(4.6,7.6,4.3),'L1':(4.3,4.3,3.1),'L2':(4.3,4.3,2.1),'U1':(15.6,20.7,2.55),'SW1':(4.4,3.,2.1),'SW2':(4.4,3.,2.1)}
  if n in dims:
   dx,dy,h=dims[n];x0=x-dx/2-.2;x1=x+dx/2+.2;y0=y-dy/2-.2;y1=y+dy/2+.2;h+=.2;kind='manufacturer_max_plus_0.2mm_pose_solder'
  if n=='J1':h=7.;kind='manufacturer_nominal_plus_engineering_allowance_NOT_max_tolerance'
  if n=='JP1':x0=x-1.6;x1=x+1.6;y0=y-1.93;y1=y+4.47;h=2.8;kind='manufacturer_REF_plus_engineering_allowance_NOT_max_tolerance'
  z=13 if r['side']=='top' else 12-h
  refs['PCBA_'+n]=Part.makeBox(x1-x0,y1-y0,h,V(x0+25,y0+20,z))
  rows.append({'ref':n,'mpn':bom.get(n,{}).get('MPN'),'evidence':kind,'local_xy_bound_mm':[x0,y0,x1,y1],'assembly_z_mm':[z,z+h],'side':r['side']})
  if n in ['SW1','SW2']:access[n+'_2mm_insulated_tool']=Part.makeCylinder(1.,15,V(x+25,y+20,15.3))
  if n=='J1':
   # Full mated connector allowance incl PHR-2; exact relative body tolerance open.
   refs['PCBA_J1_mated']=Part.makeBox(8.9,6.0,9.2,V(x+25-4.45,y+20-3.0,2.8))
   for i,pin in enumerate(sorted(r['pads'][:2],key=lambda p:p['position_mm'][0])):
    wx,wy=pin['position_mm'];wx+=25;wy+=20;radius=8.+2*i;cz=1.8;cx=wx-radius
    start=V(wx,wy,cz);mid=V(cx+radius*2**-.5,wy,cz-radius*2**-.5);end=V(cx,wy,cz-radius)
    arc=Part.Wire([Part.Arc(start,mid,end).toShape()]);profile=Part.Wire([Part.makeCircle(.75,start,V(0,0,1))])
    curve=arc.makePipeShell([profile],True,False)
    straight=Part.makeCylinder(.75,1.0,V(wx,wy,1.8),V(0,0,1)).fuse(Part.makeCylinder(.75,cx+2,V(cx,wy,cz-radius),V(-1,0,0)))
    refs['PCBA_J1_wire_'+str(i+1)]=curve.fuse(straight)
  if n=='JP1':
   # The cap/pin union starts at the header body top; no claim of snug supplier fit.
   refs['PCBA_JP1_cap']=Part.makeBox(3.4,6.,7.,V(x+25-1.7,y+20+1.27-3.,15.54))
   access['JP1_cap_removal_10mm']=Part.makeBox(3.4,6.,10.,V(x+25-1.7,y+20+1.27-3.,22.54))
 return refs,access,{'pcb_sha256':data['pcb_sha256'],'bom_sha256':hashlib.sha256((BASE/'bom.csv').read_bytes()).hexdigest(),'sources':sources,'coverage':rows,'battery_harness':{'housing':'PHR-2','contacts':'2x SPH-002T-P0.5S','wires':['MEDI101022-08 black','MEDI101022-07 red'],'source':'https://www.medikabel.de/de/product-overview/pvc/liy-in-anlehnung-an-vde-0812/p/12','nominal_wire_od_mm':1.1,'incoming_max_od_required_mm':1.3,'static_min_bend_multiplier':5,'model_centerline_radii_mm':[8,10],'min_table_clearance_mm':1.05,'note':'MEDI LiY0.22 service candidate, each<=0.15m, not a2A release. Geometric reserve for0.20..0.22mm2 withOD<=1.5 and stationary inner bend radius<=7.25mm; Specific MEDI selection recorded in J1-KABEL-REVIEW; electrical sample qualification remains open; PSU service only, each lead<=0.15m, incoming OD<=1.3mm, R8/R10.'},'limits':['Library Fab bounds with assumed2mm height are screening only, not exact MPN qualification.','No generic STEP is used as fit evidence.','JST mate9.2mm total height is engineering envelope vs8.6mmREF; LiY MEDI wireOD must be measured<=1.3mm; geometricOD1.5mm reserve, static inner bend radii7.25/9.25mm exceed5x1.3=6.5mm. Crimp, dimensional tolerances and physical routing remain qualification.','Component pairwise screening is separate from fixture/mate clearance.']}
