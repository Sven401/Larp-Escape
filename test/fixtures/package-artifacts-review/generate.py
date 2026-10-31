"""Independent fixed artifact-set vectors; no product codec/compiler imports."""
import copy
import hashlib
import json
from pathlib import Path
HERE=Path(__file__).resolve().parent

def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf8')
def sha(data):return hashlib.sha256(data).hexdigest()
def identity(value):return value.to_bytes(16,'big')
def head(major,n):
    if n<24:return bytes([(major<<5)|n])
    for size,tag in [(1,24),(2,25),(4,26),(8,27)]:
        if n<1<<(8*size):return bytes([(major<<5)|tag])+n.to_bytes(size,'big')
    raise ValueError('uint64')
def cbor(value):
    if type(value) is int:return head(0,value)
    if isinstance(value,bytes):return head(2,len(value))+value
    if isinstance(value,list):return head(4,len(value))+b''.join(cbor(x) for x in value)
    if isinstance(value,dict):return head(5,len(value))+b''.join(cbor(k)+cbor(v) for k,v in sorted(value.items()))
    raise TypeError(type(value))
def literal(value):return dict(op='literal',type='bool' if type(value) is bool else 'int32',value=value)
def interface(name,type):return dict(id=name,type=type,label=name,description='independent artifact review')

def module(gate=False):
    name='gate' if gate else 'emitter'
    trigger=dict(kind='fact',subjects=['previous']) if gate else dict(kind='local',ports=['pulse'])
    guard=dict(op='fact',subject='previous') if gate else dict(op='eq',left=dict(op='input',field='value'),right=literal(1))
    rule=dict(id='finish',generation=1,priority=0,mode='once',trigger=trigger,guard=guard,offline=dict(policy='wait' if gate else 'local_continue'),actions=[dict(op='emit',subject='done'),dict(op='effect',port='light',resource='preset')])
    return dict(format='larp.module.v1',moduleId=name,version='1.0.0',label=name,parameters=[],constraints=[],values=[],timers=[],instances=[],inputs=[interface('previous','solved')] if gate else [interface('pulse','int32_sample')],outputs=[interface('done','solved'),interface('light','light')],resources=[dict(id='preset',kind='light_preset')],rules=[rule])
def pin(m):return dict(moduleId=m['moduleId'],version=m['version'],digest=sha(canonical(m)))
def binding(index,box=None,generation=1):
    return {0:1,1:identity(0xE100),2:identity(0x5000+index),3:generation,4:identity(0xB000+index) if box is None else box,5:identity(0xF000+index),6:[identity(0xD000+index*2),identity(0xD001+index*2)],7:[[1,identity(0xA000+index)],[3,identity(0xB000+index) if box is None else box]]}
def source_metadata(content,modules):
    total=len(canonical(content))-sum(len(canonical(s['rules'])) for s in content['stations'])
    seen=set()
    for m in modules:
        d=sha(canonical(m))
        if d in seen:continue
        seen.add(d);total+=len(canonical(m))-len(canonical(m['rules']))-len(canonical(m['constraints']))
    return total

def build(count,metadata_target=None,shared_box=False):
    modules=[module(),module(True)];media=b'{"effect":"sparkle","revision":1}'
    unused=b'independent-unused-preset-bytes'
    def content_for(mods):
        stations=[]
        for i in range(count):
            m=mods[0 if i==0 else 1];ports=[dict(id='light',direction='output',type='light',capability='review.light.v1')]
            if i==0:ports.append(dict(id='pulse',direction='input',type='int32_sample',capability='review.sample.v1'))
            inst=dict(pin(m),id='inst',parameters={},inputs=dict(previous='done_s'+str(i-1)) if i else dict(pulse='pulse'),outputs=dict(done='done_s'+str(i),light='light'),resources=dict(preset='preset'))
            stations.append(dict(id='s'+str(i),label='station '+str(i),values=[],timers=[],rules=[],instances=[inst],ports=ports))
        return dict(format='larp.package.v1',packageId='artifact_review',revision=1,label='artifact review',requires=[dict(name='rules_ir_v1',version=1)],modules=[pin(m) for m in mods],resources=[dict(id='preset',kind='light_preset',digest=sha(media),bytes=len(media)),dict(id='unused',kind='light_preset',digest=sha(unused),bytes=len(unused))],stations=stations,subjects=[dict(id='done_s'+str(i),originStation='s'+str(i)) for i in range(count)])
    content=content_for(modules)
    if metadata_target is not None:
        missing=metadata_target-source_metadata(content,modules);assert missing>=0
        modules[0]['inputs'][0]['description']+='x'*missing
        content=content_for(modules);assert source_metadata(content,modules)==metadata_target
    records=[]
    def file(role,path,data):records.append(dict(role=role,path=path,hex=data.hex()))
    file('content','content.json',canonical(content))
    module_pins=[]
    for m in modules:
        path='modules/'+m['moduleId']+'.json';file('module',path,canonical(m));module_pins.append(dict(pin(m),path=path))
    deploy=dict(format='larp.deployment.v1',event=identity(0xE100).hex(),run=identity(0xE200).hex(),contentDigest=sha(canonical(content)),stations=[],profiles=[],resources=[],rules=[],dependencies=[],requires=copy.deepcopy(content['requires']))
    expected_profiles={};bindings=[]
    for i,s in enumerate(content['stations']):
        sid=s['id'];b=binding(i,identity(0xB000) if shared_box else None)
        if shared_box:b[6]=binding(0)[6]
        raw=cbor(b);bindings.append(b);bpath='bindings/'+sid+'.cbor';file('binding',bpath,raw)
        output_component=2+i*2 if shared_box else 2
        ports=[dict(id='light',componentId=output_component,channel=0,resources=['preset'])]
        if i==0:ports.append(dict(id='pulse',componentId=1,channel=0,resources=[]))
        deploy['stations'].append(dict(id=sid,stationId=b[2].hex(),boxId=b[4].hex(),generation=b[3],bindingDigest=sha(raw),bindingPath=bpath,ports=ports,subjects=[dict(id='done_'+sid,subjectId=identity(0xA000+i).hex())]))
        m=modules[0 if i==0 else 1];rule=copy.deepcopy(m['rules'][0]);rule['id']='/inst/finish';rule['actions'][0]['subject']='done_'+sid
        if i:
            rule['trigger']['subjects']=['done_s'+str(i-1)];rule['guard']['subject']='done_s'+str(i-1)
        profile=canonical(dict(format='larp.rules.v1',station=sid,values=[],timers=[],ports=s['ports'],rules=[rule]));ppath='profiles/'+sid+'.json';file('profile',ppath,profile);expected_profiles[sid]=profile.hex()
        deploy['profiles'].append(dict(station=sid,path=ppath,digest=sha(profile),bytes=len(profile),format='larp.rules.v1'))
        rpath='resources/'+sid+'.preset';file('resource',rpath,media)
        deploy['resources'].append(dict(station=sid,id='preset',path=rpath,digest=sha(media),bytes=len(media),locator=dict(preset=1),verification='operator_inventory'))
        rid=identity(0xC000+i).hex();deploy['rules'].append(dict(station=sid,expandedId='/inst/finish',ruleId=rid,generation=1,offline=rule['offline']))
        if i:deploy['dependencies'].append(dict(station=sid,ruleId=rid,originStation='s'+str(i-1),subject='done_s'+str(i-1)))
    file('unmapped-resource','resources/unmapped.preset',unused)
    file('deployment','deployment.json',canonical(deploy))
    return dict(name='valid-'+str(count)+('-shared-box' if shared_box else ''),selectedStation='s0',owner=dict(event=identity(0xE100).hex(),box=identity(0xB000).hex(),epoch=identity(0xD001).hex()),deploymentPath='deployment.json',contentPath='content.json',modulePins=module_pins,resourcePins=[dict(digest=sha(media),bytes=len(media),path='resources/s0.preset'),dict(digest=sha(unused),bytes=len(unused),path='resources/unmapped.preset')],sources=records,expected='IntegrityCheckedUnauthenticated',sourceMetadataBytes=source_metadata(content,modules),deploymentDigest=sha(canonical(deploy)),expectedProfiles=expected_profiles,expectedSelected=dict(event=identity(0xE100).hex(),run=identity(0xE200).hex(),station=identity(0x5000).hex(),box=identity(0xB000).hex(),epoch=identity(0xD001).hex()))
def record(case,path):return next(r for r in case['sources'] if r['path']==path)
def data(case,path):return bytes.fromhex(record(case,path)['hex'])
def setdata(case,path,value):record(case,path)['hex']=value.hex()
def deployment(case):return json.loads(data(case,'deployment.json'))
def setdeployment(case,value):setdata(case,'deployment.json',canonical(value));case['deploymentDigest']=sha(canonical(value))
def replace_binding(case,index,raw):
    path='bindings/s'+str(index)+'.cbor';setdata(case,path,raw);d=deployment(case);d['stations'][index]['bindingDigest']=sha(raw);setdeployment(case,d)

CASES=[build(2),build(8),build(2,shared_box=True)]
for n in [32768,32769]:
    c=build(2,metadata_target=n);c['name']='source-metadata-'+str(n)
    if n==32769:c['expected']='OperatingProfile'
    CASES.append(c)
# Canonical binding uint64 generation edges, with matching JSON duplicate field.
for generation in [1,23,24,255,256,65535,65536,4294967295,4294967296,18446744073709551615]:
    c=build(2);c['name']='binding-generation-'+str(generation);b=binding(0,generation=generation);replace_binding(c,0,cbor(b));d=deployment(c);d['stations'][0]['generation']=generation;setdeployment(c,d);CASES.append(c)
# Every malformed-CBOR case is repinned to its actual bytes to isolate decoding.
def badbinding(name,raw):
    c=build(2);c['name']=name;c['expected']='Rejected';replace_binding(c,0,raw);CASES.append(c)
b=binding(0);raw=cbor(b)
badbinding('cbor-map-nonminimal-length',b'\xb8\x08'+raw[1:])
badbinding('cbor-key-nonminimal',raw[:1]+b'\x18\x00'+raw[2:])
badbinding('cbor-map-indefinite',b'\xbf'+raw[1:]+b'\xff')
badbinding('cbor-trailing-byte',raw+b'\x00')
badbinding('cbor-tag',b'\xc0'+raw)
badbinding('cbor-reordered-keys',b'\xa8'+cbor(1)+cbor(b[1])+cbor(0)+cbor(1)+b''.join(cbor(k)+cbor(b[k]) for k in range(2,8)))
badbinding('cbor-duplicate-key',b'\xa8'+cbor(0)+cbor(1)+cbor(0)+cbor(b[1])+b''.join(cbor(k)+cbor(b[k]) for k in range(2,8)))
for key in [1,2,4,5]:
    q=copy.deepcopy(b);q[key]=b'\x01'*15;badbinding('cbor-id-width-'+str(key),cbor(q))
q=copy.deepcopy(b);q[6]=list(reversed(q[6]));badbinding('cbor-epoch-order',cbor(q))
q=copy.deepcopy(b);q[6]=[q[6][0],q[6][0]];badbinding('cbor-epoch-duplicate',cbor(q))
q=copy.deepcopy(b);q[7]=list(reversed(q[7]));badbinding('cbor-permission-order',cbor(q))
q=copy.deepcopy(b);q[7]=[q[7][0],q[7][0]];badbinding('cbor-permission-duplicate',cbor(q))
q=copy.deepcopy(b);q[8]=0;badbinding('cbor-unknown-key',cbor(q))
q=copy.deepcopy(b);q[3]=0;badbinding('cbor-generation-zero',cbor(q))
# Nonminimal integer generation (1 encoded with an additional byte).
needle=cbor(3)+cbor(1)+cbor(4);assert raw.count(needle)==1
badbinding('cbor-generation-nonminimal',raw.replace(needle,cbor(3)+b'\x18\x01'+cbor(4),1))
# One-file corruption and closure/identity counterexamples.
for role,path in [('binding','bindings/s0.cbor'),('profile','profiles/s1.json'),('resource','resources/s1.preset'),('unmapped','resources/unmapped.preset')]:
    c=build(2);c['name']='changed-'+role+'-actual-bytes';c['expected']='Rejected';setdata(c,path,data(c,path)+b'x');CASES.append(c)
c=build(2);c['name']='regenerated-ir-tamper-rehashed';c['expected']='Rejected';path='profiles/s1.json';p=json.loads(data(c,path));p['rules'][0]['actions'].reverse();changed=canonical(p);setdata(c,path,changed);d=deployment(c);d['profiles'][1].update(digest=sha(changed),bytes=len(changed));setdeployment(c,d);CASES.append(c)
for path in ['../escape','a/../b','a//b','/absolute','a\\b','C:relative','a:b','a/./b']:
    c=build(2);c['name']='unsafe-profile-path-'+path;c['expected']='Rejected';d=deployment(c);d['profiles'][0]['path']=path;setdeployment(c,d);CASES.append(c)
c=build(2);c['name']='selected-owner-epoch-is-not-first';assert c['owner']['epoch']==binding(0)[6][1].hex();CASES.append(c)
c=build(2);c['name']='selected-owner-epoch-not-allowed';c['owner']['epoch']=identity(0xFFFF).hex();c['expected']='Rejected';CASES.append(c)
c=build(2);c['name']='selected-owner-box-mismatch';c['owner']['box']=identity(0xFFFF).hex();c['expected']='Rejected';CASES.append(c)
c=build(2);c['name']='selected-owner-event-mismatch';c['owner']['event']=identity(0xFFFF).hex();c['expected']='Rejected';CASES.append(c)
# Counter-only sources: separate from full deployment correctness vectors.
metadata_cases=[]
def metadata_case(name,content,mods):
    metadata_cases.append(dict(name=name,content=canonical(content).decode(),modules=[dict(pin(m),bytes=canonical(m).decode()) for m in mods],expectedMetadata=source_metadata(content,mods),expectedRaw=len(canonical(content))+sum(len(canonical(m)) for m in {sha(canonical(m)):m for m in mods}.values())))
def source_parts(case):
    content=json.loads(data(case,'content.json'));mods=[json.loads(data(case,p['path'])) for p in case['modulePins']];return content,mods
for n in [32768,32769]:
    c,mods=source_parts(build(2,metadata_target=n));metadata_case('aggregate-'+str(n),c,mods)
c,mods=source_parts(build(2));unused=module();unused.update(moduleId='unused',label='unused',inputs=[],outputs=[],resources=[],values=[],timers=[],rules=[],parameters=[],instances=[],constraints=[])
mods.append(unused);c['modules'].append(pin(unused));c['modules'].append(pin(unused));metadata_case('unused-distinct-source-once-duplicate-pin-text-counts',c,mods)
# Large rule/constraint arrays in an unused definition are excluded, not truncated.
large=copy.deepcopy(unused);large['inputs']=[interface('pulse','int32_sample')]
large['rules']=[dict(id='r%03d'%i,generation=1,priority=0,mode='once',trigger=dict(kind='local',ports=['pulse']),guard=literal(True),offline=dict(policy='local_continue'),actions=[]) for i in range(128)]
large['constraints']=[literal(True) for _ in range(256)]
base=copy.deepcopy(c);base['modules']=[pin(m) for m in mods[:2]]+[pin(large)];metadata_case('schema-rule-constraint-arrays-excluded',base,mods[:2]+[large])
small=copy.deepcopy(large);small['rules']=[];small['constraints']=[];short=copy.deepcopy(base);short['modules']=[pin(m) for m in mods[:2]]+[pin(small)];metadata_case('same-excluded-arrays-empty',short,mods[:2]+[small]);assert metadata_cases[-1]['expectedMetadata']==metadata_cases[-2]['expectedMetadata']
# UTF-8 and JSON quote/backslash escape bytes are counted RAW.
c,mods=source_parts(build(2));mods[0]['inputs'][0]['description']='é "rules" \\ constraints';c['modules']=[pin(m) for m in mods];c['stations'][0]['instances'][0].update(pin(mods[0]));metadata_case('raw-utf8-and-escaped-metadata',c,mods)
# The full instance map is included, even if a parameter is literally named rules.
m=copy.deepcopy(unused);m['parameters']=[dict(id='rules',type='bool',label='rules',description='constraints',default=False)];mods=[m];c,ignore=source_parts(build(2));c['modules']=[pin(m)];c['stations']=c['stations'][:1];c['subjects']=c['subjects'][:1];c['stations'][0]['instances']=[dict(pin(m),id='i',parameters=dict(rules=literal(True)),inputs={},outputs={},resources={})];metadata_case('instance-map-key-rules-is-included',c,mods)
# Only direct station rule arrays are excluded, not arbitrary text or instances.
c,mods=source_parts(build(2));direct=dict(id='direct',generation=1,priority=0,mode='once',trigger=dict(kind='local',ports=['pulse']),guard=literal(False),offline=dict(policy='local_continue'),actions=[]);c['stations'][0]['rules']=[direct];metadata_case('direct-station-rules-excluded',c,mods)
empty=copy.deepcopy(c);empty['stations'][0]['rules']=[];metadata_case('direct-station-rules-empty',empty,mods);assert metadata_cases[-1]['expectedMetadata']==metadata_cases[-2]['expectedMetadata']
(HERE/'metadata-vectors.json').write_text(json.dumps(metadata_cases,ensure_ascii=False),encoding='utf8')
print(len(metadata_cases),'independent shared metadata counter cases')
# Selected-program identity and complete epoch membership, not first-entry choice.
for count,index in [(2,1),(8,7)]:
    c=build(count);c['name']='selected-nonfirst-station-'+str(index);c['selectedStation']='s'+str(index);c['owner'].update(box=identity(0xB000+index).hex(),epoch=identity(0xD001+index*2).hex());c['expectedSelected'].update(station=identity(0x5000+index).hex(),box=c['owner']['box'],epoch=c['owner']['epoch']);CASES.append(c)
# Requirements are exact lexical positive majors, not uint32/uint64 numbers.
for kind in ['extra','missing','conflict','huge-major']:
    c=build(2);c['name']='manifest-requires-'+kind;d=deployment(c)
    if kind=='extra':d['requires'].insert(0,dict(name='audio_preview_v1',version=3))
    elif kind=='missing':d['requires']=[];c['expected']='Rejected'
    elif kind=='conflict':d['requires'][0]['version']=2;c['expected']='Rejected'
    else:
        large=10**80+17;content=json.loads(data(c,'content.json'));content['requires'][0]['version']=large;setdata(c,'content.json',canonical(content));d['contentDigest']=sha(canonical(content));d['requires'][0]['version']=large
        mods=[json.loads(data(c,p['path'])) for p in c['modulePins']];c['sourceMetadataBytes']=source_metadata(content,mods)
    setdeployment(c,d);CASES.append(c)
# Type-2 metadata permission does not itself authorize fallback execution.
c=build(2);c['name']='own-type2-metadata-not-fallback-authority';b=binding(0);b[7].insert(1,[2,identity(0xA000)]);replace_binding(c,0,cbor(b));CASES.append(c)
c=build(2);c['name']='missing-required-type1-permission';c['expected']='Rejected';b=binding(0);b[7]=b[7][1:];replace_binding(c,0,cbor(b));CASES.append(c)
for field in ['event','stationId','boxId','generation']:
    c=build(2);c['name']='json-cbor-disagreement-'+field;c['expected']='Rejected';d=deployment(c)
    if field=='event':d['event']=identity(0xFFFF).hex()
    else:d['stations'][0][field]=2 if field=='generation' else identity(0xFFFF).hex()
    setdeployment(c,d);CASES.append(c)
for field in ['bytes','digest']:
    c=build(2);c['name']='actual-profile-'+field+'-mismatch';c['expected']='Rejected';d=deployment(c);d['profiles'][1][field]=d['profiles'][1]['bytes']+1 if field=='bytes' else '0'*64;setdeployment(c,d);CASES.append(c)
for field in ['ruleId','offline','generation']:
    c=build(2);c['name']='rule-binding-'+field+'-mismatch';c['expected']='Rejected';d=deployment(c)
    if field=='ruleId':d['rules'][0][field]='0'*32
    elif field=='offline':d['rules'][0][field]='local_continue'
    else:d['rules'][0][field]=2
    setdeployment(c,d);CASES.append(c)
c=build(2);c['name']='light-resource-with-audio-locator';c['expected']='Rejected';d=deployment(c);d['resources'][0]['locator']=dict(bank=1,track=1);setdeployment(c,d);CASES.append(c)
c=build(2,shared_box=True);c['name']='cross-station-physical-component-alias';c['expected']='OperatingProfile';d=deployment(c);d['stations'][1]['ports'][0]['componentId']=2;setdeployment(c,d);CASES.append(c)
# Full static transitive provenance: set writer -> add old value -> arm -> timer.
c=build(2);c['name']='static-value-add-timer-fixedpoint';m=json.loads(data(c,'modules/gate.json'));m['inputs']=[interface('arm','int32_sample'),interface('bump','int32_sample'),interface('previous','solved'),interface('remember','int32_sample')];m['values']=[dict(id='count',type='int32',initial=0)];m['timers']=[dict(id='window',durationMs=1)]
def local_rule(id,port,guard,actions,mode='each'):
    return dict(id=id,generation=1,priority=0,mode=mode,trigger=dict(kind='local',ports=[port]),guard=guard,offline=dict(policy='local_continue'),actions=actions)
m['rules']=[local_rule('arm','arm',dict(op='eq',left=dict(op='value',id='count'),right=literal(2)),[dict(op='arm',timer='window')]),local_rule('bump','bump',literal(True),[dict(op='add',id='count',expr=literal(1))]),dict(id='finish',generation=1,priority=0,mode='once',trigger=dict(kind='timer',timer='window'),guard=literal(True),offline=dict(policy='local_continue'),actions=[dict(op='emit',subject='done'),dict(op='effect',port='light',resource='preset')]),local_rule('seed','remember',dict(op='fact',subject='previous'),[dict(op='set',id='count',expr=literal(1))])]
content=json.loads(data(c,'content.json'));content['modules'][1]=pin(m);s=content['stations'][1];s['ports']=[dict(id=p,direction='output' if p=='light' else 'input',type='light' if p=='light' else 'int32_sample',capability='review.light.v1' if p=='light' else 'review.sample.v1') for p in ['arm','bump','light','remember']];s['instances'][0].update(pin(m),inputs=dict(arm='arm',bump='bump',previous='done_s0',remember='remember'))
setdata(c,'content.json',canonical(content));setdata(c,'modules/gate.json',canonical(m));c['modulePins'][1].update(pin(m))
def expanded_expr(expr):
    e=copy.deepcopy(expr)
    if e['op']=='value':e['id']='/inst/'+e['id']
    if e['op']=='fact':e['subject']='done_s0'
    for k in ['arg','left','right']:
        if k in e:e[k]=expanded_expr(e[k])
    if 'args' in e:e['args']=[expanded_expr(x) for x in e['args']]
    return e
rules=[]
for r in m['rules']:
    r=copy.deepcopy(r);r['id']='/inst/'+r['id'];r['guard']=expanded_expr(r['guard'])
    if r['trigger']['kind']=='timer':r['trigger']['timer']='/inst/window'
    for a in r['actions']:
        if a['op'] in ['set','add']:a['id']='/inst/count';a['expr']=expanded_expr(a['expr'])
        if a['op'] in ['arm','cancel']:a['timer']='/inst/window'
        if a['op']=='emit':a['subject']='done_s1'
    rules.append(r)
ir=canonical(dict(format='larp.rules.v1',station='s1',ports=s['ports'],values=[dict(id='/inst/count',type='int32',initial=0)],timers=[dict(id='/inst/window',durationMs=1)],rules=rules));setdata(c,'profiles/s1.json',ir);c['expectedProfiles']['s1']=ir.hex();d=deployment(c);d['contentDigest']=sha(canonical(content));d['profiles'][1].update(digest=sha(ir),bytes=len(ir));d['stations'][1]['ports']=[dict(id=p['id'],componentId={'arm':3,'bump':4,'light':2,'remember':1}[p['id']],channel=0,resources=['preset'] if p['id']=='light' else []) for p in s['ports']]
d['rules']=d['rules'][:1]+[dict(station='s1',expandedId=r['id'],ruleId=identity(0xC010+i).hex(),generation=1,offline=r['offline']) for i,r in enumerate(rules)];d['dependencies']=[dict(station='s1',ruleId=r['ruleId'],originStation='s0',subject='done_s0') for r in d['rules'][1:]];setdeployment(c,d);c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']]);c['selectedStation']='s1';c['owner'].update(box=identity(0xB001).hex(),epoch=identity(0xD003).hex());c['expectedSelected'].update(station=identity(0x5001).hex(),box=c['owner']['box'],epoch=c['owner']['epoch']);CASES.append(c)
for mode in ['missing','extra']:
    q=copy.deepcopy(c);q['name']='static-fixedpoint-dependency-'+mode;q['expected']='Rejected';d=deployment(q)
    if mode=='missing':d['dependencies'].pop(2)
    else:d['dependencies'].append(dict(station='s1',ruleId=d['rules'][-1]['ruleId'],originStation='s1',subject='done_s1'))
    setdeployment(q,d);CASES.append(q)
# One path may be both exact IR and compatible raw resource bytes.
c=build(2);c['name']='compatible-profile-resource-path-alias';shared=data(c,'profiles/s0.json');content=json.loads(data(c,'content.json'));content['resources'][0].update(digest=sha(shared),bytes=len(shared));setdata(c,'content.json',canonical(content));setdata(c,'resources/s1.preset',shared);d=deployment(c);d['contentDigest']=sha(canonical(content))
for r in d['resources']:r.update(digest=sha(shared),bytes=len(shared))
d['resources'][0]['path']='profiles/s0.json';setdeployment(c,d);c['resourcePins'][0].update(digest=sha(shared),bytes=len(shared),path='profiles/s0.json');c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']]);CASES.append(c)

# Actual audio-kind bytes and bank/track locations have their own positive path.
c=build(2);c['name']='valid-actual-audio-two-stations';content=json.loads(data(c,'content.json'))
for r in content['resources']:r['kind']='audio'
for station in content['stations']:
    next(p for p in station['ports'] if p['id']=='light')['type']='audio'
    next(p for p in station['ports'] if p['id']=='light')['capability']='review.audio.v1'
for pinentry in c['modulePins']:
    m=json.loads(data(c,pinentry['path']));next(p for p in m['outputs'] if p['id']=='light')['type']='audio';m['resources'][0]['kind']='audio';raw=canonical(m);setdata(c,pinentry['path'],raw);pinentry['digest']=sha(raw)
    for sourcepin in content['modules']:
        if sourcepin['moduleId']==m['moduleId']:sourcepin['digest']=sha(raw)
    for station in content['stations']:
        for inst in station['instances']:
            if inst['moduleId']==m['moduleId']:inst['digest']=sha(raw)
setdata(c,'content.json',canonical(content));d=deployment(c);d['contentDigest']=sha(canonical(content))
for resource in d['resources']:resource['locator']=dict(bank=1,track=1)
for profile in d['profiles']:
    raw=json.loads(data(c,profile['path']));next(p for p in raw['ports'] if p['id']=='light')['type']='audio';next(p for p in raw['ports'] if p['id']=='light')['capability']='review.audio.v1';value=canonical(raw);setdata(c,profile['path'],value);profile.update(bytes=len(value),digest=sha(value));c['expectedProfiles'][profile['station']]=value.hex()
setdeployment(c,d);c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']]);CASES.append(c)
q=copy.deepcopy(c);q['name']='audio-resource-with-light-locator';q['expected']='Rejected';d=deployment(q);d['resources'][0]['locator']=dict(preset=1);setdeployment(q,d);CASES.append(q)

# Compatible physical-address aliases are allowed; incompatible bytes are not.
c=next(c for c in CASES if c['name']=='cross-station-physical-component-alias');c['name']='compatible-cross-station-physical-locator-alias';c['expected']='IntegrityCheckedUnauthenticated'
q=copy.deepcopy(c);q['name']='incompatible-cross-station-physical-locator-bytes';q['expected']='Rejected';content=json.loads(data(q,'content.json'));content['stations'][1]['instances'][0]['resources']['preset']='unused';setdata(q,'content.json',canonical(content));raw=json.loads(data(q,'profiles/s1.json'));raw['rules'][0]['actions'][1]['resource']='unused';profile=canonical(raw);setdata(q,'profiles/s1.json',profile);q['expectedProfiles']['s1']=profile.hex();media=data(q,'resources/unmapped.preset');setdata(q,'resources/s1.preset',media);d=deployment(q);d['contentDigest']=sha(canonical(content));d['stations'][1]['ports'][0]['resources']=['unused'];d['resources'][1].update(id='unused',digest=sha(media),bytes=len(media));d['profiles'][1].update(digest=sha(profile),bytes=len(profile));setdeployment(q,d);q['sourceMetadataBytes']=source_metadata(content,[json.loads(data(q,p['path'])) for p in q['modulePins']]);CASES.append(q)
r=copy.deepcopy(q);r['name']='different-physical-locator-different-bytes';r['expected']='IntegrityCheckedUnauthenticated';d=deployment(r);d['resources'][1]['locator']['preset']=2;setdeployment(r,d);CASES.append(r)
# A declared source subject may be read under T2 metadata without any emit.
c=build(2);c['name']='type2-only-source-read-no-source-emit';content=json.loads(data(c,'content.json'));m=json.loads(data(c,'modules/emitter.json'));m['rules'][0]['actions']=m['rules'][0]['actions'][1:];modulebytes=canonical(m);setdata(c,'modules/emitter.json',modulebytes);c['modulePins'][0]['digest']=sha(modulebytes);content['modules'][0]['digest']=sha(modulebytes);content['stations'][0]['instances'][0]['digest']=sha(modulebytes);setdata(c,'content.json',canonical(content));raw=json.loads(data(c,'profiles/s0.json'));raw['rules'][0]['actions']=raw['rules'][0]['actions'][1:];profile=canonical(raw);setdata(c,'profiles/s0.json',profile);c['expectedProfiles']['s0']=profile.hex();d=deployment(c);d['contentDigest']=sha(canonical(content));d['profiles'][0].update(digest=sha(profile),bytes=len(profile));setdeployment(c,d);b=binding(0);b[7][0][0]=2;replace_binding(c,0,cbor(b));c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']]);CASES.append(c)

c=build(2);c['name']='same-stable-rule-id-different-stations';d=deployment(c);d['rules'][1]['ruleId']=d['rules'][0]['ruleId'];d['dependencies'][0]['ruleId']=d['rules'][0]['ruleId'];setdeployment(c,d);CASES.append(c)

# Legacy extras are parsed with the actual old reader, and all aliased roles run.
legacy=canonical(json.loads((HERE.parents[2]/'docs/configuration/hint-profile.json').read_text(encoding='utf8')))
c=build(2);c['name']='valid-actual-legacy-extra';c['sources'].append(dict(role='legacy',path='legacy/s0.json',hex=legacy.hex()));d=deployment(c);d['profiles'].append(dict(station='s0',path='legacy/s0.json',digest=sha(legacy),bytes=len(legacy),format='profile.v1'));d['profiles'].sort(key=lambda p:(p['station'],p['path']));setdeployment(c,d);CASES.append(c)
for invalid in [False,True]:
    c=copy.deepcopy(c if not invalid else CASES[-1]);c['name']='invalid-legacy-resource-role-alias' if invalid else 'compatible-legacy-resource-role-alias';c['expected']='Rejected' if invalid else 'IntegrityCheckedUnauthenticated';raw=b'{}' if invalid else legacy;setdata(c,'legacy/s0.json',raw);setdata(c,'resources/s1.preset',raw);content=json.loads(data(c,'content.json'));content['resources'][0].update(digest=sha(raw),bytes=len(raw));setdata(c,'content.json',canonical(content));d=deployment(c);d['contentDigest']=sha(canonical(content))
    for r in d['resources']:r.update(digest=sha(raw),bytes=len(raw))
    d['resources'][0]['path']='legacy/s0.json'
    next(p for p in d['profiles'] if p['format']=='profile.v1').update(digest=sha(raw),bytes=len(raw));setdeployment(c,d);c['resourcePins'][0].update(digest=sha(raw),bytes=len(raw),path='legacy/s0.json');c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']]);CASES.append(c)

c=copy.deepcopy(next(c for c in CASES if c['name']=='valid-actual-legacy-extra'));c['name']='valid-legacy-whitespace-unicode-escape';raw=(HERE.parents[2]/'docs/configuration/hint-profile.json').read_bytes().replace(b'\r\n',b'\n').replace(b'Larp-Escape with hint',b'\\u004carp-Escape with hint');setdata(c,'legacy/s0.json',raw);d=deployment(c);next(p for p in d['profiles'] if p['format']=='profile.v1').update(digest=sha(raw),bytes=len(raw));setdeployment(c,d);CASES.append(c)
for mode in ['streamed-262145','uint32-max-early-source-failure','uint32-max-short-source','uint32-plus-one']:
    c=copy.deepcopy(next(c for c in CASES if c['name']=='valid-actual-audio-two-stations'));c['name']='audio-'+mode;c['expected']='IntegrityCheckedUnauthenticated' if mode=='streamed-262145' else 'Rejected';raw=(b'0123456789abcdef'*16385)[:262145] if mode=='streamed-262145' else b'audio bytes';declared=len(raw) if mode=='streamed-262145' else 4294967296 if mode=='uint32-plus-one' else 4294967295
    for path in ['resources/s0.preset','resources/s1.preset']:setdata(c,path,raw)
    content=json.loads(data(c,'content.json'));content['resources'][0].update(digest=sha(raw),bytes=declared);setdata(c,'content.json',canonical(content));d=deployment(c);d['contentDigest']=sha(canonical(content))
    for resource in d['resources']:resource.update(digest=sha(raw),bytes=declared)
    setdeployment(c,d);c['resourcePins'][0].update(digest=sha(raw),bytes=min(declared,4294967295));c['sourceMetadataBytes']=source_metadata(content,[json.loads(data(c,p['path'])) for p in c['modulePins']])
    if mode=='uint32-max-early-source-failure':c['sourceFailure']=dict(path='resources/s0.preset',offset=0);c['expectedError']='SourceFailure'
    elif mode=='uint32-max-short-source':c['expectedError']='LengthMismatch'
    elif mode=='uint32-plus-one':c['expectedError']='Range'
    CASES.append(c)
(HERE/'vectors.json').write_text(json.dumps(CASES,ensure_ascii=False),encoding='utf8')
print(len(CASES),'complete artifact-set vectors including legacy grammar and media length boundaries')
