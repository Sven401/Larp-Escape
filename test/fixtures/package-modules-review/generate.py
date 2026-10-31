"""Independent fixed canonical module vectors, derived from P0 reference text."""
import copy
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent

def encode(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)

def literal(value):
    return dict(op='literal', type='bool' if type(value) is bool else 'int32', value=value)

def parameter(name): return dict(op='parameter', id=name)
def value(name): return dict(op='value', id=name)
def binary(op,left,right): return dict(op=op,left=left,right=right)
def port(name,type): return dict(id=name,type=type,label=name,description='review')
def param(name,default,low,high,unit='count'):
    return dict(id=name,type='int32',label=name,description='review',default=default,min=low,max=high,unit=unit)
def rule(name,trigger,guard,actions,mode='once',policy='local_continue'):
    return dict(id=name,generation=1,priority=0,mode=mode,trigger=trigger,guard=guard,offline=dict(policy=policy),actions=actions)
def effect(): return dict(op='effect',port='feedback',resource='glow')
def emit(name='done'): return dict(op='emit',subject=name)
def module(name):
    return dict(format='larp.module.v1',moduleId=name,version='1.0.0',label=name,parameters=[],inputs=[],outputs=[port('done','solved'),port('feedback','light')],values=[],timers=[],rules=[],instances=[],constraints=[],resources=[dict(id='glow',kind='light_preset')])
def threshold(name='threshold'):
    m=module(name);m['parameters']=[param('limit',3,1,1000)];m['inputs']=[port('pulse','int32_sample')];m['values']=[dict(id='count',type='int32',initial=0)]
    trigger=dict(kind='local',ports=['pulse']);valid=binary('eq',dict(op='input',field='value'),literal(1));boundary=binary('sub',parameter('limit'),literal(1))
    m['rules']=[rule('finish',trigger,dict(op='all',args=[valid,binary('eq',value('count'),boundary)]),[dict(op='set',id='count',expr=parameter('limit')),emit(),effect()]),rule('increment',trigger,dict(op='all',args=[valid,binary('lt',value('count'),boundary)]),[dict(op='add',id='count',expr=literal(1))],mode='each')]
    return m

def ordered():
    m=module('ordered_window');m['parameters']=[param('first',1,1,255),param('second',2,1,255),param('windowMs',5000,1,86400000,'ms')];m['inputs']=[port('sequence','int32_sample')];m['values']=[dict(id='phase',type='int32',initial=0)];m['timers']=[dict(id='window',durationMs=dict(parameter='windowMs'))]
    m['constraints']=[dict(op='not',arg=binary('eq',parameter('first'),parameter('second')))]
    trigger=dict(kind='local',ports=['sequence']);phase=lambda v:binary('eq',value('phase'),literal(v));sample=lambda p:binary('eq',dict(op='input',field='value'),parameter(p));setphase=lambda v:dict(op='set',id='phase',expr=literal(v));cancel=dict(op='cancel',timer='window')
    m['rules']=[rule('begin',trigger,dict(op='all',args=[sample('first'),phase(0)]),[setphase(1),dict(op='arm',timer='window')],mode='each'),rule('timeout',dict(kind='timer',timer='window'),phase(1),[setphase(0)],mode='each'),rule('win',trigger,dict(op='all',args=[sample('second'),phase(1)]),[setphase(2),cancel,emit(),effect()]),rule('wrong',trigger,dict(op='all',args=[dict(op='not',arg=sample('second')),phase(1)]),[setphase(0),cancel],mode='each')]
    return m

def all_solved():
    m=module('all_solved');m['inputs']=[port('left','solved'),port('right','solved')];m['rules']=[rule('gate',dict(kind='fact',subjects=['left','right']),dict(op='all',args=[dict(op='fact',subject='left'),dict(op='fact',subject='right')]),[emit(),effect()],policy='wait')];return m

def digest(m): return hashlib.sha256(encode(m).encode()).hexdigest()
def pin(m): return dict(moduleId=m['moduleId'],version=m['version'],digest=digest(m))
def instance(m,id='inst',parameters=None,inputs=None,outputs=None):
    return dict(pin(m),id=id,parameters=parameters or {},inputs=inputs or {},outputs=outputs or dict(done='done',feedback='lamp'),resources=dict(glow='preset'))
def content(modules,inst):
    return dict(format='larp.package.v1',packageId='review',revision=1,label='review',requires=[],modules=[pin(m) for m in modules],resources=[dict(id='preset',kind='light_preset',digest='a'*64,bytes=1)],stations=[dict(id='s',label='station',values=[],timers=[],ports=[dict(id='lamp',type='light',direction='output',capability=''),dict(id='lamp_mid',type='light',direction='output',capability=''),dict(id='pulse',type='int32_sample',direction='input',capability='')],rules=[],instances=[inst])],subjects=[dict(id='done',originStation='s'),dict(id='left',originStation='s'),dict(id='mid',originStation='s'),dict(id='right',originStation='s')])
def build(m=None):
    m=m or threshold();i=instance(m,parameters=dict(limit=literal(3)),inputs=dict(pulse='pulse'));return [m],content([m],i)
CASES=[]
def add(name,mods,c,want='A'):
    CASES.append(dict(name=name,content=encode(c),sources=[dict(pin(m),bytes=encode(m)) for m in mods],expected=want))
def mutate(name,fn,want='R'):
    mods,c=build();fn(mods,c);add(name,mods,c,want)
mods,c=build();add('threshold-reference',mods,c)
mods,c=build(threshold('renamed_counter'));add('threshold-renamed-equivalent',mods,c)
m=ordered();i=instance(m,parameters=dict(first=literal(1),second=literal(2),windowMs=literal(5000)),inputs=dict(sequence='pulse'));add('ordered-window-reference',[m],content([m],i))
m=all_solved();i=instance(m,inputs=dict(left='left',right='right'));add('all-solved-reference',[m],content([m],i))
# Reference wrapper composes local solved outputs into another module's solved inputs.
a=threshold();g=all_solved();w=module('wrapper');w['parameters']=[param('limit',2,1,1000)];w['inputs']=[port('pulse','int32_sample')];w['outputs']=[port('done','solved'),port('feedback','light'),port('mid_done','solved'),port('mid_feedback','light')]
w['instances']=[instance(a,'counter',dict(limit=parameter('limit')),dict(pulse='pulse'),dict(done='mid_done',feedback='mid_feedback')),instance(g,'gate',{},dict(left='mid_done',right='mid_done'),dict(done='done',feedback='feedback'))]
# Nested resource map resolves against the parent resource slot.
for child in w['instances']:child['resources']['glow']='glow'
i=instance(w,parameters=dict(limit=literal(2)),inputs=dict(pulse='pulse'),outputs=dict(done='done',feedback='lamp',mid_done='mid',mid_feedback='lamp_mid'))
add('nested-solved-composition',[a,g,w],content([a,g,w],i))
# Update affected pins after every semantic mutation, except deliberate digest attacks.
def repin(mods,c):
    c['modules']=[pin(m) for m in mods]
    c['stations'][0]['instances'][0].update(pin(mods[0]))
def semantic(name,fn,want='R'):
    mods,c=build();fn(mods[0],c);repin(mods,c);add(name,mods,c,want)
for key in threshold():semantic('module-missing-'+key,lambda m,c,k=key:m.pop(k) if k not in ('moduleId','version') else None) if key not in ('moduleId','version') else None
semantic('unused-rule-extra-field',lambda m,c:m['rules'][1].update(extra=True))
semantic('constraint-forbidden-value',lambda m,c:m.update(constraints=[value('count')]))
semantic('constraint-false-bound-instance',lambda m,c:m.update(constraints=[literal(False)]))
semantic('constraint-true',lambda m,c:m.update(constraints=[literal(True)]),'A')
semantic('constraint-overflow-in-true-any',lambda m,c:m.update(constraints=[dict(op='any',args=[literal(True),binary('eq',binary('add',literal(2147483647),literal(1)),literal(0))])]))
semantic('parameter-bool-default-for-int',lambda m,c:m['parameters'][0].update(default=True))
semantic('parameter-min-over-max',lambda m,c:m['parameters'][0].update(min=5,max=4))
semantic('parameter-default-out-of-range',lambda m,c:m['parameters'][0].update(default=1001))
semantic('parameter-unknown-unit',lambda m,c:m['parameters'][0].update(unit='seconds'))
semantic('duplicate-interface-id',lambda m,c:m['outputs'][0].update(id='pulse'))
semantic('effect-wrong-resource-slot-kind',lambda m,c:m['resources'][0].update(kind='audio'))
for mapname in ['parameters','inputs','outputs','resources']:
 mutate('missing-map-'+mapname,lambda mods,c,k=mapname:c['stations'][0]['instances'][0].pop(k))
 mutate('extra-key-'+mapname,lambda mods,c,k=mapname:c['stations'][0]['instances'][0][k].update(extra=literal(1) if k=='parameters' else 'pulse'))
 mutate('empty-map-'+mapname,lambda mods,c,k=mapname:c['stations'][0]['instances'][0].update({k:{}}))
mutate('parameter-int-as-bool',lambda mods,c:c['stations'][0]['instances'][0]['parameters'].update(limit=literal(True)))
mutate('parameter-out-of-range',lambda mods,c:c['stations'][0]['instances'][0]['parameters'].update(limit=literal(1001)))
mutate('input-wrong-direction',lambda mods,c:c['stations'][0]['instances'][0]['inputs'].update(pulse='lamp'))
mutate('output-wrong-direction',lambda mods,c:c['stations'][0]['instances'][0]['outputs'].update(feedback='pulse'))
mutate('output-missing-subject',lambda mods,c:c['stations'][0]['instances'][0]['outputs'].update(done='absent'))
mutate('resource-missing',lambda mods,c:c['stations'][0]['instances'][0]['resources'].update(glow='absent'))
mutate('module-pin-id-mismatch',lambda mods,c:c['modules'][0].update(moduleId='other'))
mutate('instance-pin-version-mismatch',lambda mods,c:c['stations'][0]['instances'][0].update(version='2.0.0'))
mutate('module-source-changed-label',lambda mods,c:mods[0].update(label='changed'))
mutate('missing-pinned-source',lambda mods,c:mods.clear())
# Same semantic rule bytes with tricky path segments exercise injective generated IDs.
mods,c=build();c['stations'][0]['instances'][0]['id']='a/b~c';add('escaped-instance-path',mods,c)
mods,c=build();c['modules'].append(copy.deepcopy(c['modules'][0]));add('duplicate-identical-pin',mods,c)
# Explicit reference expansion oracle; independent from product code and codecs.
def escape(segment):return segment.replace('~','~0').replace('/','~1')
def expected_module(m,path,parameters,inputs,outputs,resources):
    def ident(name):return '/'+ '/'.join(escape(x) for x in path+[name])
    def expression(e):
        if e['op']=='parameter':return literal(parameters[e['id']])
        e=copy.deepcopy(e);op=e['op']
        if op=='value':e['id']=ident(e['id'])
        if op=='port_is':e['id']=inputs[e['id']]
        if op=='fact':e['subject']=(inputs|outputs)[e['subject']]
        for key in ['arg','left','right']:
            if key in e:e[key]=expression(e[key])
        if 'args' in e:e['args']=[expression(x) for x in e['args']]
        return e
    vv=[dict(v,id=ident(v['id']),initial=parameters[v['initial']['parameter']] if isinstance(v['initial'],dict) else v['initial']) for v in m['values']]
    tt=[dict(t,id=ident(t['id']),durationMs=parameters[t['durationMs']['parameter']] if isinstance(t['durationMs'],dict) else t['durationMs']) for t in m['timers']]
    rr=[]
    for r in m['rules']:
        r=copy.deepcopy(r);r['id']=ident(r['id']);r['guard']=expression(r['guard']);tr=r['trigger']
        if tr['kind']=='local':tr['ports']=[inputs[x] for x in tr['ports']]
        if tr['kind']=='fact':tr['subjects']=[(inputs|outputs)[x] for x in tr['subjects']]
        if tr['kind']=='timer':tr['timer']=ident(tr['timer'])
        for a in r['actions']:
            if a['op'] in ['set','add']:a['id']=ident(a['id']);a['expr']=expression(a['expr'])
            if a['op'] in ['arm','cancel']:a['timer']=ident(a['timer'])
            if a['op']=='emit':a['subject']=outputs[a['subject']]
            if a['op']=='effect':a['port']=outputs[a['port']];a['resource']=resources[a['resource']]
        rr.append(r)
    return vv,tt,rr
for case in CASES:
    if case['expected']!='A':continue
    c=json.loads(case['content']);s=c['stations'][0];i=s['instances'][0];mods={m['moduleId']:m for m in [json.loads(src['bytes']) for src in case['sources']]};m=mods[i['moduleId']]
    parameters={k:v['value'] for k,v in i['parameters'].items()}
    if m['moduleId']=='wrapper':
        vv=[];tt=[];rr=[]
        for child in m['instances']:
            childparams={k:parameters[v['id']] if v['op']=='parameter' else v['value'] for k,v in child['parameters'].items()}
            childinputs={k:(i['inputs']|i['outputs'])[v] for k,v in child['inputs'].items()}
            childoutputs={k:i['outputs'][v] for k,v in child['outputs'].items()}
            childresources={k:i['resources'][v] for k,v in child['resources'].items()}
            v,t,r=expected_module(mods[child['moduleId']],[i['id'],child['id']],childparams,childinputs,childoutputs,childresources);vv+=v;tt+=t;rr+=r
    else:vv,tt,rr=expected_module(m,[i['id']],parameters,i['inputs'],i['outputs'],i['resources'])
    case['stations']=[encode(dict(format='larp.rules.v1',station='s',values=sorted(vv,key=lambda x:x['id']),timers=sorted(tt,key=lambda x:x['id']),ports=s['ports'],rules=sorted(rr,key=lambda x:x['id'])))]
for case in CASES:
    if case['name']=='module-source-changed-label':case['sources'][0]['digest']=json.loads(case['content'])['modules'][0]['digest']
# Independent expansion occurrence/depth and injective-name boundary vectors.
def empty(name):
    m=module(name);m['outputs']=[];m['resources']=[];return m
def bareinstance(m,id):return dict(pin(m),id=id,parameters={},inputs={},outputs={},resources={})
def empty_expected(c,values=None):
    return [encode(dict(format='larp.rules.v1',station='s',values=values or [],timers=[],ports=c['stations'][0]['ports'],rules=[]))]
for depth in [4,5]:
    leaf=empty('leaf');mods=[leaf]
    for n in range(1,depth):
        m=empty('level'+str(n));m['instances']=[bareinstance(mods[-1],'child')];mods.append(m)
    c=content(mods,bareinstance(mods[-1],'root'));add('expansion-depth-'+str(depth),mods,c,'A' if depth==4 else 'R')
    if depth==4:CASES[-1]['stations']=empty_expected(c)
for count in [32,33]:
    leaf=empty('leaf');top=empty('top');top['instances']=[bareinstance(leaf,'child%03d'%n) for n in range(count-1)];mods=[leaf,top];c=content(mods,bareinstance(top,'root'));add('expanded-occurrences-'+str(count),mods,c,'A' if count==32 else 'R')
    if count==32:CASES[-1]['stations']=empty_expected(c)
mods=[empty('leaf')];mods[0]['values']=[dict(id='/'*48,type='int32',initial=0)]
for n in range(1,4):
    m=empty('level'+str(n));m['instances']=[bareinstance(mods[-1],'/'*48)];mods.append(m)
c=content(mods,bareinstance(mods[-1],'/'*48));add('qualified-name-485-bytes',mods,c);CASES[-1]['stations']=empty_expected(c,[dict(id='/'+'/'.join(['~1'*48]*5),type='int32',initial=0)])
m=empty('leaf');m['values']=[dict(id='b',type='int32',initial=2)];c=content([m],bareinstance(m,'a'));c['stations'][0]['values']=[dict(id='a/b',type='int32',initial=1)];add('direct-and-nested-id-injective',[m],c);CASES[-1]['stations']=empty_expected(c,[dict(id='/a/b',type='int32',initial=2),dict(id='/a~1b',type='int32',initial=1)])
# Unused definitions must be typechecked, not silently instantiated with defaults.
mods,c=build();unused=empty('unused');unused['parameters']=[param('x',1,0,2)];unused['constraints']=[binary('eq',parameter('x'),literal(2))];mods.append(unused);c['modules']=[pin(m) for m in mods];add('unused-constraint-default-false',mods,c);CASES[-1]['stations']=next(v['stations'] for v in CASES if v['name']=='threshold-reference')
mods,c=build();unused=empty('unused');unused['constraints']=[dict(op='input',field='value')];mods.append(unused);c['modules']=[pin(m) for m in mods];add('unused-forbidden-constraint',mods,c,'R')
# bool parameters have their own exact metadata schema and no numeric casts.
m=empty('boolean_parameter');m['parameters']=[dict(id='flag',type='bool',label='flag',description='review',default=False)];m['inputs']=[port('pulse','int32_sample')];m['values']=[dict(id='v',type='bool',initial=dict(parameter='flag'))];m['constraints']=[parameter('flag')];m['rules']=[rule('pass',dict(kind='local',ports=['pulse']),parameter('flag'),[])]
i=bareinstance(m,'inst');i['parameters']=dict(flag=literal(True));i['inputs']=dict(pulse='pulse');c=content([m],i);add('bool-parameter-initial-guard-constraint',[m],c);v,t,r=expected_module(m,['inst'],dict(flag=True),dict(pulse='pulse'),{},{});CASES[-1]['stations']=empty_expected(c,v);obj=json.loads(CASES[-1]['stations'][0]);obj['rules']=r;CASES[-1]['stations']=[encode(obj)]
wrong=copy.deepcopy(c);wrong['stations'][0]['instances'][0]['parameters']['flag']=literal(1);add('bool-parameter-integer-binding',[m],wrong,'R')
wrong=copy.deepcopy(m);wrong['parameters'][0]['unit']='none';wc=copy.deepcopy(c);wc['modules']=[pin(wrong)];wc['stations'][0]['instances'][0].update(pin(wrong));add('bool-parameter-extra-numeric-field',[wrong],wc,'R')

mods,c=build();bad=all_solved();bad['moduleId']='unused';bad['rules'][0]['guard']=dict(op='not',arg=dict(op='fact',subject='left'));mods.append(bad);c['modules']=[pin(m) for m in mods];add('unused-negative-fact-guard',mods,c,'R')
# Expanded station budgets are tested independently of template/instance counts.
def one_module_case(name,m,want='A',direct_value=False,direct_timer=False,direct_rule=False):
    i=bareinstance(m,'inst');i['inputs']={p['id']:'pulse' for p in m['inputs']};c=content([m],i)
    if direct_value:c['stations'][0]['values']=[dict(id='direct',type='int32',initial=0)]
    if direct_timer:c['stations'][0]['timers']=[dict(id='direct',durationMs=1)]
    if direct_rule:c['stations'][0]['rules']=[rule('direct',dict(kind='local',ports=['pulse']),literal(True),[])]
    add(name,[m],c,want)
    if want=='A':
        v,t,r=expected_module(m,['inst'],{},i['inputs'],{},{});CASES[-1]['stations']=[encode(dict(format='larp.rules.v1',station='s',values=v,timers=t,ports=c['stations'][0]['ports'],rules=r))]
for count in [32,33]:
    m=empty('many_values');m['values']=[dict(id='v%03d'%n,type='int32',initial=n) for n in range(32)];one_module_case('expanded-values-'+str(count),m,'A' if count==32 else 'R',direct_value=count==33)
for count in [16,17]:
    m=empty('many_timers');m['timers']=[dict(id='t%03d'%n,durationMs=1) for n in range(16)];one_module_case('expanded-timers-'+str(count),m,'A' if count==16 else 'R',direct_timer=count==17)
for count in [64,65]:
    m=empty('many_rules');m['inputs']=[port('pulse','int32_sample')];m['rules']=[rule('r%03d'%n,dict(kind='local',ports=['pulse']),literal(True),[]) for n in range(64)];one_module_case('expanded-rules-'+str(count),m,'A' if count==64 else 'R',direct_rule=count==65)
for count in [512,513]:
    m=empty('many_nodes');m['inputs']=[port('pulse','int32_sample')];m['rules']=[]
    for n in range(64):
        e=literal(True)
        for _ in range(7):e=dict(op='not',arg=e)
        m['rules'].append(rule('r%03d'%n,dict(kind='local',ports=['pulse']),e,[]))
    if count==513:m['rules'][0]['guard']=dict(op='all',args=[literal(True)]*8)
    one_module_case('expanded-nodes-'+str(count),m,'A' if count==512 else 'R')
for count in [16,17]:
    mods=[empty('m%03d'%n) for n in range(count)];c=content(mods,bareinstance(mods[0],'inst'));add('module-definitions-'+str(count),mods,c,'A' if count==16 else 'R')
    if count==16:CASES[-1]['stations']=empty_expected(c)
for count in [8,9]:
    m=empty('many_actions');m['inputs']=[port('pulse','int32_sample')];m['timers']=[dict(id='t%03d'%n,durationMs=1) for n in range(count)];m['rules']=[rule('cancel',dict(kind='local',ports=['pulse']),literal(True),[dict(op='cancel',timer=t['id']) for t in m['timers']])];one_module_case('actions-per-rule-'+str(count),m,'A' if count==8 else 'R')
for count in [8,9]:
    m=empty('deep_expression');m['inputs']=[port('pulse','int32_sample')];e=literal(True)
    for _ in range(count-1):e=dict(op='not',arg=e)
    m['rules']=[rule('deep',dict(kind='local',ports=['pulse']),e,[])];one_module_case('expression-depth-'+str(count),m,'A' if count==8 else 'R')
# Negative-only DAG seam: circular content-addressed hashes require fixed points;
# the constant-digest decorator below must never produce an accepted result.
for mutual in [False,True]:
    mods,c=build();a=empty('unused_a');b=empty('unused_b')
    child=bareinstance(b if mutual else a,'cycle');child['digest']='0'*64;a['instances']=[child]
    mods.append(a)
    if mutual:
        child=bareinstance(a,'cycle');child['digest']='0'*64;b['instances']=[child];mods.append(b)
    c['modules']=[dict(pin(m),digest='0'*64) for m in mods];c['stations'][0]['instances'][0]['digest']='0'*64
    add('uninstantiated-'+('mutual' if mutual else 'self')+'-cycle-negative-mock',mods,c,'R')
    for src in CASES[-1]['sources']:src['digest']='0'*64
    CASES[-1]['hash_failure']=6;CASES[-1]['hash_document']=0;CASES[-1]['error']='Cycle'
# Caller/context operating limits: these are not new P0 language limits.
for count in [128,129]:
    m=empty('interface_profile');m['inputs']=[port('i%03d'%n,'int32_sample') for n in range(count)];one_module_case('interface-profile-'+str(count),m,'A' if count==128 else 'R');CASES[-1]['error']='None' if count==128 else 'OperatingProfileLimit'
for count in [64,65]:
    m=empty('parameter_profile');m['parameters']=[param('p%03d'%n,0,0,1,'none') for n in range(count)];i=bareinstance(m,'inst');i['parameters']={p['id']:literal(0) for p in m['parameters']};c=content([m],i);add('parameter-profile-'+str(count),[m],c,'A' if count==64 else 'R');CASES[-1]['error']='None' if count==64 else 'OperatingProfileLimit'
    if count==64:CASES[-1]['stations']=empty_expected(c)
for count in [4096,4097]:
    m=empty('trigger_profile');m['inputs']=[port('pulse','int32_sample')];m['rules']=[rule('many',dict(kind='local',ports=['pulse']*count),literal(True),[])];one_module_case('caller-trigger-profile-'+str(count),m,'A' if count==4096 else 'R');CASES[-1]['error']='None' if count==4096 else 'OperatingProfileLimit'
for count in [1024,1025]:
    m=empty('scalar_profile');c=content([m],bareinstance(m,'inst'));c['stations'][0]['ports'][0]['capability']='x'*count;add('interned-scalar-profile-'+str(count),[m],c,'A' if count==1024 else 'R');CASES[-1]['error']='None' if count==1024 else 'OperatingProfileLimit'
    if count==1024:CASES[-1]['stations']=empty_expected(c)
# Preserve the existing P1a2 metadata operating count (rules/instances excluded).
for extra in [0,1]:
    m=empty('metadata_profile');c=content([m],bareinstance(m,'inst'));s=c['stations'][0];s['ports']=[dict(id='p%03d'%n,type='int32_sample',direction='input',capability='') for n in range(32)]
    overhead=len(encode(c))-len(encode(s['rules']))-len(encode(s['instances']));remaining=32768+extra-overhead
    for p in s['ports']:
        fill=min(1024,remaining);p['capability']='x'*fill;remaining-=fill
    assert remaining==0
    add('metadata-profile-'+str(32768+extra),[m],c,'A' if extra==0 else 'R')
    if extra==0:CASES[-1]['stations']=empty_expected(c)
(HERE/'vectors.json').write_text(json.dumps(CASES,ensure_ascii=False),encoding='utf8')
print(len(CASES), 'fixed canonical module vectors')
