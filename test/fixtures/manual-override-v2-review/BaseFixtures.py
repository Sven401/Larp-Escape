"""Pure independent byte helpers preserved from the prior artifact audit; no product imports."""
import copy,hashlib,json
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

def badbinding(name,raw):
    c=build(2);c['name']=name;c['expected']='Rejected';replace_binding(c,0,raw);CASES.append(c)

def metadata_case(name,content,mods):
    metadata_cases.append(dict(name=name,content=canonical(content).decode(),modules=[dict(pin(m),bytes=canonical(m).decode()) for m in mods],expectedMetadata=source_metadata(content,mods),expectedRaw=len(canonical(content))+sum(len(canonical(m)) for m in {sha(canonical(m)):m for m in mods}.values())))

def source_parts(case):
    content=json.loads(data(case,'content.json'));mods=[json.loads(data(case,p['path'])) for p in case['modulePins']];return content,mods

def local_rule(id,port,guard,actions,mode='each'):
    return dict(id=id,generation=1,priority=0,mode=mode,trigger=dict(kind='local',ports=[port]),guard=guard,offline=dict(policy='local_continue'),actions=actions)

def expanded_expr(expr):
    e=copy.deepcopy(expr)
    if e['op']=='value':e['id']='/inst/'+e['id']
    if e['op']=='fact':e['subject']='done_s0'
    for k in ['arg','left','right']:
        if k in e:e[k]=expanded_expr(e[k])
    if 'args' in e:e['args']=[expanded_expr(x) for x in e['args']]
    return e
