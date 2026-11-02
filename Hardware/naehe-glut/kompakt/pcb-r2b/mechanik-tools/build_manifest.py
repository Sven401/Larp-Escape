"""Hash the actual draft delivery, with explicit open final-PCB binding."""
from pathlib import Path
import hashlib,json
BASE=Path(__file__).resolve().parents[1];OUT=BASE/'mechanik'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    names=['base','insert_r2b','clamp_left','clamp_right','usb_tie_bar','jtag_tie_bar']
    paths=[OUT/(n+'.'+ext) for n in names for ext in ['step','stl']]
    paths += [OUT/n for n in ['R2b-service-draft.FCStd','assembly-draft.step','R2b-service-P1S-PETG-draft.3mf','geometry-report.json','slice-report.json','assembled-draft.png','exploded-draft.png','underside-draft.png','DRUCK-UND-MONTAGE.txt','component-envelope-report.json','pcba-envelope-report.json','pcb-placement.json','adapter-mating-report.json','metadata-rebind-report.json']]
    paths += sorted((BASE/'mechanik-tools').glob('*.py'))+sorted((BASE/'mechanik-tools').glob('*.txt'))+sorted((OUT/'slicer').glob('*'))
    bindings={}
    for n in ['PLANUNG.txt','bom.csv','SERVICE-INTERFACE.csv','circuit.json','naehe-glut-r2b.kicad_pcb','mechanical-interface.json']:
        p=BASE/n;bindings[n]=sha(p) if p.exists() else None
    manifest={'date':'2026-11-02','status':'engineering_draft_not_final_fit_or_operation_release','units':'mm','print_quantities':{n:1 for n in names},'pcb_binding':bindings,'binding_note':'Hashes bind frozen PCB and digitally screened fixture/PCBA; exact MPN tolerance and cross-vendor mating qualification plus physical fit remain open.','connectors':[{'mpn':'USB4105-GF-A','model':'self-modelled conservative tolerance envelope; see component-envelope-report.json','physical_fit_qualified':False},{'mpn':'FTSH-105-01-L-DV-007-K','model':'self-modelled conservative tolerance envelope; see component-envelope-report.json','physical_fit_qualified':False}],'sha256':{str(p.relative_to(BASE)).replace('\\','/'):sha(p) for p in paths}}
    (OUT/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');print(len(paths),'files hashed')
if __name__=='__main__':main()
