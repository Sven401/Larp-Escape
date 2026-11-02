from pathlib import Path
import os,json,hashlib,shutil
P=Path(__file__).resolve().parent;D=P/'stackup-trial';D.mkdir(exist_ok=True);cfg=D/'kicad-config';cfg.mkdir(exist_ok=True);os.environ['KICAD_CONFIG_HOME']=str(cfg)
import pcbnew as p
src=P/'host-pair-trial/host-pair.kicad_pcb'
data=json.loads((P/'jlc-stackups.json').read_text(encoding='utf-8-sig'));tpl=next(x for x in data['body']['list'] if x['appointName']=='JLC061011-3313A')
rows=tpl['basicDataList']; layers=[];ci=0;di=0;names=['F.Cu','In1.Cu','In2.Cu','In3.Cu','In4.Cu','B.Cu']
def copper(t):
 global ci
 layers.append(f'(layer "{names[ci]}" (type "copper") (thickness {t}))');ci+=1
def dielectric(t,typ,material,dk):
 global di
 material += ' [Df0.02 SOFTWAREDEFAULT UNVERIFIED]';di+=1;layers.append(f'(layer "dielectric {di}" (type "{typ}") (thickness {t} locked) (material "{material}") (epsilon_r {dk}))')
i=0
while i<len(rows):
 r=rows[i]
 if r['material']=='Copper': copper(r['topConductorThick'])
 elif r['material']=='Core':
  copper(r['topConductorThick']);dielectric(r['dielectricThick'],'core',r['brandType']+' '+r['materialName'],r['dielectricConstant']);copper(r['botConductorThick'])
 elif r['material']=='Prepreg':
  t=r['dielectricThick'];count=1
  while i+1<len(rows) and rows[i+1]['material']=='Prepreg':
   i+=1;t+=rows[i]['dielectricThick'];count+=1
  dielectric(t,'prepreg',r['brandType']+' '+str(count)+'x '+r['materialName'],r['dielectricConstant'])
 else: raise ValueError(r['material'])
 i+=1
assert ci==6 and di==5
block='(stackup\n'+'\n'.join(layers)+'\n(copper_finish "ENIG")\n(dielectric_constraints no)\n)'
s=src.read_text(encoding='utf-8-sig');assert '(stackup' not in s
s=s.replace('(setup','(setup\n'+block,1)
raw=D/'proposed-3313A-input.kicad_pcb';raw.write_text(s,encoding='utf-8')
b=p.LoadBoard(str(raw));tb=b.GetTitleBlock();tb.SetRevision('R2b engineering draft');tb.SetComment(0,'Proposed JLC061011-3313A; supplier approval pending');tb.SetComment(1,'Df0.02 SOFTWAREDEFAULT UNVERIFIED; mask thickness unspecified');b.SetTitleBlock(tb)
out=D/'proposed-3313A-roundtrip.kicad_pcb';p.SaveBoard(str(out),b)
again=D/'proposed-3313A-roundtrip2.kicad_pcb';p.SaveBoard(str(again),p.LoadBoard(str(out)))
shutil.copyfile(P.parents[1]/'naehe-glut-r2b.kicad_pro',out.with_suffix('.kicad_pro'))
shutil.copyfile(P/'host-pair-trial/fp-lib-table',D/'fp-lib-table')
sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
r={'status':'separate representation experiment; not promoted; native default loss tangent unqualified','source_sha256':sha(src),'manufacturer_source_sha256':sha(P/'jlc-stackups.json'),'input_sha256':sha(raw),'candidate_sha256':sha(out),'second_roundtrip_sha256':sha(again),'roundtrip_byte_stable':out.read_bytes()==again.read_bytes(),'nominal_board_mm':p.ToMM(b.GetDesignSettings().GetBoardThickness()),'published_copper_dielectric_sum_mm':.958,'difference_to_nominal_mm':.042,'mask_stackup_layers':'omitted: no uniform physical mask thickness published; existing mask artwork unchanged','loss_tangent':'omitted from input; KiCad automatically adds default0.02, NOT manufacturer value','source_layers':rows}
(D/'STACKUP-ROUNDTRIP.json').write_text(json.dumps(r,indent=2));print(json.dumps({k:v for k,v in r.items() if k!='source_layers'}))

