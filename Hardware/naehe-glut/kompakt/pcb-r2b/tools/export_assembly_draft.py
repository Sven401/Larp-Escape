"""Hash-bound BOM/CPL review draft only. No Gerber, order or release export."""
from pathlib import Path
import argparse, collections, csv, hashlib, json, re, subprocess, tempfile
from cad_environment import configure
configure()
import pcbnew as pcb
BASE=Path(__file__).resolve().parents[1]
CLI=BASE.parents[3]/'.tmp/cad-tools/kicad/bin/kicad-cli.exe'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def natural(ref): return [int(x) if x.isdigit() else x for x in re.split(r'(\d+)',ref)]
def rows(path):
    with path.open(encoding='utf-8-sig',newline='') as f: return list(csv.DictReader(f))
def write_csv(path,header,data):
    with path.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f); w.writerow(header); w.writerows(data)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--pcb',type=Path,default=BASE/'naehe-glut-r2b.kicad_pcb')
    parser.add_argument('--require-final-datum',action='store_true')
    args=parser.parse_args()
    inputs={'pcb':args.pcb,'circuit':BASE/'circuit.json','bom':BASE/'bom.csv'}
    before={k:sha(p) for k,p in inputs.items()}
    circuit=json.loads(inputs['circuit'].read_text(encoding='utf-8-sig'))
    parts=circuit['parts']; refs=[p['ref'] for p in parts]
    if len(refs)!=len(set(refs)): raise RuntimeError('Duplicate circuit reference')
    fitted={p['ref']:p for p in parts if p.get('in_bom',True) and not p.get('dnp',False)}
    bom_rows=rows(inputs['bom'])
    if len(bom_rows)!=len(fitted) or {r['Reference'] for r in bom_rows}!=set(fitted):
        raise RuntimeError('BOM reference parity failure')
    for r in bom_rows:
        p=fitted[r['Reference']]
        if r['MPN']!=p['mpn'] or r['Footprint']!=p['footprint'] or int(r['Quantity'])!=1:
            raise RuntimeError('BOM identity/quantity mismatch: '+r['Reference'])
    board=pcb.LoadBoard(str(args.pcb))
    footprints={f.GetReference():f for f in board.GetFootprints()}
    if set(footprints)!=set(refs) or len(list(board.GetFootprints()))!=len(refs):
        raise RuntimeError('Native footprint reference parity failure')
    missing_native_mpn=[]
    for part in parts:
        fp=footprints[part['ref']]
        if fp.GetFPIDAsString()!=part['footprint']:
            raise RuntimeError('Native footprint ID mismatch: '+part['ref'])
        if fp.HasField('MPN') and fp.GetFieldText('MPN'):
            if fp.GetFieldText('MPN')!=part['mpn']:
                raise RuntimeError('Native MPN mismatch: '+part['ref'])
        else:
            missing_native_mpn.append(part['ref'])
    edges=[q for d in board.GetDrawings() if isinstance(d,pcb.PCB_SHAPE) and d.GetLayer()==pcb.Edge_Cuts for q in (d.GetStart(),d.GetEnd())]
    ox=min(pcb.ToMM(q.x) for q in edges); oy=max(pcb.ToMM(q.y) for q in edges)
    if abs(ox)>1e-6 or abs(oy-43)>1e-6: raise RuntimeError('Unexpected R2b host outline datum')
    aux=board.GetDesignSettings().GetAuxOrigin(); auxxy=[pcb.ToMM(aux.x),pcb.ToMM(aux.y)]
    datum_matches=abs(auxxy[0]-ox)<1e-6 and abs(auxxy[1]-oy)<1e-6
    if args.require_final_datum and not datum_matches: raise RuntimeError('Native drill/place origin must be (0,43)')
    output_root=BASE/'pruefung/bestueckung-draft'; output_root.mkdir(parents=True,exist_ok=True)
    # Fresh directory per run: failed exports cannot inherit a stale successful manifest.
    out=Path(tempfile.mkdtemp(prefix=before['pcb'][:12]+'-',dir=output_root))
    raw=out/'positions-kicad-global.csv'
    # Deliberately global origin; one explicit translation below defines the CPL datum.
    command=[str(CLI),'pcb','export','pos','--output',str(raw),'--format','csv','--units','mm','--side','both','--exclude-dnp',str(args.pcb)]
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode: raise RuntimeError(result.stderr)
    rawrows=rows(raw); rawrefs=[r['Ref'] for r in rawrows]
    if len(rawrefs)!=len(set(rawrefs)) or not set(fitted).issubset(rawrefs) or set(rawrefs)-set(refs):
        raise RuntimeError('Native full position export reference mismatch')
    cpl=[]
    for r in sorted(rawrows,key=lambda r:natural(r['Ref'])):
        ref=r['Ref']
        if ref not in fitted: continue
        fp=footprints[ref]; pos=fp.GetPosition()
        gx,gy=pcb.ToMM(pos.x),pcb.ToMM(pos.y)
        x,y=float(r['PosX']),float(r['PosY'])
        if abs(x-gx)>1e-5 or abs(y+gy)>1e-5:
            raise RuntimeError('Unexpected global native CSV coordinate transform: '+ref)
        side={'top':'Top','front':'Top','bottom':'Bottom','back':'Bottom'}[r['Side'].lower()]
        if side!=('Bottom' if fp.GetLayer()==pcb.B_Cu else 'Top'):
            raise RuntimeError('Native side mismatch: '+ref)
        x-=ox; y+=oy
        if ref=='J1':
            if fitted[ref]['mpn']!='B2B-PH-SM4-TB(LF)(SN)' or side!='Bottom' or abs(fp.GetOrientationDegrees()-180)>1e-5:
                raise RuntimeError('J1 body-centroid adjustment requires inherited MPN/side/orientation')
            y+=1.75
        if ref=='JP1':
            if fitted[ref]['mpn']!='TSW-102-07-G-S' or side!='Top' or abs(fp.GetOrientationDegrees())>1e-5:
                raise RuntimeError('JP1 midpoint adjustment requires reviewed MPN/side/zero orientation')
            jp_pads={str(p.GetNumber()):p for p in fp.Pads()}
            if set(jp_pads)!={'1','2'}:
                raise RuntimeError('JP1 pad geometry changed')
            p1,p2=jp_pads['1'].GetPosition(),jp_pads['2'].GetPosition()
            if abs(pcb.ToMM(p2.x-p1.x))>1e-6 or abs(pcb.ToMM(p2.y-p1.y)-2.54)>1e-6:
                raise RuntimeError('JP1 pad geometry changed')
            y-=1.27
        cpl.append([ref,f'{x:.6f}mm',f'{y:.6f}mm',side,f'{float(r["Rot"])%360:.6f}'])
    if {r[0] for r in cpl}!=set(fitted): raise RuntimeError('CPL fitted reference mismatch')
    groups=collections.defaultdict(list)
    for ref,p in fitted.items(): groups[(p['mpn'],p['footprint'])].append(ref)
    write_csv(out/'BOM-DRAFT.csv',['Manufacturer Part Number','Designators','Quantity','Footprint'],
              [[mpn,','.join(sorted(rs,key=natural)),len(rs),fp] for (mpn,fp),rs in sorted(groups.items())])
    write_csv(out/'CPL-DRAFT.csv',['Designator','Mid X','Mid Y','Layer','Rotation'],cpl)
    after={k:sha(p) for k,p in inputs.items()}
    if before!=after: raise RuntimeError('Inputs changed during export; discard draft outputs')
    report={'status':'assembly_review_draft_not_manufacturing_release','inputs_sha256':before,
      'pcb_path':str(args.pcb.resolve()),'script_sha256':sha(Path(__file__)),'kicad_version':pcb.GetBuildVersion(),
      'fitted_refs':sorted(fitted,key=natural),'fitted_count':len(fitted),'bom_group_count':len(groups),'cpl_count':len(cpl),
      'excluded_integral_or_dnp_refs':sorted(set(refs)-set(fitted),key=natural),
      'required_mixed_assembly_refs_included':{ref:ref in {r[0] for r in cpl} for ref in ['J4','JP1']},
      'native_aux_origin_mm':auxxy,'cpl_origin_mm':[ox,oy],'native_datum_matches_cpl':datum_matches,
      'j1_centroid':{'pcb_y_down_offset_mm':[0,-1.75],'cpl_y_up_offset_mm':[0,1.75],
        'source':'R2a BESTUECKUNG.txt lines71-80 and tools/export_release.py; inherited J1 MPN/side/180deg orientation; supplier confirmation required'},
      'jp1_centroid':{'pcb_y_down_offset_mm':[0,1.27],'cpl_y_up_offset_mm':[0,-1.27],
        'source':'Samtec TSW Rev DS sheet2 Fig1/-S, two-position symmetric body; native Fab y37.43..42.51; mechanics independently confirmed2026-11-02'},
      'native_footprint_ids_match':True,'missing_native_mpn_fields':missing_native_mpn,
      'native_rotation_retained':True,'commands':[{'argv':command,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr}],
      'outputs_sha256':{p.name:sha(p) for p in [raw,out/'BOM-DRAFT.csv',out/'CPL-DRAFT.csv']},
      'limits':['No supplier catalog mapping, stock, price or delivery confirmation','No Gerber/drill package or manufacturing release','All other centroids/rotations need supplier assembly preview confirmation','Final Gerber/drill origin must match CPL (0,43)']}
    (out/'ASSEMBLY-DRAFT.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print(json.dumps({'output':str(out),'fitted':len(fitted),'cpl':len(cpl),'native_datum_matches':datum_matches}))
if __name__=='__main__': main()


