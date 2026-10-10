"""Compile the full original SMP type for Xtensa and audit pinned binary reads.

No modeled private layout is accepted. Original struct declarations and scalar
aliases are extracted unchanged from SHA256-pinned IDF4.4.6 files. This checks
only the narrow metadata reads, never treats a stored mode as role authority.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys
sys.dont_write_bytecode = True
from check_gatt_source_abi import PINS as GATT_PINS, block, typedef
from check_peer_security_abi import PINS as SECURITY_PINS


def checked(path, pin):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != pin:
        raise ValueError('source/binary changed: ' + str(path))
    return data.decode('utf-8')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--gatt-source-dir', type=Path, required=True)
    p.add_argument('--security-source-dir', type=Path, required=True)
    p.add_argument('--compiler', type=Path, required=True)
    p.add_argument('--objdump', type=Path, required=True)
    p.add_argument('--host-compiler', type=Path, required=True)
    p.add_argument('--sdk-library', type=Path, required=True)
    a = p.parse_args()
    common = checked(a.gatt_source_dir/'bt_common.h', GATT_PINS['bt_common.h'])
    types = checked(a.gatt_source_dir/'bt_types.h', GATT_PINS['bt_types.h'])
    defs = checked(a.gatt_source_dir/'bt_defs.h', '65fdc3ee2e62e14139a3a8b52d8197fa7ffdf5f77137c065f1a37b80491f6137')
    target = checked(a.gatt_source_dir/'bt_target.h', '64e33d0928e03c4d1738851e40fb24b9fff291305ab43fd88740f99c5384ab08')
    api = checked(a.security_source_dir/'host/bluedroid/stack/include/stack/smp_api.h', SECURITY_PINS['host/bluedroid/stack/include/stack/smp_api.h'])
    internal = checked(a.security_source_dir/'host/bluedroid/stack/smp/include/smp_int.h', SECURITY_PINS['host/bluedroid/stack/smp/include/smp_int.h'])
    if hashlib.sha256(a.sdk_library.read_bytes()).hexdigest() != '1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787':
        raise ValueError('libbt.a pin changed')
    if not re.search(r'extern void smp_proc_pairing_cmpl\(tSMP_CB \*p_cb\);', internal):
        raise ValueError('original completion function signature changed')
    cb = block(internal, 'tSMP_CB', 'struct')
    if '#' in cb:
        raise ValueError('conditional private layout needs explicit configuration audit')
    if not re.search(r'#ifndef TIMER_PARAM_TYPE\s*#define TIMER_PARAM_TYPE\s+UINT32', target):
        raise ValueError('timer parameter default changed')
    root = Path(__file__).resolve().parents[1]
    cpp = ['#include <cstddef>\n#include <cstdint>\n#include <type_traits>\n#include <cassert>',
           '#define LARP_GATT_SOURCE_HOST_MODEL 1',
           '#include "'+(root/'lib/GattsWriteAdmission/SdkSmpPreResetAbi.h').as_posix()+'"',
           'namespace original {']
    for name in ('UINT8', 'UINT16', 'UINT32', 'BOOLEAN'):
        cpp.append(typedef(common,name))
    cpp.append(typedef(types,'INT32'))
    cpp += ['#define BD_ADDR_LEN 6', re.search(r'typedef UINT8 BD_ADDR\[BD_ADDR_LEN\];',common)[0]]
    for n in (8,16,32):
        cpp += [re.search(r'(?m)^#define\s+BT_OCTET'+str(n)+r'_LEN\s+\d+',types)[0],
                re.search(r'typedef UINT8 BT_OCTET'+str(n)+r'\[BT_OCTET'+str(n)+r'_LEN\];',types)[0]]
    cpp += [typedef(types,'tBLE_ADDR_TYPE'), block(types,'tBLE_BD_ADDR','struct'),
            '#define TIMER_PARAM_TYPE UINT32', 'typedef void (TIMER_CBACK)(void *p_tle);',
            block(defs,'TIMER_LIST_ENT','struct')]
    for name in ('tSMP_EVT','tSMP_SEC_LEVEL','tSMP_OOB_DATA_TYPE','tSMP_IO_CAP','tSMP_OOB_FLAG','tSMP_AUTH_REQ','tSMP_SC_KEY_TYPE'):
        cpp.append(typedef(api,name))
    for name in ('tSMP_STATE','tSMP_BR_STATE','tSMP_ASSO_MODEL'):
        cpp.append(typedef(internal,name))
    for name in ('tSMP_PUBLIC_KEY','tSMP_LOC_OOB_DATA','tSMP_PEER_OOB_DATA','tSMP_SC_OOB_DATA'):
        cpp.append(block(api,name,'struct'))
    cpp += ['union tSMP_EVT_DATA;', re.search(r'typedef\s+UINT8\s*\(tSMP_CALLBACK\)\s*\([^;]*;',api)[0], cb, '}',
            'static_assert(sizeof(void*)==4 && sizeof(original::TIMER_LIST_ENT)==32,"target timer/pointer layout");']
    for name,offset in {'p_callback':0,'pairing_bda':37,'smp_over_br':53,'status':56,'sec_level':61,'auth_mode':598,'is_pair_cancel':702}.items():
        cpp.append(f'static_assert(offsetof(original::tSMP_CB,{name})=={offset},"original target {name}");')
    cpp += ['static_assert(sizeof(original::tSMP_CB)==720,"complete original SMP control size");',
            'static_assert(sizeof(original::tSMP_CB::auth_mode)==1,"original selected mode width");',
            'char verified_original_smp_size[sizeof(original::tSMP_CB)];']
    cpp += [r"""
static original::UINT8 callback(original::tSMP_EVT, original::UINT8 *, original::tSMP_EVT_DATA *) {return 0;}
int main() {
    original::tSMP_CB cb{};
    cb.p_callback=callback;
    for(unsigned i=0;i<6;++i) cb.pairing_bda[i]=i+1;
    cb.smp_over_br=false; cb.status=3; cb.sec_level=4; cb.auth_mode=29; cb.is_pair_cancel=true;
    larp::companion::sdk446::SmpPreReset out;
    assert(larp::companion::sdk446::readSmpPreReset(reinterpret_cast<larp::companion::sdk446::SmpControlBlock *>(&cb),out));
    assert(reinterpret_cast<void(*)()>(out.callback)==reinterpret_cast<void(*)()>(callback));
    for(unsigned i=0;i<6;++i) assert(out.address[i]==i+1);
    assert(out.status==3 && out.overBr==0 && out.securityLevel==4 && out.pairingMode==29 && out.cancelled==1);
    assert(!larp::companion::sdk446::readSmpPreReset(nullptr,out) && out.status==3);
}
"""]
    env=os.environ.copy();env['PATH']=str(a.compiler.parent)+os.pathsep+env.get('PATH','')
    with tempfile.TemporaryDirectory(prefix='larp-smp-layout-') as temp:
        source=Path(temp)/'original.cpp';source.write_text('\n'.join(cpp),encoding='utf-8')
        subprocess.run([str(a.compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-c',str(source),'-o',str(Path(temp)/'original.o')],env=env,check=True)
        host=Path(temp)/'original.exe'
        env['PATH']=str(a.host_compiler.parent)+os.pathsep+env.get('PATH','')
        subprocess.run([str(a.host_compiler),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',str(source),'-o',str(host)],env=env,check=True)
        subprocess.run([str(host)],env=env,check=True)
    disassembly=subprocess.run([str(a.objdump),'-dr','--disassemble=smp_proc_pairing_cmpl',str(a.sdk_library)],env=env,capture_output=True,text=True,check=True).stdout
    body=disassembly.split('<smp_proc_pairing_cmpl>:',1)[1].split('Disassembly of section',1)[0]
    for expected in (r'l32i\s+a4, a2, 0',r'addi\s+a5, a2, 37',r'l8ui\s+a5, a2, 53',
                     r'l8ui\s+a5, a2, 56',r'l8ui\s+a8, a2, 61',r'addmi\s+a9, a2, 0x200',
                     r'l8ui\s+a8, a9, 86',r'l8ui\s+a8, a9, 190',
                     r'l16ui\s+a8, a10, 0x130',r's8i\s+a8, a1, 24',
                     r'R_XTENSA_ASM_EXPAND\s+smp_reset_control_value',r'callx8\s+a4'):
        if not re.search(expected,body):
            raise ValueError('pinned instruction evidence missing: '+expected)
    if body.index('R_XTENSA_ASM_EXPAND\tsmp_reset_control_value') > body.index('callx8\ta4'):
        raise ValueError('reset/callback ordering changed')
    print('6 pinned original headers; complete 720-byte SMP type and 7 metadata offsets compiled for Xtensa; original 32-bit host metadata reader and 12 pinned libbt instruction checks passed. No keys read, no RF or role grant.')


if __name__ == '__main__':
    main()
