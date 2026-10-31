"""Independent actual v2 source/CBOR/IR bundles; owner/D1/signing qualification is separate."""
from BaseFixtures import *
HERE=Path(__file__).resolve().parent

def group_module():
    m=module(True);m['format']='larp.module.v2';original=m['rules'][0];original.update(generation=2,priority=10,offline=dict(policy='local_fallback',rule='manual'))
    replacement=copy.deepcopy(original);replacement.update(id='manual',generation=3,priority=20,trigger=dict(kind='override'),guard=literal(True),offline=dict(policy='local_continue'));replacement['actions'][0]['op']='override';m['rules']=[original,replacement];return m

def qname(prefix,id):return '/'+('/'.join(s.replace('~','~0').replace('/','~1') for s in prefix+[id]))
def expression(expr,prefix,slots):
    out=copy.deepcopy(expr)
    if not isinstance(out,dict):return out
    for key,value in list(out.items()):
        if isinstance(value,dict):out[key]=expression(value,prefix,slots)
        elif isinstance(value,list):out[key]=[expression(x,prefix,slots) for x in value]
    if out.get('op')=='fact':out['subject']=slots.get(out['subject'],out['subject'])
    if out.get('op')=='port_is':out['port']=slots.get(out['port'],out['port'])
    return out

def expand_rules(definition,prefix,slots,resources,definitions):
    rules=[]
    for r in definition['rules']:
        r=copy.deepcopy(r);r['id']=qname(prefix,r['id']);r['guard']=expression(r['guard'],prefix,slots)
        if 'rule' in r['offline']:r['offline']['rule']=qname(prefix,r['offline']['rule'])
        trigger=r['trigger']
        if trigger['kind']=='fact':trigger['subjects']=[slots.get(s,s) for s in trigger['subjects']]
        elif trigger['kind']=='local':trigger['ports']=[slots.get(s,s) for s in trigger['ports']]
        for action in r['actions']:
            if action['op'] in ['emit','override']:action['subject']=slots.get(action['subject'],action['subject'])
            elif action['op']=='effect':action['port']=slots.get(action['port'],action['port']);action['resource']=resources.get(action['resource'],action['resource'])
        rules.append(r)
    for inst in definition.get('instances',[]):
        child=definitions[(inst['moduleId'],inst['version'])];childslots={k:slots.get(v,v) for k,v in {**inst['inputs'],**inst['outputs']}.items()};childresources={k:resources.get(v,v) for k,v in inst['resources'].items()};rules.extend(expand_rules(child,prefix+[inst['id']],childslots,childresources,definitions))
    return rules

def finalize(c,regenerate_profiles=True):
    content=json.loads(data(c,'content.json'));definitions={(p['moduleId'],p['version']):json.loads(data(c,p['path'])) for p in c['modulePins']};pins={(p['moduleId'],p['version']):p for p in c['modulePins']};done={}
    def rehash(key):
        if key in done:return done[key]
        m=definitions[key]
        for inst in m.get('instances',[]):
            child=(inst['moduleId'],inst['version'])
            if child in pins:inst['digest']=rehash(child)
        raw=canonical(m);setdata(c,pins[key]['path'],raw);pins[key]['digest']=sha(raw);done[key]=sha(raw);return done[key]
    for key in pins:rehash(key)
    for p in content['modules']:
        if (p['moduleId'],p['version']) in done:p['digest']=done[(p['moduleId'],p['version'])]
    for station in content['stations']:
        for inst in station['instances']:
            if (inst['moduleId'],inst['version']) in done:inst['digest']=done[(inst['moduleId'],inst['version'])]
    setdata(c,'content.json',canonical(content));d=deployment(c);d['contentDigest']=sha(canonical(content));d['requires']=copy.deepcopy(content['requires'])
    if regenerate_profiles:
        for station in content['stations']:
            rules=sorted(expand_rules(station,[],{}, {},definitions),key=lambda r:r['id'])
            profile=canonical(dict(format='larp.rules.v2' if content['format']=='larp.package.v2' else 'larp.rules.v1',station=station['id'],values=station['values'],timers=station['timers'],ports=station['ports'],rules=rules))
            path='profiles/'+station['id']+'.json';setdata(c,path,profile);c['expectedProfiles'][station['id']]=profile.hex();p=next(p for p in d['profiles'] if p['station']==station['id']);p.update(format='larp.rules.v2' if content['format']=='larp.package.v2' else 'larp.rules.v1',digest=sha(profile),bytes=len(profile))
    setdeployment(c,d);c['sourceMetadataBytes']=source_metadata(content,list(definitions.values()));return c

def v2():
    c=build(2);c['name']='valid-full-v2-independent';content=json.loads(data(c,'content.json'));content['format']='larp.package.v2';content['requires'].append(dict(name='rules_manual_override',version=1));content['requires'].sort(key=lambda x:x['name']);setdata(c,'content.json',canonical(content));setdata(c,'modules/gate.json',canonical(group_module()));d=deployment(c);original=d['rules'][1];original.update(generation=2,offline=dict(policy='local_fallback',rule='/inst/manual'));manual=copy.deepcopy(original);manual.update(expandedId='/inst/manual',generation=3,offline=dict(policy='local_continue'));d['rules'].append(manual);setdeployment(c,d);b=binding(1);b[7].insert(1,[2,identity(0xA001)]);replace_binding(c,1,cbor(b));c['selectedStation']='s1';c['owner'].update(box=identity(0xB001).hex(),epoch=identity(0xD003).hex());c['expectedSelected'].update(station=identity(0x5001).hex(),box=c['owner']['box'],epoch=c['owner']['epoch']);finalize(c);c['expectedProgram']=dict(language=2,manualOverride=True,permissions={'done_s0':2,'done_s1':6},groups=[dict(original='/inst/finish',replacement='/inst/manual',stable=identity(0xC001).hex(),originalGeneration=2,replacementGeneration=3)]);return c

CASES=[v2()]
for edits in ['different-priority-generation','empty-unselected-epoch-list','same-stable-id-different-station']:
    c=v2();c['name']='valid-'+edits
    if edits=='different-priority-generation':
        m=json.loads(data(c,'modules/gate.json'));m['rules'][0].update(priority=65535,generation=2147483647);m['rules'][1].update(priority=0,generation=1);setdata(c,'modules/gate.json',canonical(m));d=deployment(c);d['rules'][1]['generation']=2147483647;d['rules'][2]['generation']=1;setdeployment(c,d);c['expectedProgram']['groups'][0].update(originalGeneration=2147483647,replacementGeneration=1);finalize(c)
    elif edits=='empty-unselected-epoch-list':
        b=binding(0);b[6]=[];replace_binding(c,0,cbor(b))
    else:
        d=deployment(c);d['rules'][0]['ruleId']=d['rules'][1]['ruleId'];setdeployment(c,d)
    CASES.append(c)
for mode in ['content-v1','module-v1','content-cap-missing','content-cap-wrong','manifest-cap-missing','manifest-cap-wrong','mixed-profile-version','unknown-group-json']:
    c=v2();c['name']='gate-'+mode;c['expected']='Rejected'
    if mode.startswith('content-'):
        content=json.loads(data(c,'content.json'))
        if mode=='content-v1':content['format']='larp.package.v1'
        elif mode=='content-cap-missing':content['requires']=[r for r in content['requires'] if r['name']!='rules_manual_override']
        else:next(r for r in content['requires'] if r['name']=='rules_manual_override')['version']=2
        setdata(c,'content.json',canonical(content));finalize(c)
    elif mode=='module-v1':m=json.loads(data(c,'modules/gate.json'));m['format']='larp.module.v1';setdata(c,'modules/gate.json',canonical(m));finalize(c)
    elif mode.startswith('manifest-cap-'):
        d=deployment(c)
        if mode.endswith('missing'):d['requires']=[r for r in d['requires'] if r['name']!='rules_manual_override']
        else:next(r for r in d['requires'] if r['name']=='rules_manual_override')['version']=2
        setdeployment(c,d)
    elif mode=='mixed-profile-version':
        raw=json.loads(data(c,'profiles/s0.json'));raw['format']='larp.rules.v1';value=canonical(raw);setdata(c,'profiles/s0.json',value);d=deployment(c);d['profiles'][0].update(format='larp.rules.v1',bytes=len(value),digest=sha(value));setdeployment(c,d)
    else:
        m=json.loads(data(c,'modules/gate.json'));m['rules'][0]['group']='forged';setdata(c,'modules/gate.json',canonical(m));finalize(c)
    CASES.append(c)
for mode in ['different-stable-id','missing-t1','missing-t2','missing-static-union','extra-static-dependency','wrong-qualified-target','wrong-generation-binding']:
    c=v2();c['name']='binding-'+mode;c['expected']='Rejected';d=deployment(c)
    if mode=='different-stable-id':d['rules'][2]['ruleId']=identity(0xFFFF).hex()
    elif mode=='missing-static-union':d['dependencies']=[]
    elif mode=='extra-static-dependency':d['dependencies'].append(dict(station='s1',ruleId=d['rules'][1]['ruleId'],originStation='s1',subject='done_s1'))
    elif mode=='wrong-qualified-target':d['rules'][1]['offline']['rule']='/neighbor/manual'
    elif mode=='wrong-generation-binding':d['rules'][2]['generation']=2
    else:
        b=binding(1);b[7].insert(1,[2,identity(0xA001)]);b[7]=[p for p in b[7] if p[0]!=(1 if mode=='missing-t1' else 2)];replace_binding(c,1,cbor(b));d=deployment(c)
    setdeployment(c,d);CASES.append(c)
for mode in ['pure-effect-pair','wrong-effect-resource','reordered-actions','duplicate-subject','replacement-each','original-each','replacement-local-trigger','replacement-extra-trigger-field','negative-original-fact','replacement-input','replacement-port-is','original-override-action','replacement-emit-action','replacement-chain','foreign-fallback','no-fallback']:
    c=v2();c['name']='group-'+mode;c['expected']='Rejected';m=json.loads(data(c,'modules/gate.json'));a,b=m['rules']
    if mode=='pure-effect-pair':a['actions']=a['actions'][1:];b['actions']=b['actions'][1:]
    elif mode=='wrong-effect-resource':b['actions'][1]['resource']='unused'
    elif mode=='reordered-actions':b['actions'].reverse()
    elif mode=='duplicate-subject':a['actions'].append(copy.deepcopy(a['actions'][0]));b['actions'].append(copy.deepcopy(b['actions'][0]))
    elif mode=='replacement-each':b['mode']='each'
    elif mode=='original-each':a['mode']='each'
    elif mode=='replacement-local-trigger':b['trigger']=dict(kind='local',ports=['previous'])
    elif mode=='replacement-extra-trigger-field':b['trigger']['subjects']=[]
    elif mode=='negative-original-fact':a['guard']=dict(op='not',arg=a['guard'])
    elif mode=='replacement-input':b['guard']=dict(op='eq',left=dict(op='input',field='value'),right=literal(1))
    elif mode=='replacement-port-is':b['guard']=dict(op='port_is',port='light')
    elif mode=='original-override-action':a['actions'][0]['op']='override'
    elif mode=='replacement-emit-action':b['actions'][0]['op']='emit'
    elif mode=='replacement-chain':b['offline']=dict(policy='local_fallback',rule='finish')
    elif mode=='foreign-fallback':a['offline']['rule']='neighbor/manual'
    else:a['offline']=dict(policy='wait')
    setdata(c,'modules/gate.json',canonical(m));finalize(c);CASES.append(c)

def bind_group_names(c,original,replacement):
    d=deployment(c);g=c['expectedProgram']['groups'][0];old_original,old_replacement=g['original'],g['replacement']
    for r in d['rules']:
        if r['station']!='s1':continue
        if r['expandedId']==old_original:r['expandedId']=original;r['offline']['rule']=replacement
        elif r['expandedId']==old_replacement:r['expandedId']=replacement
    d['rules'].sort(key=lambda r:(r['station'],r['expandedId']));setdeployment(c,d);g.update(original=original,replacement=replacement)

def wrap(c,id,childid,language=2):
    content=json.loads(data(c,'content.json'));root=content['stations'][1]['instances'][0];old=copy.deepcopy(root);definition=group_module();definition.update(format='larp.module.v'+str(language),moduleId=id,label='independent wrapper '+id,rules=[]);old.update(id=childid,inputs={'previous':'previous'},outputs={'done':'done','light':'light'},resources={'preset':'preset'});definition['instances']=[old];path='modules/'+id+'.json';raw=canonical(definition);c['sources'].append(dict(role='module',path=path,hex=raw.hex()));c['modulePins'].append(dict(pin(definition),path=path));content['modules'].append(pin(definition));content['modules'].sort(key=lambda p:(p['moduleId'],p['version']));root.update(pin(definition));setdata(c,'content.json',canonical(content));g=c['expectedProgram']['groups'][0];segment=childid.replace('~','~0').replace('/','~1');bind_group_names(c,'/inst/'+segment+g['original'][len('/inst'):],'/inst/'+segment+g['replacement'][len('/inst'):]);return finalize(c)

for mode in ['renamed-module','escaped-instance','escaped-rule','one-nested-v2','ordinary-v1-wrapper-v2-child','max-five-name-segments']:
    c=v2();c['name']='valid-generic-'+mode
    if mode=='renamed-module':
        m=json.loads(data(c,'modules/gate.json'));m['moduleId']='renamed_gate';m['label']='semantically equal, different module id';setdata(c,'modules/gate.json',canonical(m));c['modulePins'][1]['moduleId']='renamed_gate';content=json.loads(data(c,'content.json'));content['modules'][1]['moduleId']='renamed_gate';content['stations'][1]['instances'][0]['moduleId']='renamed_gate';setdata(c,'content.json',canonical(content));finalize(c)
    elif mode=='escaped-instance':
        content=json.loads(data(c,'content.json'));content['stations'][1]['instances'][0]['id']='scope/~';setdata(c,'content.json',canonical(content));bind_group_names(c,qname(['scope/~'],'finish'),qname(['scope/~'],'manual'));finalize(c)
    elif mode=='escaped-rule':
        m=json.loads(data(c,'modules/gate.json'));m['rules'][0]['id']='first/~';m['rules'][0]['offline']['rule']='second/~';m['rules'][1]['id']='second/~';setdata(c,'modules/gate.json',canonical(m));bind_group_names(c,qname(['inst'],'first/~'),qname(['inst'],'second/~'));finalize(c)
    elif mode=='one-nested-v2':wrap(c,'wrapper','inner')
    elif mode=='ordinary-v1-wrapper-v2-child':wrap(c,'wrapper','inner',1)
    else:
        for i in range(3):wrap(c,'wrapper'+str(i),'inner'+str(i))
    CASES.append(c)
for mode in ['valid-unused-v2','invalid-unused-v2-orphan','invalid-unused-v1-override']:
    c=v2();c['name']=mode;m=group_module();m['moduleId']='unused_module';m['label']='unused definition still checked'
    if mode=='invalid-unused-v2-orphan':m['rules']=m['rules'][1:]
    elif mode=='invalid-unused-v1-override':m['format']='larp.module.v1'
    if mode!='valid-unused-v2':c['expected']='Rejected'
    raw=canonical(m);path='modules/unused_module.json';c['sources'].append(dict(role='module',path=path,hex=raw.hex()));c['modulePins'].append(dict(pin(m),path=path));content=json.loads(data(c,'content.json'));content['modules'].append(pin(m));content['modules'].sort(key=lambda p:(p['moduleId'],p['version']));setdata(c,'content.json',canonical(content));finalize(c);CASES.append(c)
# A source alias or merely naming the shared ID never turns an unrelated rule into a member.
for mode in ['extra-normal-rule-same-stable-id','extra-producer-outside-group','fallback-neighbor-instance']:
    c=v2();c['name']=mode;c['expected']='Rejected'
    if mode=='extra-normal-rule-same-stable-id':
        content=json.loads(data(c,'content.json'));content['stations'][1]['ports'].append(dict(id='sample',direction='input',type='boolean_sample',capability='review.sample.v1'));content['stations'][1]['ports'].sort(key=lambda p:p['id']);content['stations'][1]['rules']=[dict(id='z_unrelated',generation=1,priority=1,mode='once',trigger=dict(kind='local',ports=['sample']),guard=literal(True),offline=dict(policy='local_continue'),actions=[])];setdata(c,'content.json',canonical(content));d=deployment(c);d['stations'][1]['ports'].append(dict(id='sample',componentId=3,channel=0,resources=[]));d['stations'][1]['ports'].sort(key=lambda p:p['id']);d['rules'].append(dict(station='s1',expandedId='/z_unrelated',ruleId=identity(0xC001).hex(),generation=1,offline=dict(policy='local_continue')));d['rules'].sort(key=lambda r:(r['station'],r['expandedId']));setdeployment(c,d)
    elif mode=='extra-producer-outside-group':
        m=json.loads(data(c,'modules/gate.json'));r=copy.deepcopy(m['rules'][0]);r.update(id='z_competitor',offline=dict(policy='wait'));m['rules'].append(r);setdata(c,'modules/gate.json',canonical(m))
    else:
        m=json.loads(data(c,'modules/gate.json'));m['rules'][0]['offline']['rule']='neighbor/manual';setdata(c,'modules/gate.json',canonical(m))
    finalize(c);CASES.append(c)

def all_guard(args):
    if len(args)==1:return args[0]
    return dict(op='all',args=args if len(args)<=8 else [all_guard(args[:8]),all_guard(args[8:])])

def extra_guard_facts(c,original_count=0,manual_count=0):
    content=json.loads(data(c,'content.json'));m=json.loads(data(c,'modules/gate.json'));d=deployment(c);b=binding(0);all_names=['done_s0'];original=[dict(op='fact',subject='previous')];manual=[]
    for i,(kind,count) in enumerate([('orig',original_count),('manual',manual_count)]):
        for n in range(count):
            slot=kind+str(n);subject=kind+'_remote'+str(n);sid=identity(0xA100+i*32+n);m['inputs'].append(interface(slot,'solved'));content['stations'][1]['instances'][0]['inputs'][slot]=subject;content['subjects'].append(dict(id=subject,originStation='s0'));d['stations'][0]['subjects'].append(dict(id=subject,subjectId=sid.hex()));b[7].append([1,sid]);all_names.append(subject);c['expectedProgram']['permissions'][subject]=2
            (original if kind=='orig' else manual).append(dict(op='fact',subject=slot))
    m['inputs'].sort(key=lambda x:x['id']);content['subjects'].sort(key=lambda x:x['id']);d['stations'][0]['subjects'].sort(key=lambda x:x['id']);b[7].sort(key=lambda x:(x[0],x[1]));m['rules'][0]['guard']=all_guard(original);m['rules'][0]['trigger']['subjects']=['previous']+['orig'+str(n) for n in range(original_count)]
    if manual:m['rules'][1]['guard']=all_guard(manual)
    d['dependencies']=[dict(station='s1',ruleId=identity(0xC001).hex(),originStation='s0',subject=name) for name in sorted(all_names)];setdata(c,'content.json',canonical(content));setdata(c,'modules/gate.json',canonical(m));setdeployment(c,d);replace_binding(c,0,cbor(b));return finalize(c)
for original,manual in [(0,1),(8,0),(0,8),(0,9),(4,4)]:
    c=extra_guard_facts(v2(),original,manual);c['name']='valid-static-group-union-orig'+str(original+1)+'-manual'+str(manual);c['expectedStaticUnionCount']=1+original+manual;CASES.append(c)
for mode in ['missing-original-side','missing-manual-side','duplicate-by-both-members']:
    c=extra_guard_facts(v2(),0,1);c['name']='union-'+mode;c['expected']='Rejected';d=deployment(c)
    if mode=='missing-original-side':d['dependencies']=[r for r in d['dependencies'] if r['subject']!='done_s0']
    elif mode=='missing-manual-side':d['dependencies']=[r for r in d['dependencies'] if r['subject']!='manual_remote0']
    else:d['dependencies'].insert(1,copy.deepcopy(d['dependencies'][0]))
    setdeployment(c,d);CASES.append(c)

def outputs(c,count,effects=False):
    content=json.loads(data(c,'content.json'));m=json.loads(data(c,'modules/gate.json'));d=deployment(c);b=binding(1);m['outputs']=[interface('done'+str(i),'solved') for i in range(count)]+([interface('light'+str(i),'light') for i in range(count)] if effects else [interface('light','light')]);m['rules'][0]['actions']=[];m['rules'][1]['actions']=[];content['subjects']=[s for s in content['subjects'] if s['id']!='done_s1'];content['stations'][1]['instances'][0]['outputs']={};d['stations'][1]['subjects']=[];b[7]=[[3,identity(0xB001)]];c['expectedProgram']['permissions']={'done_s0':2};d['stations'][1]['ports']=[]
    content['stations'][1]['ports']=[dict(id='light'+str(i),direction='output',type='light',capability='review.light.v1') for i in range(count)] if effects else [dict(id='light',direction='output',type='light',capability='review.light.v1')]
    for i in range(count):
        slot='done'+str(i);subject='released'+str(i);sid=identity(0xA200+i);content['subjects'].append(dict(id=subject,originStation='s1'));content['stations'][1]['instances'][0]['outputs'][slot]=subject;d['stations'][1]['subjects'].append(dict(id=subject,subjectId=sid.hex()));b[7].extend([[1,sid],[2,sid]]);c['expectedProgram']['permissions'][subject]=6;m['rules'][0]['actions'].append(dict(op='emit',subject=slot));m['rules'][1]['actions'].append(dict(op='override',subject=slot))
        if effects:
            port='light'+str(i);content['stations'][1]['instances'][0]['outputs'][port]=port;d['stations'][1]['ports'].append(dict(id=port,componentId=2+i,channel=0,resources=['preset']));m['rules'][0]['actions'].append(dict(op='effect',port=port,resource='preset'));m['rules'][1]['actions'].append(dict(op='effect',port=port,resource='preset'))
    if not effects:content['stations'][1]['instances'][0]['outputs']['light']='light';d['stations'][1]['ports']=[dict(id='light',componentId=2,channel=0,resources=['preset'])]
    content['subjects'].sort(key=lambda x:x['id']);b[7].sort(key=lambda x:(x[0],x[1]));setdata(c,'content.json',canonical(content));setdata(c,'modules/gate.json',canonical(m));setdeployment(c,d);replace_binding(c,1,cbor(b));return finalize(c)
for count,effects in [(4,True),(8,False),(9,False)]:
    c=outputs(v2(),count,effects);c['name']='actions-'+str(count*(2 if effects else 1))+'-with-'+str(count)+'-emissions'
    if count==9:c['expected']='Rejected'
    CASES.append(c)
c=outputs(v2(),4,True);c['name']='group-exception-does-not-hide-intrarule-physical-alias';c['expected']='Rejected';d=deployment(c);d['stations'][1]['ports'][1]['componentId']=d['stations'][1]['ports'][0]['componentId'];setdeployment(c,d);CASES.append(c)
# Maximum generated names count decoded source IDs, never cap generated IDs at48.
c=v2();c['name']='valid-generated-group-names-485-bytes';m=json.loads(data(c,'modules/gate.json'));first='~/'*24;second='~/'*23+'~~';m['rules'][0]['id']=first;m['rules'][0]['offline']['rule']=second;m['rules'][1]['id']=second;setdata(c,'modules/gate.json',canonical(m));bind_group_names(c,qname(['inst'],first),qname(['inst'],second));finalize(c)
for i in range(3):wrap(c,'wide_wrapper'+str(i),'~/'*24)
content=json.loads(data(c,'content.json'));content['stations'][1]['instances'][0]['id']='~/'*24;setdata(c,'content.json',canonical(content));g=c['expectedProgram']['groups'][0];prefix=qname([], '~/'*24);bind_group_names(c,prefix+g['original'][len('/inst'):],prefix+g['replacement'][len('/inst'):]);finalize(c);assert len(c['expectedProgram']['groups'][0]['original'])==485;CASES.append(c)
c=v2();c['name']='valid-full-uint64-binding-generation-distinct-from-rule-generations';b=binding(1,generation=2**64-1);b[7].insert(1,[2,identity(0xA001)]);replace_binding(c,1,cbor(b));d=deployment(c);d['stations'][1]['generation']=2**64-1;setdeployment(c,d);c['expectedProgram']['bindingGeneration']=2**64-1;CASES.append(c)
HERE.joinpath('vectors.json').write_text(json.dumps(CASES,ensure_ascii=False,separators=(',',':')),encoding='utf8')
print(len(CASES),'independent complete v2 artifact vectors;',sum(c['expected']=='IntegrityCheckedUnauthenticated' for c in CASES),'positive integrity cases, no owner admission claim')
