"""Preserve actual full false-guard artifact set; no product encoder imports."""
import ast,copy,hashlib,importlib.util,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
OLD=HERE.parent/'manual-override-v2-review'
def build():
 spec=importlib.util.spec_from_file_location('owner_review_byte_helpers',OLD/'BaseFixtures.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
 source=(OLD/'generate.py').read_text(encoding='utf8');tree=ast.parse(source)
 # Execute ONLY these pure function definitions, never the old generator's
 # top-level corpus edits/writes. In particular never import generate.py.
 names={'qname','expression','expand_rules','finalize'}
 selected=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names];assert len(selected)==4
 env=dict(base.__dict__);exec(compile(ast.Module(body=selected,type_ignores=[]),str(OLD/'generate.py'),'exec'),env)
 cases=json.loads((OLD/'vectors.json').read_text(encoding='utf8'));case=copy.deepcopy(next(c for c in cases if c['name']=='valid-full-v2-independent'))
 case['name']='owner-runtime-actual-false-replacement-guard'
 module=json.loads(base.data(case,'modules/gate.json'));assert module['rules'][1]['trigger']=={'kind':'override'}
 module['rules'][1]['guard']={'op':'literal','type':'bool','value':False}
 # Existing expression literal type is the exact original source schema.
 module['rules'][1]['guard']['type']=json.loads(base.data(next(c for c in cases if c['name']=='valid-full-v2-independent'),'modules/gate.json'))['rules'][1]['guard']['type']
 base.setdata(case,'modules/gate.json',base.canonical(module));env['finalize'](case)
 return case
if __name__=='__main__':
 p=HERE/'norelease-artifact.json';p.write_text(json.dumps(build(),indent=2)+'\n',encoding='utf8',newline='\n');print(p.name,hashlib.sha256(p.read_bytes()).hexdigest())
