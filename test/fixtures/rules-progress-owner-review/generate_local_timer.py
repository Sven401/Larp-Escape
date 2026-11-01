"""Mechanical existing full-artifact variants, not a third acceptance game."""
import ast,copy,importlib.util,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
old=HERE.parent/'manual-override-v2-review'
spec=importlib.util.spec_from_file_location('local_timer_bytes',old/'BaseFixtures.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
tree=ast.parse((old/'generate.py').read_text(encoding='utf8'));names={'qname','expression','expand_rules','finalize'};env=dict(b.__dict__);exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names],type_ignores=[]),str(old/'generate.py'),'exec'),env)
cases=[b.build(2)];cases[0]['name']='owner-local-source-identity'
c=copy.deepcopy(cases[0]);c['name']='owner-timer-source-identity';content=json.loads(b.data(c,'content.json'));s=content['stations'][0];s['instances']=[];s['timers']=[dict(id='wait',durationMs=10)]
s['rules']=[b.local_rule('arm','pulse',dict(op='eq',left=dict(op='input',field='value'),right=b.literal(1)),[dict(op='arm',timer='wait')]),b.local_rule('cancel','pulse',dict(op='eq',left=dict(op='input',field='value'),right=b.literal(2)),[dict(op='cancel',timer='wait')]),dict(id='fire',generation=1,priority=0,mode='each',trigger=dict(kind='timer',timer='wait'),guard=b.literal(True),offline=dict(policy='local_continue'),actions=[dict(op='effect',port='light',resource='preset')])]
b.setdata(c,'content.json',b.canonical(content));d=b.deployment(c);d['rules']=[r for r in d['rules'] if r['station']!='s0']+[dict(station='s0',expandedId='/'+r['id'],ruleId=b.identity(0xC010+i).hex(),generation=1,offline=r['offline']) for i,r in enumerate(s['rules'])];d['rules'].sort(key=lambda r:(r['station'],r['expandedId']));b.setdeployment(c,d);env['finalize'](c)
# The independent expansion helper predates timers. Qualify direct internal IDs
# explicitly and compare these exact expected IR bytes with the actual compiler.
profile=json.loads(b.data(c,'profiles/s0.json'));profile['timers'][0]['id']='/wait'
for r in profile['rules']:
 if r['trigger']['kind']=='timer':r['trigger']['timer']='/wait'
 for a in r['actions']:
  if 'timer' in a:a['timer']='/wait'
raw=b.canonical(profile);b.setdata(c,'profiles/s0.json',raw);c['expectedProfiles']['s0']=raw.hex();d=b.deployment(c);r=next(r for r in d['profiles'] if r['station']=='s0');r.update(bytes=len(raw),digest=b.sha(raw));b.setdeployment(c,d);cases.append(c)
if __name__=='__main__':(HERE/'local-timer-artifacts.json').write_text(json.dumps(cases,indent=2)+'\n',encoding='utf8',newline='\n')
