#!/usr/bin/env python3
"""Independent correlation of the native C++ security observation and public records.
Only SDK/task models; never promotes metadata to measured SMP/RF evidence.
"""
from pathlib import Path
import copy
import json
import sys
sys.dont_write_bytecode=True
from check_peer_registry import decode
ROOT=Path(__file__).resolve().parents[1]
EXPECTED=dict(scope='host_actual_crypto_original_smp_sdk_metadata_model',smpRegistrations=1,
    originalSmpCallbacks=3,gattReads=1,flagReads=1,stateReads=1,procedureReads=2,
    resolvedPairingMode=13,securityLevel=4,flags=22,linkState=5,keyBytes=16,
    proofFrames=6,commandFrames=13,sourceActor=1,cryptoWorker=3,pairMilliseconds=30000,
    storedExplicitRole=True,oldProofRejected=True,runningAfterRevocation=True,
    bondRestoreImplemented=False,smpMeasured=False,rfInitialized=False)

def accept(value,records,proof):
    if not isinstance(value,dict) or value!=EXPECTED: raise ValueError('security observation')
    if any(type(value[k]) is not type(v) for k,v in EXPECTED.items()): raise ValueError('security types')
    if value['proofFrames']*12!=proof['proofBytes'] or proof['proofKind']!=10 or proof['cryptoVersion']!='45.0.7': raise ValueError('proof correlation')
    if proof['deviceMeasured'] is not False or proof['transportSecurityMeasured'] is not False: raise ValueError('unmeasured proof')
    if value['resolvedPairingMode']&13!=13 or value['securityLevel']!=4 or value['flags']&22!=22 or value['linkState']&5!=5 or value['keyBytes']!=16: raise ValueError('SDK metadata')
    if len(records)!=3 or [r['generation'] for r in records]!=[1,2,3]: raise ValueError('records')
    if any(len(r['entries'])!=1 for r in records) or [r['entries'][0]['principal'] for r in records]!=[0,7,0]: raise ValueError('explicit stored roles')
    if any(r['device']!=records[0]['device'] or r['entries'][0]['key']!=records[0]['entries'][0]['key'] or r['entries'][0]['peer']!=records[0]['entries'][0]['peer'] or r['entries'][0]['credential']!=1 for r in records): raise ValueError('original credential')

def main():
    folder=ROOT/'docs/companion/fixtures'
    value=json.loads((folder/'peer-security/observation.json').read_text(encoding='utf-8'))
    records=[decode(bytes.fromhex((folder/('peer-registry/'+n+'.hex')).read_text(encoding='utf-8').strip())) for n in ('key-only','game-master','revoked')]
    proof=json.loads((folder/'peer-proof/test-vector-v1.json').read_text(encoding='utf-8'))
    accept(value,records,proof)
    wrong=[]
    for key,wanted in value.items():
        entry=dict(value); entry[key]=not wanted if type(wanted) is bool else wanted+1 if type(wanted) is int else 'device_measured'; wrong.append((entry,records,proof))
        entry=dict(value); entry[key]=int(wanted) if type(wanted) is bool else str(wanted) if type(wanted) is int else None; wrong.append((entry,records,proof))
    wrong += [(dict(value,peerDelivered=True),records,proof),({k:v for k,v in value.items() if k!='scope'},records,proof)]
    for variant in range(5):
        changed=copy.deepcopy(records); changedProof=dict(proof)
        if variant==0: changed[1]['entries'][0]['principal']=0
        if variant==1: changed[2]['entries'][0]['principal']=7
        if variant==2: changed[2]['entries'][0]['key']=bytes(65)
        if variant==3: changedProof['proofBytes']=71
        if variant==4: changedProof['deviceMeasured']=True
        wrong.append((value,changed,changedProof))
    for entry,r,p in wrong:
        try: accept(entry,r,p)
        except ValueError: continue
        raise ValueError('wrong security correlation accepted')
    print(f'Security counterpart: original SMP3/SDK reads5, real crypto6 proof frames/13 commands and stored GM/revoke; {len(wrong)} wrong observations/correlations rejected. SDK model only; RF/SMP measurement open.')
if __name__=='__main__': main()
