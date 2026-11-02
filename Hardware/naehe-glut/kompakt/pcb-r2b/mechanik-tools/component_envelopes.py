"""Conservative self-modelled connector/mate envelopes, not vendor STEP models.
Geometry is bound to mechanical-interface.json. Ref dimensions receive explicit
engineering margins, NOT fictitious manufacturer tolerances.
"""
from pathlib import Path
import json, hashlib
import FreeCAD as A
import Part
BASE=Path(__file__).resolve().parents[1];OUT=BASE/'mechanik';V=A.Vector
SOURCES={
 'gct':{'url':'https://datasheet.octopart.com/USB4105-GF-A-060-GCT-datasheet-192318489.pdf','author':'GCT','revision':'B1,2022-12-28','file':'usb4105-manufacturer.pdf','note':'Manufacturer drawing mirrored by Octopart; contains full ordering grid including GF-A blank0.95mm stake. Current GCT revision not independently obtainable.'},
 'ftsh':{'url':'https://suddendocs.samtec.com/prints/ftsh-1xx-xx-xxx-dv-xxx-xxx-x-xx-mkt.pdf','author':'Samtec','revision':'FZ','file':'ftsh-dv-manufacturer.pdf'},
 'ffsd':{'url':'https://suddendocs.samtec.com/prints/ffsd-xx-x-xx.xx-01-x-x-xxx-mkt.pdf','author':'Samtec','revision':'AW','file':'ffsd-manufacturer.pdf'},
 'ribbon':{'url':'https://suddendocs.samtec.com/prints/cb-xx-30-7-38-025-gr.pdf','author':'Samtec','revision':'G,2017-02-14','file':'ribbon-manufacturer.pdf'},
 'usb_cable':{'url':'https://www.assmann-wsw.com/uploads/datasheets/ASS_7348_CA.pdf','author':'ASSMANN WSW','revision':'08,2022-08-25','file':'usb-cable-manufacturer.pdf'}}
SOURCE_HASHES={'gct': '767fff1dd8e5b3ba91c2b4e5a65bd31f658c0f1df8ea1abb10820d9951abc089', 'ftsh': 'af7cf92f6c7aea3dade5ed54c60e3668b833f5da6372df0124bed651692e1e05', 'ffsd': 'aa2f465fbed3ad313227a1ade17d0e52d743a8d4e1a96ca6e04afc4aa5fe71dc', 'ribbon': '6e067c3a0034a9a59014cf5bedf9b93443d7407d8391437044224940893be942', 'usb_cable': '35ee709e548f41ba3aee36bff69eda43ed25bc96906f820ca2a0ba98be2ae4a4'}
def box(x,y,z,dx,dy,dz):return Part.makeBox(dx,dy,dz,V(x,y,z))
def build():
    contract=json.loads((BASE/'mechanical-interface.json').read_text())
    assert contract['pcb_bounds_mm']==[0,5.2,30,43] and contract['pcb_thickness_mm']==1
    usb=contract['ports']['USB'];jt=contract['ports']['JTAG']
    assert usb['mpn']=='USB4105-GF-A' and abs(usb['front_y_mm']-43)<1e-6
    assert jt['mpn']=='FTSH-105-01-L-DV-007-K' and jt['origin_mm']==[23.5,33]
    # Do not silently accept the old text-position/PCB-edge confusion.
    assert abs(usb['origin_mm'][1]+3.675-usb['front_y_mm'])<1e-6, 'USB origin does not match actual footprint front'
    for key,data in SOURCES.items():
        p=OUT/'sources'/data['file'];data['sha256']=SOURCE_HASHES[key]
        if p.exists(): assert hashlib.sha256(p.read_bytes()).hexdigest()==SOURCE_HASHES[key], 'Source changed: '+data['file']
    refs={
      'USB4105_bound':box(27.75,55.30,13,9.50,7.90,3.86),
      'FTSH_K_bound':box(44.75,49.60,13,7.50,6.80,6.30),
      'FFSD_mated_bound':box(42.95,49.40,15.3,11.1,7.20,6.20),
      'USB_overmold_bound':box(26.3,64.85,11.03,12.4,25.4,7.40),
      'USB_cable_max':Part.makeCylinder(2.15,29.75,V(32.5,90.25,14.73),V(0,1,0)),
      'JTAG_ribbon_max':box(45.235,56.6,20.,6.53,63.4,0.74),
    }
    sweeps={
      'USB_mate_removal_10mm':box(26.3,64.85,11.03,12.4,35.4,7.4),
      'JTAG_mate_removal_10mm':box(42.95,49.4,21.5,11.1,7.2,10.)}
    metal={}
    for x in (21.5,58.5):
        metal['clamp_fastener_'+str(x)]=Part.makeCylinder(2.5,1.8,V(x,43,3)).fuse(Part.makeCylinder(1.,13,V(x,43,2))).fuse(Part.makeCylinder(1.9,2,V(x,43,16)))
        metal['base_fastener_'+str(x)]=Part.makeCylinder(2.5,1.6,V(x,68,-7.3)).fuse(Part.makeCylinder(1.,14,V(x,68,-8))).fuse(Part.makeCylinder(1.9,2,V(x,68,6)))
    note={'status':'dimensioned_conservative_envelopes_not_vendor_STEP_or_physical_fit_approval','sources':SOURCES,
      'contract_sha256':hashlib.sha256((BASE/'mechanical-interface.json').read_bytes()).hexdigest(),
      'USB4105':{'mpn':'USB4105-GF-A','nominal_body_mm':[8.94,7.35,3.31],'drawing_tolerance_mm':0.15,'engineering_xy_pose_margin_each_side_mm':0.20,'engineering_z_solder_margin_mm':0.40,'model_note':'Body only; anchors/pegs excluded because through-board and checked separately by PCB. Backside shell pins may protrude0.1mm below1mm PCB.'},
      'FTSH':{'mpn':'FTSH-105-01-L-DV-007-K','nominal_length_mm':6.35,'length_plus_mm':0.13,'cut_flash_max_mm':0.51,'key_width_REF_mm':5.08,'height_REF_mm':5.71,'bound_mm':[7.5,6.8,6.3],'note':'REF dimensions have no manufactured tolerance claim. Envelope includes tails and engineering margin; exact 3D supplier model not assumed.'},
      'FFSD':{'mpn':'FFSD-05-D-06.00-01-N','length_mm':152.4,'length_tolerance_mm':3.175,'body_length_mm':10.54,'body_height_max_mm':5.23,'mate_bound_mm':[11.1,7.2,6.2],'ribbon_width_max_mm':6.5278,'ribbon_thickness_max_mm':0.7366,'used_fixture_cable_length_mm':63.4,'remaining_length_lower_bound_mm':85.825,'note':'152.4-3.175-63.4=85.825 before second connector and service slack; 1:1 pin mapping and actual adapter-side mate still require verification.'},
      'USB_cable':{'mpn':'A-USB31C-20A-100','length_mm':1000,'length_tolerance_mm':50,'overmold_nominal_mm':[11.,24.,6.5],'overmold_max_mm':[12.,25.,7.],'pose_margin_each_side_mm':0.2,'cable_od_nominal_mm':3.8,'cable_od_max_mm':4.3,'minimum_overmold_front_gap_mm':1.85,'connector_axis_z_mm':14.73,'note':'Straight cable seat avoids inferred minimum bend radius. Generic USB cable substitutes require their own envelope check.'},
      'scope_limits':['Exact manufacturer tolerance for REF dimensions remains supplier qualification.','USB drawing revision freshness remains open; body/footprint checked to recorded B1.','Mated envelopes deliberately overlap the mating connector; pairwise component intersection is not used as an invalid failure.','Cable clamp contact is intentional and reviewed separately from the free cable corridor.','Final full-PCBA components, adapter-side cable mate and physical fit/load/RF/electrical tests remain open.']}
    return refs,sweeps,metal,note
