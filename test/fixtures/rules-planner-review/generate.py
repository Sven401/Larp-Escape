"""Independent witness oracle: exhaustive certificates, not product antichains."""
import itertools,json,hashlib,sys,copy
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from p0_rules_model import Model,check_fixtures
source=ROOT/'docs/spielpakete/fixtures/p0-v1.json'
# Git stores LF; Windows may check out CRLF. Normalize only CRLF pairs.
source_lf=source.read_bytes().replace(b'\r\n',b'\n')
source_sha256=hashlib.sha256(source_lf).hexdigest()
assert source_sha256=='58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605'
assert check_fixtures()==6
original=json.loads(source.read_text(encoding='utf8'));traces=[]
for trace in original['traces']:
    model=Model(trace['spec']);steps=[]
    for step in trace['steps']:
        status=model.at(step['at'],step.get('event'))
        steps.append(dict(input=copy.deepcopy(step),status=status,state=copy.deepcopy(model.state)))
    traces.append(dict(id=trace['id'],spec=copy.deepcopy(trace['spec']),steps=steps))
(HERE/'reference-traces.json').write_text(json.dumps(dict(sourceSha256=source_sha256,sourceHashNormalization='CRLF-to-LF only',historicalWindowsCheckoutRawSha256='473346cfc53aea72de41a9f2be355964c8f7a4d037174a5716f7b22bccd9ac02',traces=traces),ensure_ascii=False),encoding='utf8')

def f(name):return ['fact',name]
def lit(v):return ['literal',v]
def conjunction(*args):return ['all',*args]
def disjunction(*args):return ['any',*args]
def val(v,*refs):return ['stored',v,list(refs)]

def evaluate(expr,certificate):
    op=expr[0]
    if op=='literal':return expr[1]
    if op=='fact':return True if expr[1] in certificate else None
    if op=='stored':return expr[1] if set(expr[2])<=certificate else None
    args=[evaluate(child,certificate) for child in expr[1:]]
    if op=='not':return None if args[0] is None else not args[0]
    if op=='all':return False if any(a is False for a in args) else None if None in args else True
    if op=='any':return True if any(a is True for a in args) else None if None in args else False
    if None in args:return None
    if op=='eq':return type(args[0]) is type(args[1]) and args[0]==args[1]
    raise ValueError(op)

def refs(expr):
    if expr[0]=='fact':return {expr[1]}
    if expr[0]=='stored':return set(expr[2])
    if expr[0]=='literal':return set()
    return set().union(*(refs(c) for c in expr[1:]))

vectors=[]
def witness(name,expr,mandatory=()):
    catalog=sorted(refs(expr)|set(mandatory));available=set(catalog)
    assert evaluate(expr,available) is True
    proof=None
    for size in range(0,min(8,len(catalog))+1):
        for candidate in itertools.combinations(catalog,size):
            candidate=set(candidate)
            if set(mandatory)<=candidate and evaluate(expr,candidate) is True:
                proof=sorted(candidate);break
        if proof is not None:break
    vectors.append(dict(name=name,expression=expr,available=catalog,mandatory=list(mandatory),expected=proof,status='Ready' if proof is not None else 'FactBudget'))
witness('global-overlap',conjunction(disjunction(f('a'),f('z')),f('z')))
witness('global-nine-to-one',conjunction(conjunction(*(disjunction(f('a'+str(i)),f('z')) for i in range(8))),f('z')))
witness('larger-local-alternative-wins',conjunction(disjunction(f('a'),conjunction(f('b'),f('c'))),f('b'),f('c')))
witness('mandatory-base-overlap',disjunction(f('a'),conjunction(f('b'),f('c'))),['b','c'])
witness('canonical-tie',disjunction(f('z'),f('a')))
witness('all-needs-eight',conjunction(*(f('f'+str(i)) for i in range(8))))
witness('nine-genuinely-necessary',conjunction(conjunction(*(f('f'+str(i)) for i in range(8))),f('f8')))
witness('irrelevant-overbudget-branch',disjunction(conjunction(conjunction(*(f('f'+str(i)) for i in range(8))),f('f8')),lit(True)))
witness('false-all-alternative', ['not',conjunction(val(False,'a'),val(False,'z'))])
witness('false-all-base-overlap', ['not',conjunction(val(False,'a'),val(False,'z'))],['z'])
witness('false-any-union', ['not',disjunction(val(False,'a'),val(False,'z'))])
witness('true-any-literal-witness',disjunction(f('a'),lit(True)))
(HERE/'witness-goldens.json').write_text(json.dumps(vectors,ensure_ascii=False),encoding='utf8')
print(len(traces),'unchanged reference traces',sum(len(t['steps']) for t in traces),'steps;',len(vectors),'exhaustive witness goldens')
# Consumer-specific D0 dependency limits. Successful non-Fact consumers may
# require more than eight references; no global eight-reference cap is implied.
consumer_cases=[]
for purpose in ['effect','no_actions','emit']:
    expr=conjunction(conjunction(*(f('f'+str(i)) for i in range(8))),f('f8'))
    consumer_cases.append(dict(name='nine-ref-guard-'+purpose,guard=expr,available=['f'+str(i) for i in range(9)],purpose=purpose,expected='Ready' if purpose!='emit' else 'FactBudget',expectedProof=['f'+str(i) for i in range(9)]))
consumer_cases.append(dict(name='eight-new-facts-sixtyfour-plan-refs',rules=[dict(id='r'+str(i),guard=conjunction(*(f('f%02d'%j) for j in range(i*8,(i+1)*8))),emit='out'+str(i),expectedProof=['f%02d'%j for j in range(i*8,(i+1)*8)]) for i in range(8)],available=['f%02d'%i for i in range(64)],expected='Ready',expectedNewFacts=8,expectedUnionRefs=64))
consumer_cases.append(dict(name='false-guard-no-proof-frontier-needed',guard=conjunction(lit(False),conjunction(*(disjunction(f('a'+str(i)),f('z'+str(i))) for i in range(8)))),available=['a'+str(i) for i in range(8)]+['z'+str(i) for i in range(8)],witnessCapacity=0,expected='ReadyReceiptOnly',expectedNewFacts=0))
consumer_cases.append(dict(name='false-guard-still-eager-overflow',guard=['all',lit(False),['eq',['add',lit(2147483647),lit(1)],lit(0)]],available=[],expected='OverflowNoCommit'))
consumer_cases.append(dict(name='trigger-fact-audit-separate',guard=conjunction(*(f('f'+str(i)) for i in range(8))),triggerFact='wake',available=['wake']+['f'+str(i) for i in range(8)],purpose='emit',expected='Ready',expectedProof=['f'+str(i) for i in range(8)],expectedReceiptTrigger='wake'))
consumer_cases.append(dict(name='timer-arm-causality-mandatory',guard=lit(True),timerArmProof=['historical'],purpose='emit',expected='Ready',expectedProof=['historical']))
(HERE/'consumer-goldens.json').write_text(json.dumps(consumer_cases,ensure_ascii=False),encoding='utf8')
print(len(consumer_cases),'consumer/provenance cap counterexamples')
# Canonical generic module packages for the SAME decoded six reference specs.
def canonical(x):return json.dumps(x,sort_keys=True,separators=(',',':'),ensure_ascii=False)
def expression(x):
    op=x[0]
    if op=='literal':return dict(op=op,type='bool' if type(x[1]) is bool else 'int32',value=x[1])
    if op in ['value','port_is']:return dict(op=op,id=x[1])
    if op=='fact':return dict(op=op,subject=x[1])
    if op=='input':return dict(op=op,field='value')
    if op=='not':return dict(op=op,arg=expression(x[1]))
    if op in ['all','any']:return dict(op=op,args=[expression(a) for a in x[1:]])
    return dict(op=op,left=expression(x[1]),right=expression(x[2]))
def gather(x,op):
    if x[0]==op:return {x[1]}
    if x[0] in ['literal','input','value','port_is','fact']:return set()
    return set().union(*(gather(c,op) for c in x[1:]))
def package_for_trace(trace):
    spec=trace['spec'];inputs=set();facts=set();outputs=set();effects=set();rules=[]
    for r in spec['rules']:
        tr=r['trigger'];facts|=gather(r['guard'],'fact')
        if tr[0]=='local':inputs.update(tr[1])
        if tr[0]=='fact':facts.update(tr[1])
        actions=[]
        for a in r['actions']:
            op,target=a[:2]
            if op in ['set','add']:actions.append(dict(op=op,id=target,expr=expression(a[2])))
            elif op in ['arm','cancel']:actions.append(dict(op=op,timer=target))
            elif op=='emit':outputs.add(target);actions.append(dict(op=op,subject=target))
            else:effects.add(target);actions.append(dict(op=op,port=target,resource='preset'))
        trigger=dict(kind=tr[0],**({'ports':tr[1]} if tr[0]=='local' else {'subjects':tr[1]} if tr[0]=='fact' else {'timer':tr[1]}))
        rules.append(dict(id=r['id'],generation=r.get('generation',1),priority=r['priority'],mode=r['mode'],trigger=trigger,guard=expression(r['guard']),offline=dict(policy='wait' if tr[0]=='fact' else 'local_continue'),actions=actions))
    port=lambda name,typ:dict(id=name,type=typ,label=name,description='independent P0 review')
    module=dict(format='larp.module.v1',moduleId='generic_reference',version='1.0.0',label='generic reference',parameters=[],constraints=[],instances=[],inputs=sorted([port(p,'int32_sample') for p in inputs]+[port(s,'solved') for s in facts-outputs],key=lambda p:p['id']),outputs=sorted([port(s,'solved') for s in outputs]+[port(p,'light') for p in effects],key=lambda p:p['id']),values=[dict(id=n,type='bool' if type(v) is bool else 'int32',initial=v) for n,v in sorted(spec['values'].items())],timers=[dict(id=n,durationMs=v) for n,v in sorted(spec['timers'].items())],rules=sorted(rules,key=lambda r:r['id']),resources=[dict(id='preset',kind='light_preset')] if effects else [])
    raw=canonical(module);pin=dict(moduleId=module['moduleId'],version=module['version'],digest=hashlib.sha256(raw.encode()).hexdigest());inst=dict(pin,id='inst',parameters={},inputs={p:p for p in sorted(inputs|facts-outputs)},outputs={p:p for p in sorted(outputs|effects)},resources=dict(preset='preset') if effects else {})
    stations=[dict(id='s',label='station',values=[],timers=[],rules=[],instances=[inst],ports=sorted([dict(id=p,type='int32_sample',direction='input',capability='') for p in inputs]+[dict(id=p,type='light',direction='output',capability='') for p in effects],key=lambda p:p['id']))]
    if facts-outputs:stations.insert(0,dict(id='remote',label='remote',values=[],timers=[],rules=[],instances=[],ports=[]))
    content=dict(format='larp.package.v1',packageId='review_reference',label='review reference',revision=1,requires=[],modules=[pin],resources=[dict(id='preset',kind='light_preset',digest='a'*64,bytes=1)] if effects else [],stations=stations,subjects=[dict(id=s,originStation='s' if s in outputs else 'remote') for s in sorted(facts|outputs)])
    return dict(id=trace['id'],content=canonical(content),module=dict(pin,bytes=raw),ruleBindings=['/inst/'+r['id'] for r in sorted(rules,key=lambda r:r['id'])],subjects=[dict(name=s,local=s in outputs) for s in sorted(facts|outputs)])
packages=[package_for_trace(t) for t in original['traces']]
(HERE/'reference-packages.json').write_text(json.dumps(packages,ensure_ascii=False),encoding='utf8')
print(len(packages),'ordinary canonical module packages for reference chain')
# Concrete module-chain scenarios for the independently exhaustive witnesses.
scenarios=[]
def allrefs(names):return ['fact',names[0]] if len(names)==1 else ['all',*(['fact',n] for n in names)] if names else ['literal',True]
def proof_scenario(golden,purpose='emit'):
    stored=[]
    def replace(e):
        if e[0]=='stored':
            n='v'+str(len(stored));stored.append((n,e[1],e[2]));return ['value',n]
        if e[0] in ['fact','literal','value','input','port_is']:return copy.deepcopy(e)
        return [e[0],*(replace(c) for c in e[1:])]
    guard=replace(golden['expression']);mandatory=golden['mandatory'];available=golden['available'];rules=[];steps=[];values={}
    for ref in available:steps.append(dict(at=0,event=dict(kind='fact',id=ref,subject=ref)))
    for n,v,causes in stored:
        values[n]=v;rules.append(dict(id='seed_'+n,priority=1,mode='each',trigger=['local',['seed_'+n]],guard=allrefs(causes),actions=[['set',n,['literal',v]]]))
        steps.append(dict(at=0,event=dict(kind='local',id='seed_'+n,port='seed_'+n,value=1)))
    if mandatory:
        rules.append(dict(id='arm',priority=1,mode='each',trigger=['local',['arm']],guard=allrefs(mandatory),actions=[['arm','window']]))
        steps.append(dict(at=0,event=dict(kind='local',id='arm',port='arm',value=1)))
        trigger=['timer','window'];steps.append(dict(at=1))
    else:trigger=['local',['fire']];steps.append(dict(at=1,event=dict(kind='local',id='fire',port='fire',value=1)))
    actions=[['emit','out']] if purpose=='emit' else [['effect','light']] if purpose=='effect' else []
    rules.append(dict(id='proof',priority=10,mode='once',trigger=trigger,guard=guard,actions=actions))
    trace=dict(id=golden['name'],spec=dict(values=values,timers=dict(window=1) if mandatory else {},rules=rules),steps=steps)
    package=package_for_trace(trace)
    scenarios.append(dict(name=golden['name'],package=package,trace=dict(id=trace['id'],steps=[dict(input=s) for s in steps]),expected=golden['status'],expectedProof=golden['expected'],purpose=purpose))
for golden in vectors:proof_scenario(golden)
for purpose in ['effect','no_actions']:
    proof_scenario(dict(name='nine-refs-'+purpose,expression=['all',['all',*(['fact','f'+str(i)] for i in range(8))],['fact','f8']],available=['f'+str(i) for i in range(9)],mandatory=[],expected=['f'+str(i) for i in range(9)],status='Ready'),purpose)
(HERE/'proof-scenarios.json').write_text(json.dumps(scenarios,ensure_ascii=False),encoding='utf8')
print(len(scenarios),'concrete compiler-chain proof scenarios')
def extra(name,spec,steps,expect,**checks):
    trace=dict(id=name,spec=spec,steps=steps);case=dict(name=name,package=package_for_trace(trace),trace=dict(id=name,steps=[dict(input=s) for s in steps]),expected=expect,**checks);scenarios.append(case);return case
def admitted(names):return [dict(at=0,event=dict(kind='fact',id=n,subject=n)) for n in names]
def fire():return dict(at=1,event=dict(kind='local',id='fire',port='fire',value=1))
def r(name,guard,actions,trigger=None):return dict(id=name,priority=10,mode='once',trigger=trigger or ['local',['fire']],guard=guard,actions=actions)
facts=['f%02d'%i for i in range(64)]
rules=[r('r'+str(i),allrefs(facts[i*8:(i+1)*8]),[['emit','out'+str(i)]],['fact',['wake']]) for i in range(8)]
extra('eight-facts-sixtyfour-plan-union',dict(values={},timers={},rules=rules),admitted(facts)+[dict(at=1,event=dict(kind='fact',id='wake',subject='wake'))],'Ready',emissionProofs=[facts[i*8:(i+1)*8] for i in range(8)],unionRefs=64)
c=consumer_cases[4];case=extra(c['name'],dict(values={},timers={},rules=[r('proof',c['guard'],[['emit','out']])]),admitted(c['available'])+[fire()],'ReadyReceiptOnly');case['package']['proofCapacity']=0
c=consumer_cases[5];extra(c['name'],dict(values={},timers={},rules=[r('proof',c['guard'],[['emit','out']])]),[fire()],'Overflow')
c=consumer_cases[6];extra(c['name'],dict(values={},timers={},rules=[r('proof',c['guard'],[['emit','out']],['fact',['wake']])]),admitted(['f'+str(i) for i in range(8)])+[dict(at=1,event=dict(kind='fact',id='wake',subject='wake'))],'Ready',purpose='emit',expectedProof=c['expectedProof'],receipt='wake')
for shared in [False,True]:
    spec=dict(values={},timers={},rules=[r('a',['literal',True],[['effect','pa']]),r('z',['literal',True],[['effect','pz']])]);case=extra('physical-'+('alias-conflict' if shared else 'distinct-resources'),spec,[fire()],'Conflict' if shared else 'Ready',intents=[] if shared else ['pa','pz']);case['package']['exclusiveKeys']=dict(pa=1,pz=1 if shared else 2)
spec=dict(values={},timers={},rules=[r('a',['literal',True],[['effect','pa']]),r('z',['literal',True],[['effect','pz']])]);case=extra('stable-rule-id-orders-before-display-name',spec,[fire()],'Ready',intents=['pz','pa'],latches=['z','a']);case['package']['ruleIdentities']={'/inst/a':255,'/inst/z':1}
# A proof-space limit is a retryable operating profile, not invalid language.
case=extra('proof-space-exhaustion-defers',dict(values={},timers={},rules=[r('proof',['any',['fact','a'],['fact','z']],[['emit','out']])]),admitted(['a','z'])+[fire()],'Deferred');case['package']['proofCapacity']=1
# Committed local drafts precede their separate loopback D0 inputs.
spec=dict(values={},timers={},rules=[r('produce',['literal',True],[['emit','out']])]);extra('self-echo-after-emit-and-intervening-local-input',spec,[dict(at=0,event=dict(kind='local',id='fire',port='fire',value=1,sequence=1)),dict(at=1,event=dict(kind='local',id='next',port='fire',value=1,sequence=2)),dict(at=2,event=dict(kind='fact',id='echo',subject='out',sequence=1))],'Ready',emissions=['out'],ownKnown=['out'])
spec=dict(values={},timers={},rules=[r('never',['literal',False],[['emit','out']])]);extra('own-future-sequence-rejected',spec,[dict(at=0,event=dict(kind='fact',id='forged',subject='out',sequence=1))],'Rejected')
spec=dict(values={},timers={},rules=[r('never',['literal',False],[['emit','out']]),r('produce',['literal',True],[['emit','issued']])]);extra('own-issued-sequence-unsolved-subject-rejected',spec,[dict(at=0,event=dict(kind='local',id='setup',port='fire',value=1,sequence=2)),dict(at=1,event=dict(kind='fact',id='forged',subject='out',sequence=1))],'Rejected',emissions=['issued'])
(HERE/'proof-scenarios.json').write_text(json.dumps(scenarios,ensure_ascii=False),encoding='utf8')
print(len(scenarios),'concrete full-chain planner scenarios')
# Exact full-plan action, fact and intent limits; no prefix may be committed.
for count in [32,33]:
    vals={'v%02d'%i:0 for i in range(32)};actions=[['set',n,['literal',1]] for n in sorted(vals)]
    if count==33:actions.append(['cancel','t'])
    rules=[r('r%02d'%(i//8),['literal',True],actions[i:i+8]) for i in range(0,len(actions),8)]
    extra('plan-actions-'+str(count),dict(values=vals,timers={'t':1},rules=rules),[fire()],'Ready' if count==32 else 'Budget',values={n:1 if count==32 else 0 for n in vals})
for rearm in [False,True]:
    vals={'v%02d'%i:0 for i in range(32)};actions=[['set',n,['literal',1]] for n in sorted(vals)[:31 if rearm else 32]]
    if rearm:actions.append(['arm','t'])
    rules=[dict(r('arm',['literal',True],[['arm','t']],['local',['arm']]),mode='each')]
    rules += [r('r%02d'%(i//8),['literal',True],actions[i:i+8],['timer','t']) for i in range(0,32,8)]
    extra('implicit-consume-plus-32-'+('rearm' if rearm else 'writes'),dict(values=vals,timers={'t':1},rules=rules),[dict(at=0,event=dict(kind='local',id='arm',port='arm',value=1)),dict(at=1)],'Ready',values={n:0 if rearm and n=='v31' else 1 for n in vals},timers={'t':[2,2]} if rearm else {})
for kind in ['facts','intents']:
    for count in [8,9]:
        actions=[['emit','o%02d'%i] if kind=='facts' else ['effect','p%02d'%i] for i in range(count)]
        rules=[r('r%02d'%(i//8),['literal',True],actions[i:i+8]) for i in range(0,count,8)]
        extra('new-'+kind+'-'+str(count),dict(values={},timers={},rules=rules),[fire()],'Ready' if count==8 else 'Budget',**({'emissions':['o%02d'%i for i in range(count)] if count==8 else []} if kind=='facts' else {'intents':['p%02d'%i for i in range(count)] if count==8 else []}))
spec=dict(values={'x':0},timers={},rules=[dict(r('count',['literal',True],[['add','x',['literal',1]]]),mode='each')])
case=extra('ledger-full-no-eviction',spec,[dict(at=0,event=dict(kind='local',id='one',port='fire',value=1)),dict(at=1,event=dict(kind='local',id='two',port='fire',value=1))],'Deferred',values={'x':1},operations=['one']);case['package']['receiptCapacity']=1
extra('operation-rebinding-conflict',spec,[dict(at=0,event=dict(kind='local',id='op',port='fire',value=1)),dict(at=1,event=dict(kind='local',id='op',port='fire',value=2))],'Conflict',values={'x':1},operations=['op'])
for fault,name in enumerate(['progress','profile','trust','facts','ledger','resources','rights','capacity','lastIssued','activeTime','mode','context'],1):
    extra('commit-fence-'+name,dict(values={'x':0},timers={},rules=[r('set',['literal',True],[['set','x',['literal',1]]])]),[dict(at=1,event=dict(kind='local',id='op',port='fire',value=1,fault=fault))],'Stale',values={'x':0},operations=[])
extra('commit-staging-alias-rejected',dict(values={'x':0},timers={},rules=[r('set',['literal',True],[['set','x',['literal',1]]])]),[dict(at=1,event=dict(kind='local',id='op',port='fire',value=1,fault=200))],'InvalidSnapshot',values={'x':0},operations=[])
extra('commit-evidence-capacity-deferred',dict(values={'x':0},timers={},rules=[r('set',['fact','f'],[['set','x',['literal',1]]])]),admitted(['f'])+[dict(at=1,event=dict(kind='local',id='op',port='fire',value=1,fault=100))],'Deferred',values={'x':0},operations=['f'])
(HERE/'proof-scenarios.json').write_text(json.dumps(scenarios,ensure_ascii=False),encoding='utf8')
print(len(scenarios),'including independent limits, revision fences, dedup and staging counterexamples')
for reverse in [False,True]:
    steps=[dict(at=0,event=dict(kind='fact',id='a',subject='a',sequence=100)),dict(at=0,event=dict(kind='fact',id='z',subject='z',sequence=1))]
    if reverse:steps.reverse()
    extra('factid-tie-'+('reverse' if reverse else 'forward')+'-arrival',dict(values={},timers={},rules=[r('proof',['any',['fact','a'],['fact','z']],[['emit','out']])]),steps+[fire()],'Ready',purpose='emit',expectedProof=['z'])
extra('same-factid-digest-changed-subject-conflict',dict(values={},timers={},rules=[r('unused',['all',['fact','a'],['fact','b']],[])]),[dict(at=0,event=dict(kind='fact',id='one',subject='a',sequence=1)),dict(at=1,event=dict(kind='fact',id='two',subject='b',sequence=1))],'Conflict',ownKnown=['a'],operations=['one'])
(HERE/'proof-scenarios.json').write_text(json.dumps(scenarios,ensure_ascii=False),encoding='utf8')
print(len(scenarios),'including FactId tuple ordering and metadata conflict')
case=extra('extra-rule-binding-rejected',dict(values={},timers={},rules=[r('proof',['literal',True],[['emit','out']])]),[fire()],'BuilderRejected');case['package']['ruleBindings'].append('/unknown');case['package']['expectBuilderRejected']=True
case=extra('extra-subject-binding-rejected',dict(values={},timers={},rules=[r('proof',['literal',True],[['emit','out']])]),[fire()],'BuilderRejected');case['package']['subjects'].append(dict(name='unknown',local=False));case['package']['expectBuilderRejected']=True
(HERE/'proof-scenarios.json').write_text(json.dumps(scenarios,ensure_ascii=False),encoding='utf8')
print(len(scenarios),'including exact binding-map rejection cases')
