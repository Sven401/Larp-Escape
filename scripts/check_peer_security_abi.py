#!/usr/bin/env python3
"""Read pinned official ESP-IDF4.4.6 definitions; independently compile narrow ABI.
No installed SDK edits, SDK start, radio, source download or mutable private cast.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_gatt_source_abi import PINS as GATT_PINS, block, typedef
PINS = {'host/bluedroid/stack/include/stack/smp_api.h': '12a5538113c57267481afd16d79aabeb08913f3e6bb2aca945f220f17d17f96e', 'host/bluedroid/stack/smp/include/smp_int.h': '1dcf4752977e3affcd81d593bd08d688eb6a2cc0ea3bc341adb45ac30b2d1ff3', 'host/bluedroid/stack/smp/smp_api.c': '8a90b1e6813cb825a2aa8d23819a82b691d14036c0d9a20d0f00436d29b42875', 'host/bluedroid/stack/smp/smp_act.c': 'f77085870a25a0928fcd9603a9093e3d65b0da0b358a9ba55c4ed06b424dec12', 'host/bluedroid/stack/smp/smp_utils.c': '1398344ad9afa3c958267a7d31d3b9df5e19f4c4cc6fb92c996ae59aa3c08c2a', 'host/bluedroid/stack/btm/btm_sec.c': 'd3207f4b4a369cf534f128c124fca8192979769a01986ed46bcb15ace4c8b394', 'host/bluedroid/stack/btm/btm_ble.c': '2a1805a0643666d671d24ca5a1e5c5072a509aa88fb53140b513c3161d5a7f65', 'host/bluedroid/stack/include/stack/btm_ble_api.h': '539f0c805cd5ee6cdcfa44676d7e6ada9d86dc57ab89bd9d5ce50a15b2820402'}

def function(source, name):
    match = re.search(r"(?:BOOLEAN|UINT8|uint16_t)\s+"+re.escape(name)+r"\s*\([^)]*\)\s*\{", source)
    if not match: raise ValueError("function absent: "+name)
    start=match.start(); end=match.end(); depth=1
    while depth and end<len(source):
        if source[end]=='{': depth+=1
        if source[end]=='}': depth-=1
        end+=1
    if depth: raise ValueError("function unbalanced: "+name)
    return source[start:end]

def prototype(source,name):
    definition=function(source,name)
    return definition[:definition.index('{')].strip()+';'

def checked(path, pin):
    data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=pin: raise ValueError("source changed: "+str(path))
    return data.decode('utf-8')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir',type=Path,required=True)
    parser.add_argument('--gatt-source-dir',type=Path,required=True)
    parser.add_argument('--compiler',type=Path,required=True)
    args=parser.parse_args()
    sources={name:checked(args.source_dir/name,pin) for name,pin in PINS.items()}
    old={name:checked(args.gatt_source_dir/name,GATT_PINS[name]) for name in ('bt_common.h','bt_types.h','gatt_api.c')}
    api=sources['host/bluedroid/stack/include/stack/smp_api.h']
    internal=sources['host/bluedroid/stack/smp/include/smp_int.h']
    act=sources['host/bluedroid/stack/smp/smp_act.c']
    util=sources['host/bluedroid/stack/smp/smp_utils.c']
    ble=sources['host/bluedroid/stack/btm/btm_ble.c']
    sec=sources['host/bluedroid/stack/btm/btm_sec.c']
    callback=re.search(r'typedef\s+UINT8\s*\(tSMP_CALLBACK\)\s*\([^;]*;',api)
    if not callback: raise ValueError('original SMP callback absent')
    union=block(api,'tSMP_EVT_DATA','union')
    if not re.search(r'tSMP_CMPL\s+cmplt\s*;',union): raise ValueError('original completion union member absent')
    declarations=['#include <cstddef>\n#include <cstdint>\n#include <type_traits>\n', '#define LARP_GATT_SOURCE_HOST_MODEL 1',
        '#include "'+(Path(__file__).resolve().parents[1]/'lib/GattsWriteAdmission/SdkPeerSecurityAbi.h').as_posix()+'"',
        'namespace reference {', typedef(old['bt_common.h'],'UINT8'),typedef(old['bt_common.h'],'UINT16'),
        typedef(old['bt_common.h'],'BOOLEAN'), 'typedef UINT8 BD_ADDR[6];',
        typedef(old['bt_types.h'],'tBT_TRANSPORT'),
        typedef(sources['host/bluedroid/stack/include/stack/btm_ble_api.h'],'tGATT_IF')]
    for name in ('tSMP_EVT','tSMP_AUTH_REQ','tSMP_STATUS','tSMP_SEC_LEVEL'):
        declarations.append(typedef(api,name))
    declarations += [block(api,'tSMP_CMPL','struct'),
        '// Opaque event pointer, no event union allocation or whole-layout claim.',
        'using tSMP_EVT_DATA=larp::companion::sdk446::SmpData;',callback[0]]
    funcs=(('SMP_Register','__real_SMP_Register',sources['host/bluedroid/stack/smp/smp_api.c']),
        ('GATT_GetConnectionInfor','GATT_GetConnectionInfor',old['gatt_api.c']),
        ('BTM_GetSecurityFlagsByTransport','BTM_GetSecurityFlagsByTransport',sec),
        ('BTM_GetLeSecurityState','BTM_GetLeSecurityState',ble),
        ('BTM_BleSecurityProcedureIsRunning','BTM_BleSecurityProcedureIsRunning',ble))
    for name,ours,source in funcs: declarations.append(prototype(source,name))
    declarations.append('}')
    declarations.append('static_assert(sizeof(reference::tSMP_CMPL)==5 && alignof(reference::tSMP_CMPL)==1,"five byte SDK completion");')
    for index,name in enumerate(('reason','sec_level','is_pair_cancel','smp_over_br','auth_mode')):
        declarations.append(f'static_assert(offsetof(reference::tSMP_CMPL,{name})=={index},"original offset {name}");')
    declarations.append('static_assert(std::is_same<reference::BOOLEAN,bool>::value,"SDK BOOLEAN is bool");')
    declarations.append('static_assert(std::is_same<reference::tSMP_CALLBACK *,larp::companion::sdk446::SmpCallback>::value,"exact scalar callback ABI, opaque data pointer");')
    for name,ours,source in funcs:
        declarations.append(f'static_assert(std::is_same<decltype(&reference::{name}),decltype(&::{ours})>::value,"exact {name} ABI");')
    for source,pattern in ((internal,r'^#define\s+SMP_MODEL_\w+[^\n]*'),
                            (api,r'^#define\s+SMP_(?:SC_SUPPORT_BIT|AUTH_YN_BIT|AUTH_GEN_BOND)[^\n]*')):
        declarations += re.findall(pattern,source,re.M)
    declarations += ['using tSMP_ASSO_MODEL=reference::UINT8;','#define SMP_TRACE_DEBUG(...)', function(act,'smp_get_auth_mode')]
    declarations.append('int main(){ const unsigned expected[10]={0,4,4,4,8,12,12,12,12,0}; for(unsigned i=0;i<10;++i) if(smp_get_auth_mode(i)!=expected[i]) return 1; return 0; }')
    # Check actual SDK dataflow as well as isolated association-mode execution.
    for token in ('auth = smp_get_auth_mode(p_cb->selected_association_model)',
                  'p_cb->peer_auth_req & p_cb->loc_auth_req & SMP_AUTH_GEN_BOND','p_cb->auth_mode = auth'):
        if token not in act: raise ValueError('resolved mode dataflow changed')
    for token in ('evt_data.cmplt.auth_mode = p_cb->auth_mode','evt_data.cmplt.auth_mode =  p_rec->ble.auth_mode',
                  '(*p_callback) (SMP_COMPLT_EVT, pairing_bda, &evt_data)'):
        if token not in util: raise ValueError('original terminal callback dataflow changed')
    if 'smp_cb.p_callback = p_cback' not in sources['host/bluedroid/stack/smp/smp_api.c']: raise ValueError('original registration changed')
    with tempfile.TemporaryDirectory(prefix='larp-security-abi-') as directory:
        path=Path(directory)/'reference.cpp'; path.write_text('\n'.join(declarations),encoding='utf-8')
        output=Path(directory)/'reference.exe'; environment=os.environ.copy(); environment['PATH']=str(args.compiler.parent)+os.pathsep+environment.get('PATH','')
        result=subprocess.run([str(args.compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',str(path),'-o',str(output)],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60,env=environment)
        if result.returncode: raise ValueError(f'compiler Exit{result.returncode}: '+result.stdout+result.stderr)
        result=subprocess.run([str(output)],capture_output=True,timeout=10,env=environment)
        if result.returncode: raise ValueError('original association-mode function disagrees')
    print('11 pinned official sources; completion5/5 offsets, callback and5 exact function ABIs;10 resolved SDK association models. Compile/run on host only; no SMP/RF measurement.')
    return 0
if __name__=='__main__': raise SystemExit(main())
