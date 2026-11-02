"""Pre-routing R2b base generator. Final routed native CAD is the editable source."""
from pathlib import Path
import json,csv,shutil,hashlib,runpy,os,subprocess,sys
BASE=Path(__file__).resolve().parents[1]; ROOT=BASE.parents[3]; OLD=BASE/'sources/r2a-input'
LIB=ROOT/'.tmp/cad-tools/kicad/share/kicad/footprints'
(BASE/'pruefung/kicad-config').mkdir(parents=True,exist_ok=True)
os.environ['KICAD_CONFIG_HOME']=str(BASE/'pruefung/kicad-config')
import pcbnew as pcb

def write(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf-8')
def dump(p,o):write(p,json.dumps(o,indent=2,ensure_ascii=False)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def vec(x,y):return pcb.VECTOR2I(pcb.FromMM(x),pcb.FromMM(y))

def main():
 if (BASE/"pruefung/layout-freeze.json").exists() and "--allow-replace-routed-board" not in sys.argv:
  raise SystemExit("Refusing to replace frozen routed CAD with pre-routing base. Explicit override required.")
 provenance=json.loads((OLD/"PROVENANCE.json").read_text(encoding="utf-8-sig"))
 for item in provenance["files"]:
  if sha(OLD/item["path"])!=item["sha256"]:raise RuntimeError("Changed immutable input: "+item["path"])
 c=json.loads((OLD/'circuit.json').read_text(encoding='utf-8-sig')); original={p['ref'] for p in c['parts']}
 c.update(revision='R2b',status='engineering draft: service nets unrouted; NO manufacturing or operation release',board_mm=[30,37.8],assembly_xy_nominal_mm=[30,43],max_assembly_mm=None)
 c['source_r2a_sha256']={f:sha(OLD/f) for f in ['circuit.json','naehe-glut-r2a.kicad_pcb']}
 c['power_flags']=list(c['power_flags'])+['USB_VBUS']
 next(p for p in c['parts'] if p['ref']=='U1')['pins'].update({'7':'JTAG_SEL','23':'USB_DM_MCU','24':'USB_DP_MCU','35':'JTAG_TCK','36':'JTAG_TDO','37':'JTAG_TDI','38':'JTAG_TMS'})
 def part(ref,value,mpn,fp,pins,names=None,types=None,**kw):
  pins={str(k):v for k,v in pins.items()};c['parts'].append(dict(ref=ref,value=value,mpn=mpn,footprint=fp,pins=pins,pin_count=len(pins),pin_numbers=list(pins),pin_names={str(k):v for k,v in (names or {}).items()},types={str(k):v for k,v in (types or {}).items()},dnp=False,in_bom=True,status='R2b selection; supplier/land review pending',**kw))
 def r(ref,value,a,b):part(ref,value,'RC0402FR-07'+value+'L','Resistor_SMD:R_0402_1005Metric',{1:a,2:b},tolerance='1%')
 def cap(ref,value,a,b,mpn='GRM188R71C104KA01D',fp='Capacitor_SMD:C_0603_1608Metric'):part(ref,value,mpn,fp,{1:a,2:b})
 usb={'A1':'GND','A4':'USB_VBUS','A5':'USB_CC1','A6':'USB_DP_HOST','A7':'USB_DM_HOST','A8':None,'A9':'USB_VBUS','A12':'GND','B1':'GND','B4':'USB_VBUS','B5':'USB_CC2','B6':'USB_DP_HOST','B7':'USB_DM_HOST','B8':None,'B9':'USB_VBUS','B12':'GND','SH':'GND'}
 part('J4','USB-C DATA ONLY / external target supply','USB4105-GF-A','Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal',usb)
 part('J5','GPIO JTAG / VTref reference only','FTSH-105-01-L-DV-007-K','NearGlowR2b:FTSH105_007_K',{1:'+3V3',2:'JTAG_TMS_PORT',3:'GND',4:'JTAG_TCK_PORT',5:'GND',6:'JTAG_TDO_PORT',8:'JTAG_TDI_PORT',9:None,10:'CHIP_EN'},geometry_status='Land from Samtec DV footprint Rev H; body from series Rev FZ; exact mating STEP remains open')
 part('JP1','OPEN USB / CLOSED GPIO after authorized eFuse','TSW-102-07-G-S','Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical',{1:'JTAG_SEL',2:'GND'},assembly_process='THT assembly required; shunt separate')
 for ref,net in [('SW1','BOOT0'),('SW2','CHIP_EN')]:part(ref,'BOOT' if ref=='SW1' else 'RESET','KMR221NGLFS','Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2',{1:net,2:'GND'})
 part('U8','USB hardware disconnect','TS3USB221ARSER','Package_DFN_QFN:Texas_UQFN-10_1.5x2mm_P0.5mm',{1:'USB_DP_ISO',2:'USB_DM_ISO',3:None,4:None,5:'GND',6:'USB_OE_N',7:'USB_DM_HOST',8:'USB_DP_HOST',9:'GND',10:'USB_IF_3V3'},{1:'1D+',2:'1D-',3:'2D+',4:'2D-',5:'GND',6:'OE_N',7:'D-',8:'D+',9:'S',10:'VCC'},{5:'power_in',6:'input',9:'input',10:'power_in'})
 for ref,mpn,sense,ct in [('U9','TPS389001DSER','USB_TARGET_SENSE','USB_TARGET_CT'),('U10','TPS389001DSER','USB_VBUS_SENSE','USB_VBUS_CT')]:
  part(ref,'USB attach supervisor',mpn,'NearGlowR2b:TI_DSE0006A',{1:sense,2:'GND',3:'USB_IF_3V3',4:'USB_IF_3V3',5:ct,6:'USB_READY'},{1:'SENSE',2:'GND',3:'MR_N',4:'VDD',5:'CT',6:'RESET_N'},{1:'input',2:'power_in',3:'input',4:'power_in',5:'passive',6:'open_collector'})
 part('U11','USB interface ONLY / no target power','TPS70933DBVR','Package_TO_SOT_SMD:SOT-23-5',{1:'USB_VBUS',2:'GND',3:None,4:None,5:'USB_IF_3V3'},{1:'IN',2:'GND',3:'EN internally pulled HIGH',4:'NC',5:'OUT'},{1:'power_in',2:'power_in',5:'power_out'})
 part('U12','USB ready Schmitt inverter','SN74LVC1G14DCKR','Package_TO_SOT_SMD:SOT-353_SC-70-5',{1:None,2:'USB_READY',3:'GND',4:'USB_OE_N',5:'USB_IF_3V3'},{1:'NC',2:'A',3:'GND',4:'Y',5:'VCC'},{2:'input',3:'power_in',4:'output',5:'power_in'})
 for ref,dp,dm in [('D1','USB_DP_HOST','USB_DM_HOST'),('D2','JTAG_TMS_PORT','JTAG_TCK_PORT'),('D3','JTAG_TDO_PORT','JTAG_TDI_PORT'),('D4','CHIP_EN','+3V3'),('D5','USB_CC1','USB_CC2')]:part(ref,'rail-free ESD array','TPD2EUSB30DRTR','Package_TO_SOT_SMD:Texas_DRT-3',{1:dp,2:dm,3:'GND'},{1:'IO1',2:'IO2',3:'GND'})
 rs=[('5K1','USB_CC1','GND'),('5K1','USB_CC2','GND'),('22R','USB_DM_MCU','USB_DM_ISO'),('22R','USB_DP_MCU','USB_DP_ISO'),('10K','+3V3','JTAG_SEL'),('10K','+3V3','JTAG_TMS'),('10K','+3V3','JTAG_TDI'),('10K','JTAG_TCK','GND'),('33R','JTAG_TCK_PORT','JTAG_TCK'),('33R','JTAG_TMS_PORT','JTAG_TMS'),('33R','JTAG_TDI_PORT','JTAG_TDI'),('33R','JTAG_TDO','JTAG_TDO_PORT'),('200K','USB_VBUS','USB_VBUS_SENSE'),('75K','USB_VBUS_SENSE','GND'),('47K','USB_IF_3V3','USB_READY'),('10K','USB_READY','USB_READY_BASE'),('100K','USB_READY_BASE','GND'),('47K','USB_IF_3V3','USB_OE_N'),('10K','USB_IF_3V3','GND')]
 for i,args in enumerate(rs,28):
  if i not in [43,44]:r('R'+str(i),*args)
 for p in c['parts']:
  if p['ref'] in ['R40','R41']:p.update(mpn='RT0402BRD07'+('200K' if p['ref']=='R40' else '75K')+'L',tolerance='0.1%',tcr_ppm_per_K=25)
 for ref,mpn,value,a,b in [('R47','RT0402BRD07169KL','169K','+3V3','USB_TARGET_SENSE'),('R48','RT0402BRD07100KL','100K','USB_TARGET_SENSE','GND')]:part(ref,value,mpn,'Resistor_SMD:R_0402_1005Metric',{1:a,2:b},tolerance='0.1%',tcr_ppm_per_K=25)
 for ref in ['C23','C24','C25']:cap(ref,'100n','USB_IF_3V3','GND')
 for ref,a in [('C26','USB_TARGET_CT'),('C27','USB_VBUS_CT')]:cap(ref,'10n',a,'GND','GRM188R71H103KA01D')
 cap('C28','100n 16V X7R / input not stability-critical','USB_VBUS','GND')
 for ref in ['C29','C30']:cap(ref,'4.7u 16V X7R / two parallel for bias reserve','USB_IF_3V3','GND','GRM21BR71C475KE51L','Capacitor_SMD:C_0805_2012Metric')
 cap('C31','100n inverter bypass','USB_IF_3V3','GND')
 dump(BASE/'circuit.json',c)
 with (BASE/'bom.csv').open('w',newline='',encoding='utf-8') as f:
  w=csv.writer(f);w.writerow(['Reference','Value','MPN','Footprint','Quantity','Status'])
  for p in c['parts']:
   if p.get('in_bom',True):w.writerow([p['ref'],p['value'],p['mpn'],p['footprint'],1,p.get('status','inherited R2a')])
 shutil.copy2(OLD/'KICAD-LIBRARY-LICENSE.md',BASE/'KICAD-LIBRARY-LICENSE.md');shutil.copy2(OLD/'LIBRARY-SOURCES.json',BASE/'R2A-LIBRARY-SOURCES.json')
 shutil.copytree(OLD/'NearGlowR2a.pretty',BASE/'NearGlowR2a.pretty',dirs_exist_ok=True);shutil.copytree(OLD/'footprints',BASE/'footprints',dirs_exist_ok=True)
 custom=BASE/'NearGlowR2b.pretty';custom.mkdir(exist_ok=True)
 pads=[]
 for n,x,y,sx,sy in [(1,-.6,-.5,.7,.25),(2,-.6,0,.7,.25),(3,-.6,.5,.7,.25),(4,.35,.5,1.2,.25),(5,.6,0,.7,.25),(6,.6,-.5,.7,.25)]:pads.append(f'(pad "{n}" smd rect (at {x} {y}) (size {sx} {sy}) (layers "F.Cu" "F.Paste" "F.Mask"))')
 write(custom/'TI_DSE0006A.kicad_mod','(footprint "TI_DSE0006A" (version 20250108)(generator "pcbnew")(layer "F.Cu")(attr smd)(property "Reference" "REF**" (at 0 -1.5 0)(layer "F.SilkS")(effects(font(size .7 .7)(thickness .1))))(property "Value" "TPS3890" (at 0 1.5 0)(layer "F.Fab")(effects(font(size .7 .7)(thickness .1))))(fp_rect(start -.75 -.75)(end .75 .75)(stroke(width .1)(type default))(fill none)(layer "F.Fab"))(fp_rect(start -1.2 -1)(end 1.2 1)(stroke(width .05)(type default))(fill none)(layer "F.CrtYd"))'+''.join(pads)+')')
 hpads=[]
 for i,x in enumerate([-2.54,-1.27,0,1.27,2.54]):
  for n,y in [(2*i+1,2.035),(2*i+2,-2.035)]:
   if n!=7:hpads.append(f'(pad "{n}" smd rect (at {x} {y}) (size .74 2.79) (layers "F.Cu" "F.Paste" "F.Mask"))')
 header='(footprint "FTSH105_007_K" (version 20250108) (generator "pcbnew") (layer "F.Cu") (attr smd) (descr "Samtec FTSH DV footprint Rev H; no alignment holes for K; Pin7 omitted") (property "Reference" "REF**" (at 0 -4.3 0) (layer "F.SilkS") (effects (font (size .7 .7) (thickness .1)))) (property "Value" "FTSH-105-01-L-DV-007-K" (at 0 4.3 0) (layer "F.Fab") (effects (font (size .7 .7) (thickness .1)))) (fp_rect (start -3.175 -2.54) (end 3.175 2.54) (stroke (width .1) (type default)) (fill none) (layer "F.Fab")) (fp_rect (start -4 -3.68) (end 4 3.68) (stroke (width .05) (type default)) (fill none) (layer "F.CrtYd")) '+''.join(hpads)+')'
 write(custom/'FTSH105_007_K.kicad_mod',header)
 for p in c['parts']:
  lib,name=p['footprint'].split(':')
  if lib.startswith('NearGlow'):continue
  src=LIB/(lib+'.pretty')/(name+'.kicad_mod');dest=BASE/'footprints'/(lib+'.pretty')/src.name
  if src.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
 text=(OLD/'tools/build_schematic.py').read_text(encoding='utf-8').replace('NearGlowR2a','NearGlowR2b').replace('naehe-glut-r2a','naehe-glut-r2b').replace('R2a','R2b').replace('2026-11-01','2026-11-02').replace('no USB/charger onboard.','USB data service, external target supply; no charger.').replace('d582f673-9ab4-4e2e-a4d1-897ac1830bc4','49459116-5084-4c82-83b9-e20bf06f8e90')
 write(BASE/'tools/build_schematic.py',text);subprocess.run([sys.executable,str(BASE/'tools/build_schematic.py')],check=True)
 entries=[]
 for lib in sorted({p['footprint'].split(':')[0] for p in c['parts']}):
  uri='${KIPRJMOD}/'+(lib+'.pretty' if lib.startswith('NearGlow') else 'footprints/'+lib+'.pretty');entries.append(f'(lib (name "{lib}")(type "KiCad")(uri "{uri}")(options "")(descr "Local library"))')
 write(BASE/'fp-lib-table','(fp_lib_table (version 7) '+''.join(entries)+')')
 board=pcb.LoadBoard(str(OLD/'naehe-glut-r2a.kicad_pcb'));offset=vec(5.1,0)
 for o in list(board.GetFootprints())+list(board.GetTracks())+list(board.Zones())+list(board.GetDrawings()):o.Move(offset)
 removed_drawings=list(board.GetDrawings())
 for o in removed_drawings:
  if o.GetLayer()==pcb.Edge_Cuts:board.Remove(o)
 for a,b in [((0,5.2),(30,5.2)),((30,5.2),(30,43)),((30,43),(0,43)),((0,43),(0,30)),((0,30),(1.5,30)),((1.5,30),(1.5,28)),((1.5,28),(0,28)),((0,28),(0,5.2))]:
  s=pcb.PCB_SHAPE(board);s.SetShape(pcb.SHAPE_T_SEGMENT);s.SetStart(vec(*a));s.SetEnd(vec(*b));s.SetLayer(pcb.Edge_Cuts);s.SetWidth(pcb.FromMM(.05));board.Add(s)
 netnames={str(n.GetNetname()) for n in board.GetNetsByNetcode().values()}
 for net in sorted({v for p in c['parts'] for v in p['pins'].values() if v}):
  if '/'+net not in netnames:board.Add(pcb.NETINFO_ITEM(board,'/'+net))
 places={'J4':(7.5,39.325,0),'J5':(23.5,33,0),'JP1':(18.3,38.7,0),'SW1':(4.7,28.8,0),'SW2':(27,27,0),'U8':(9.5,27.5,270),'U9':(25.5,34,0),'U10':(21.5,34,0),'U11':(13,29,90),'U12':(17.5,31,0),'D1':(9.5,33,180),'D2':(20,25.8,0),'D3':(23,25.8,0),'D4':(28.7,31,0),'D5':(2,25.5,0)}
 others=[p for p in c['parts'] if p['ref'] not in original and p['ref'] not in places]
 for i,p in enumerate(others):places[p['ref']]=(3+(i%10)*2.5,25.5+(i//10)*3,0)
 places.update({'R30':(14.5,25.2,180),'R31':(14.5,26.2,180),'C30':(21.7,36.5,0),'C31':(18.5,30,0),'R47':(25,38,0),'R48':(27,36,0),'C23':(3,31.5,0),'C24':(6.5,31.5,0),'C25':(10,31.5,0),'C26':(17,31.5,0),'C27':(20.5,31.5,0),'C28':(14.5,35,0),'C29':(18,34.5,0)})
 existing={f.GetReference():f for f in board.GetFootprints()}
 for p in c['parts']:
  if p['ref'] in original:f=existing[p['ref']]
  else:
   lib,name=p['footprint'].split(':');folder=BASE/(lib+'.pretty') if lib.startswith('NearGlow') else BASE/'footprints'/(lib+'.pretty');f=pcb.FootprintLoad(str(folder),name)
   if f is None:raise RuntimeError(p['footprint'])
   board.Add(f);f.SetReference(p['ref']);f.SetValue(p['value']);f.SetFPID(pcb.LIB_ID(lib,name));x,y,rot=places[p['ref']];f.SetPosition(vec(x,y));f.SetOrientationDegrees(rot);f.Reference().SetVisible(False)
   if (p in others and p['ref'] not in ['R30','R31']) or p['ref'] in ['U9','U10']:f.Flip(f.GetPosition(),False)
  if p['ref']=='Q1':
   if f.GetLayer()==pcb.B_Cu:f.Flip(f.GetPosition(),False)
   f.SetPosition(vec(26.7,14.5));f.SetOrientationDegrees(0)
  if p['ref']=='JP1':
   for g in f.GraphicalItems():
    if g.GetLayer()==pcb.F_SilkS:g.SetLayer(pcb.F_Fab)
  for pad in f.Pads():
   name=p['pins'].get(pad.GetNumber())
   if name:pad.SetNet(board.FindNet('/'+name))
 for x,y in [(1.5,22),(28.5,22),(15,41)]:
  z=pcb.ZONE(board);z.SetIsRuleArea(True);z.SetLayerSet(pcb.LSET.AllCuMask());z.SetDoNotAllowTracks(True);z.SetDoNotAllowVias(True);z.SetDoNotAllowZoneFills(True);z.SetDoNotAllowPads(True);z.SetDoNotAllowFootprints(True);poly=z.Outline();poly.NewOutline()
  for xx,yy in [(x-1.5,y-1.5),(x+1.5,y-1.5),(x+1.5,y+1.5),(x-1.5,y+1.5)]:poly.Append(pcb.FromMM(xx),pcb.FromMM(yy))
  board.Add(z)
 for label,x,y,angle in [('BOOT',4.7,26.4,0),('RESET',27,24.5,0),('USB / GPIO',28.8,37.2,90)]:
  t=pcb.PCB_TEXT(board);t.SetText(label);t.SetPosition(vec(x,y));t.SetTextSize(vec(1.,1.));t.SetTextThickness(pcb.FromMM(.15));t.SetLayer(pcb.F_SilkS);t.SetTextAngle(pcb.EDA_ANGLE(angle,pcb.DEGREES_T));board.Add(t)
 board.SetFileName(str(BASE/'naehe-glut-r2b.kicad_pcb'));pcb.SaveBoard(str(BASE/'naehe-glut-r2b.kicad_pcb'),board)
 project=json.loads((OLD/'naehe-glut-r2a.kicad_pro').read_text(encoding='utf-8'));project['meta']['filename']='naehe-glut-r2b.kicad_pro';project.get('board',{}).get('design_settings',{}).pop('drc_exclusions',None)
 severity=project.get('board',{}).get('design_settings',{}).get('rule_severities',{})
 for k in list(severity):
  if severity[k]=='ignore':severity[k]='warning'
 dump(BASE/'naehe-glut-r2b.kicad_pro',project)
 dump(BASE/'mechanical-interface.json',dict(status='DRAFT geometry binding, not fit approval',pcb_bounds_mm=[0,5.2,30,43],pcb_thickness_mm=1,assembly_xy_mm=[30,43],core_translation_mm=[5.1,0],support_centres_mm=[[1.5,22],[28.5,22],[15,41]],support_radius_mm=1.25,support_keepout_squares_mm=3,polarity_notch_mm=[0,28,1.5,30],ports={'USB':{'mpn':'USB4105-GF-A','origin_mm':[7.5,39.325],'rotation_deg':0,'entry':'+Y','front_y_mm':43},'JTAG':{'mpn':'FTSH-105-01-L-DV-007-K','origin_mm':[23.5,33],'entry':'+Z','geometry_status':'manufacturer footprint Rev H, series Rev FZ; mating CAD open'}},pcb_sha256=sha(BASE/'naehe-glut-r2b.kicad_pcb')))
 dump(BASE/'pruefung/build-state.json',dict(status='draft_not_released',source_r2a=c['source_r2a_sha256'],part_count=len(c['parts']),new_part_count=len(c['parts'])-len(original),unrouted_new_service_nets=True,baseline_routing_preserved=True,baseline_drc_inheritance_not_claimed=True))
 print('Built',len(c['parts']),'parts; new service circuitry UNROUTED')
if __name__=='__main__':main()








