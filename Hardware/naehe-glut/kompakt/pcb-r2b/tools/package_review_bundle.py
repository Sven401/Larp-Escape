"""Curated, hash-verified engineering bundle. Never a manufacturing release."""
from pathlib import Path
import ast, hashlib, json, zipfile
BASE=Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def read(relative): return json.loads((BASE/relative).read_text(encoding='utf-8-sig'))
def verify(mapping):
    for relative,expected in mapping.items():
        path=(BASE/relative).resolve()
        if not path.is_relative_to(BASE.resolve()) or not path.is_file() or sha(path)!=expected:
            raise RuntimeError('Missing, unsafe or stale input: '+relative)
def main():
    review=read('pruefung/UNABHAENGIGES-REVIEW.json')
    if review['status']!='reviewed_engineering_draft_NOT_manufacturing_or_operation_release':
        raise RuntimeError('Expected independently reviewed engineering draft')
    verify(review['source_hashes'])
    mech=read('mechanik/MANIFEST.json'); verify(mech['sha256']); verify(mech['pcb_binding'])
    provenance=read('sources/r2a-input/PROVENANCE.json')
    verify({'sources/r2a-input/'+r['path']:r['sha256'] for r in provenance['files']})
    drc=read('pruefung/drc.json')
    if drc['unconnected_items'] or drc['schematic_parity']:
        raise RuntimeError('Open connections or schematic parity failure')
    errors=[r for r in drc['violations'] if r['severity']=='error']
    if len(errors)!=4 or any(r['type']!='hole_clearance' for r in errors):
        raise RuntimeError('DRC state differs from reviewed four open DFM errors')
    pad=read('pruefung/pcb-pad-contract.json')
    if pad['status']!='pass' or pad['input_sha256']['pcb']!=sha(BASE/'naehe-glut-r2b.kicad_pcb'):
        raise RuntimeError('Pad audit does not bind current PCB')
    rfq=read('procurement/RFQ-INPUTS.json')
    verify(rfq['input_sha256'])
    verify({'procurement/'+p:h for p,h in rfq['outputs_sha256'].items()})
    screen=read('procurement/CLEARANCE-SCREEN.json')
    verify({'procurement/'+p:h for p,h in screen['input_sha256'].items()})
    if screen['frozen_pcb_sha256']!=sha(BASE/'naehe-glut-r2b.kicad_pcb') or screen['new_rule_violations']!=0:
        raise RuntimeError('Manufacturer-distance screen is stale or failed')
    files=set('''PLANUNG.txt PLANUNGSREVIEW.json SERVICE-INTERFACE.csv
UMSETZUNGSSTAND.txt TERMIN-UND-ABNAHME.txt JTAG-PROBE-AUSWAHL.txt
SCHALTUNGSVERTRAG.txt REPRODUKTION.txt HARDWARE-DATEIEN.txt
naehe-glut-r2b.kicad_pcb naehe-glut-r2b.kicad_sch naehe-glut-r2b.kicad_pro
naehe-glut-r2b.net.xml circuit.json bom.csv mechanical-interface.json
fp-lib-table sym-lib-table NearGlowR2b.kicad_sym KICAD-LIBRARY-LICENSE.md
R2A-LIBRARY-SOURCES.json SOURCES.json sources/r2a-input/PROVENANCE.json
sources/layout-evidence/route-base.kicad_pcb sources/layout-evidence/usb-paired.kicad_pcb
pruefung/AUSGANGSSTAND.json pruefung/UNABHAENGIGES-REVIEW.json
pruefung/ANALOG-REVIEW.json pruefung/J1-KABEL-REVIEW.json
pruefung/USB-KAPAZITAET.txt pruefung/USB4105-GEOMETRIE.txt
pruefung/layout-freeze.json pruefung/build-state.json pruefung/routing-progress.json
pruefung/drc.json pruefung/erc.json pruefung/usb-route-final.json pruefung/pcb-pad-contract.json
mechanik/MANIFEST.json tools/cad_environment.py tools/build_r2b.py
tools/build_schematic.py tools/check_pcb_pad_contract.py tools/export_assembly_draft.py
tools/analyze_usb_route.py tools/freeze_layout.py tools/package_review_bundle.py'''.split())
    procurement_files = '''DFM-ANGEBOT.txt QUELLEN.txt RFQ-INPUTS.json
VIA-FILL-INVENTORY.csv QUALIFIKATION-OFFEN.txt KALKULATOR.txt
FIRMWARE-ABGLEICH.txt TECHNISCHE-PRAEZISIERUNG.txt REVIEW.txt
CLEARANCE-SCREEN.json VIA-METADATA-CANDIDATE.json METADATA-PROMOTION.json
extract_rfq_inputs.py prepare_metadata_candidate.py record_promotion.py
run-clearance-screen.ps1 summarize_clearance.py
clearance-screen/screen.kicad_pcb clearance-screen/screen.kicad_pro
clearance-screen/screen.kicad_dru clearance-screen/positive-control.kicad_dru
clearance-screen/production-screen-rules.txt clearance-screen/drc-all-layers.json
clearance-screen/positive-control.json clearance-screen/metadata-candidate.kicad_pcb'''.split()
    files.update('procurement/' + name for name in procurement_files)
    files.update(mech['sha256'])
    files.update('sources/r2a-input/'+r['path'] for r in provenance['files'])
    for folder in ['NearGlowR2a.pretty','NearGlowR2b.pretty','footprints']:
        files.update(p.relative_to(BASE).as_posix() for p in (BASE/folder).rglob('*.kicad_mod'))
    assembly_reports=[p for p in review['source_hashes'] if p.endswith('/ASSEMBLY-DRAFT.json')]
    if len(assembly_reports)!=1: raise RuntimeError('Ambiguous final assembly draft')
    assembly_path=Path(assembly_reports[0]); assembly=read(assembly_path)
    if not assembly['native_datum_matches_cpl'] or assembly['fitted_count']!=assembly['cpl_count']:
        raise RuntimeError('Assembly draft is not bound to correct datum/count')
    if assembly['inputs_sha256']['pcb']!=sha(BASE/'naehe-glut-r2b.kicad_pcb'):
        raise RuntimeError('Stale assembly PCB')
    verify({(assembly_path.parent/name).as_posix():digest for name,digest in assembly['outputs_sha256'].items()})
    files.add(assembly_path.as_posix())
    files.update((assembly_path.parent/name).as_posix() for name in assembly['outputs_sha256'])
    records=[]; python_files=0
    for relative in sorted(files):
        path=(BASE/relative).resolve()
        if not path.is_relative_to(BASE.resolve()) or not path.is_file(): raise RuntimeError(relative)
        if path.suffix=='.py':
            ast.parse(path.read_text(encoding='utf-8-sig'),filename=relative); python_files+=1
        records.append({'path':relative,'sha256':sha(path),'bytes':path.stat().st_size})
    manifest={'status':'reviewed_engineering_draft_NOT_manufacturing_or_operation_release',
       'date':'2026-11-02','entry_point':'UMSETZUNGSSTAND.txt',
       'pcb_sha256':sha(BASE/'naehe-glut-r2b.kicad_pcb'),'files':records,
       'checks':{'independent_review_bindings':len(review['source_hashes']),
                 'mechanical_file_bindings':len(mech['sha256']),'python_syntax_files':python_files},
       'open_gates':'Four USB-NPTH DFM errors, documented warnings, stackup, supply/probe qualification, physical fit and two-device operation/runtime acceptance.'}
    out=BASE/'paket'; out.mkdir(exist_ok=True)
    payload=(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
    (out/'DATEIEN.json').write_bytes(payload)
    archive=out/'R2b-Entwurf.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for record in records:
            raw=(BASE/record['path']).read_bytes()
            if hashlib.sha256(raw).hexdigest()!=record['sha256']: raise RuntimeError('Input changed during packaging')
            info=zipfile.ZipInfo(record['path'],(2026,11,2,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(info,raw)
        info=zipfile.ZipInfo('DATEIEN.json',(2026,11,2,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
        z.writestr(info,payload)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None: raise RuntimeError('Archive CRC failure')
        for record in records:
            if hashlib.sha256(z.read(record['path'])).hexdigest()!=record['sha256']: raise RuntimeError('Archive readback mismatch')
    digest=sha(archive)
    (out/'ARCHIV-SHA256.txt').write_text(digest+'  R2b-Entwurf.zip\n',encoding='utf-8')
    print(json.dumps({'files':len(records),'python_syntax_files':python_files,'archive_bytes':archive.stat().st_size,'archive_sha256':digest}))
if __name__=='__main__': main()
