"""Resolve genuine shipped P1S profiles, slice all fixture parts, export project.
No printer communication. Run with ordinary Python 3.12 from repository root.
"""
from pathlib import Path
import json, hashlib, os, subprocess, shutil, sys, zipfile, re
BASE=Path(__file__).resolve().parents[1];ROOT=BASE.parents[3]
OUT=BASE/'mechanik';PROFILES=OUT/'slicer';SLICER=Path(os.environ.get('R2B_BAMBU_STUDIO_DIR',str(ROOT/'.tmp/cad-tools/bambu-studio'))).resolve()
PARTS=['base','insert_r2b','clamp_left','clamp_right','usb_tie_bar','jtag_tie_bar']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    PROFILES.mkdir(exist_ok=True);sources={}
    def resolve(kind,name,stack=()):
        if name in stack:raise ValueError('Profile cycle')
        p=SLICER/'resources/profiles/BBL'/kind/(name+'.json');sources[str(p.relative_to(SLICER))]=sha(p)
        v=json.loads(p.read_text());out={}
        if v.get('inherits'):out.update(resolve(kind,v['inherits'],stack+(name,)))
        for n in v.get('include',[]):out.update(resolve(kind,n,stack+(name,)))
        out.update({k:x for k,x in v.items() if k not in ['include','inherits']});return out
    for target,kind,name in [('machine','machine','Bambu Lab P1S 0.4 nozzle'),('process','process','0.20mm Standard @BBL X1C'),('petg','filament','Generic PETG')]:
        v=resolve(kind,name)
        if target=='process':v.update(wall_loops='4',sparse_infill_density='20%',sparse_infill_pattern='gyroid',bottom_shell_layers='5',top_shell_layers='5',enable_support='0',brim_type='outer_only',brim_width='3',curr_bed_type='Textured PEI Plate')
        if target=='machine':assert v['printer_model']=='Bambu Lab P1S' and v['nozzle_diameter']==['0.4']
        if target=='petg':assert 'Bambu Lab P1S 0.4 nozzle' in v['compatible_printers']
        (PROFILES/(target+'.json')).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
    (PROFILES/'sources.json').write_text(json.dumps({'source':'Official Bambu Studio portable profiles bundled v02.08.02.61','upstream':'https://github.com/bambulab/BambuStudio/tree/v02.08.02.61/resources/profiles/BBL','sha256':sources,'license':'AGPL-3.0','process_note':'X1C is the official default process named by the P1S machine; actual printer is P1S.'},indent=2)+'\n')
    assert (PROFILES/'LICENSE-AGPL-3.0.txt').is_file(), 'Bundled AGPL license missing'
    dest=OUT/'sliced';dest.mkdir(exist_ok=True)
    cmd=[str(SLICER/'python-cli.exe'),str(BASE/'mechanik-tools/run_bambu.py'),str(SLICER),'--datadir',str(dest/'config'),'--load-settings',str(PROFILES/'machine.json')+';'+str(PROFILES/'process.json'),'--load-filaments',str(PROFILES/'petg.json'),'--arrange','1','--orient','0','--slice','0','--outputdir',str(dest),'--export-3mf','R2b-service-P1S-PETG-draft.3mf']+[str(OUT/(n+'.stl')) for n in PARTS]
    env=os.environ.copy();env['PYTHONHOME']=sys.base_prefix
    result=subprocess.run(cmd,cwd=BASE,env=env,capture_output=True,text=True)
    (dest/'console.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode:raise SystemExit('Slicer failure '+str(result.returncode)+'; see sliced/console.log')
    report=json.loads((dest/'result.json').read_text());assert report['return_code']==0
    project=dest/'R2b-service-P1S-PETG-draft.3mf';shutil.copyfile(project,OUT/project.name)
    with zipfile.ZipFile(project) as z:
        gc=[n for n in z.namelist() if n.endswith('.gcode')];assert len(gc)==1
        gcode=z.read(gc[0]).decode();assert 'Bambu Lab P1S' in gcode
        layers=re.split(r'; CHANGE_LAYER',gcode)[1:]
        extrusions=[len(re.findall(r'^G1[^\n]*[XY][^\n]* E[0-9.]',layer,re.M)) for layer in layers]
        assert layers and all(extrusions)
        summary={'status':'digital_slice_only_no_physical_fit_release','printer':'Bambu Lab P1S','nozzle_mm':0.4,'material':'Generic PETG','layer_mm':0.2,'walls':4,'infill':'20% gyroid','brim':'outer 3mm','supports':False,'orientation':'STL z=0, orient disabled, XY arrangement allowed','part_count':len(PARTS),'stl_sha256':{n:sha(OUT/(n+'.stl')) for n in PARTS},'project_sha256':sha(project),'gcode_sha256':hashlib.sha256(gcode.encode()).hexdigest(),'layers':len(layers),'xy_extrusions_per_layer':extrusions,'slicer_result':report}
    (OUT/'slice-report.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in summary.items() if k not in ['slicer_result','xy_extrusions_per_layer']},indent=2))
if __name__=='__main__':main()

