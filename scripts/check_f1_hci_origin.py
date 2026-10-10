"""Pinned HCI prefix/dispatch ABI and original SMP completion provenance limit.

The completion's private control/device records are explicit models. The test
executes unmodified original functions and never claims their modeled layout or
NVS durability. No SDK initialization, radio, source download or fixture export.
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
        raise ValueError("pinned source changed: " + str(path))
    return data.decode("utf-8")


def function(source, name):
    match = re.search(r"(?m)^(?:static\s+)?void\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", source)
    if not match:
        raise ValueError("original function missing: " + name)
    depth, end = 1, match.end()
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError("unbalanced original function")
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gatt-source-dir", type=Path, required=True)
    parser.add_argument("--security-source-dir", type=Path, required=True)
    parser.add_argument("--hci-source", type=Path, required=True)
    parser.add_argument("--hci-definitions", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    args = parser.parse_args()
    common, types, task = [checked(args.gatt_source_dir / name, GATT_PINS[name])
                           for name in ("bt_common.h", "bt_types.h", "btu_task.c")]
    api, util = [checked(args.security_source_dir / name, SECURITY_PINS[name])
                 for name in ("host/bluedroid/stack/include/stack/smp_api.h",
                              "host/bluedroid/stack/smp/smp_utils.c")]
    hci = checked(args.hci_source, "e4485a0f4a035119827050596c0ac707158ebafc3946afecd22725efc8e1d81f")
    definitions = checked(args.hci_definitions, "1954310b1228386b8ae8710910e3122084f954bbaf71328e371f30672484bde9")
    dispatch = function(hci, "btu_hcif_process_event")
    for expression in (
            r"case HCI_ENCRYPTION_CHANGE_EVT:\s*#if \(SMP_INCLUDED == TRUE\)\s*btu_hcif_encryption_change_evt \(p\);",
            r"case HCI_ENCRYPTION_KEY_REFRESH_COMP_EVT:\s*#if \(SMP_INCLUDED == TRUE\)\s*btu_hcif_encryption_key_refresh_cmpl_evt\(p\);",
            r"UINT8\s+\*p = \(UINT8 \*\)\(p_msg \+ 1\) \+ p_msg->offset;",
            r"STREAM_TO_UINT8\s*\(hci_evt_code, p\);\s*STREAM_TO_UINT8\s*\(hci_evt_len, p\);"):
        if not re.search(expression, dispatch):
            raise ValueError("original dispatch source changed")
    if not re.search(r"case BT_EVT_TO_BTU_HCI_EVT:\s*btu_hcif_process_event \(\(UINT8\)\(p_msg->event & BT_SUB_EVT_MASK\), p_msg\);\s*osi_free\(p_msg\);", task):
        raise ValueError("original BTU owner/dispatch call changed")
    macros = []
    for source, name in ((definitions, "HCI_ENCRYPTION_CHANGE_EVT"),
                         (definitions, "HCI_ENCRYPTION_KEY_REFRESH_COMP_EVT"),
                         (types, "BT_EVT_TO_BTU_HCI_EVT")):
        found = re.search(r"(?m)^#define\s+" + name + r"\s+0x[0-9A-Fa-f]+", source)
        if not found:
            raise ValueError("original event constant missing")
        macros.append(found[0])
    root = Path(__file__).resolve().parents[1]
    cpp = ['#include <cassert>\n#include <cstdint>\n#include <cstddef>\n#include <cstring>\n#include <type_traits>\n#include <cstdio>',
           '#include "' + (root / 'lib/GattsWriteAdmission/SdkHciEventAbi.h').as_posix() + '"',
           *macros, typedef(common, 'UINT8'), typedef(common, 'UINT16'),
           'using BD_ADDR=UINT8[6];\n#define UNUSED_ATTR\n#define TRUE 1\n#define FALSE 0\n#define BLE_INCLUDED TRUE\n#define SMP_INCLUDED TRUE',
           '#define SMP_SLAVE_CON_PARAMS_UPD_ENABLE FALSE\n#define SMP_TRACE_DEBUG(...)',
           '#define STREAM_TO_UINT8(v,p) do { (v)=*(p)++; } while(0)',
           '#define STREAM_TO_UINT16(v,p) do { (v)=UINT16((p)[0]|UINT16((p)[1])<<8); (p)+=2; } while(0)',
           'namespace original_header {\n#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wpedantic"',
           block(types, 'BT_HDR', 'struct'), '#pragma GCC diagnostic pop\n}',
           'using Header=larp::companion::sdk446::HciHeader;',
           'static_assert(sizeof(original_header::BT_HDR)==sizeof(Header) && alignof(original_header::BT_HDR)==alignof(Header),"complete original prefix ABI");']
    for original, own in (('event', 'event'), ('len', 'length'), ('offset', 'offset'), ('layer_specific', 'layerSpecific')):
        cpp.append(f'static_assert(offsetof(original_header::BT_HDR,{original})==offsetof(Header,{own}),"original {original} offset");')
    cpp += ['namespace reference { using BT_HDR=Header;', dispatch[:dispatch.index('{')].strip() + ';', '}',
            'static_assert(std::is_same<decltype(&reference::btu_hcif_process_event),decltype(&__real_btu_hcif_process_event)>::value,"original external function signature");',
            'static_assert(HCI_ENCRYPTION_CHANGE_EVT==8 && HCI_ENCRYPTION_KEY_REFRESH_COMP_EVT==0x30 && BT_EVT_TO_BTU_HCI_EVT==0x1000,"original SDK event constants");',
            'static unsigned aclCalls=0, secCalls=0; static UINT16 lastHandle=0; static UINT8 lastStatus=0,lastEnabled=0;',
            'static void btm_acl_encrypt_change(UINT16,UINT8,UINT8){++aclCalls;}',
            'static void btm_sec_encrypt_change(UINT16 h,UINT8 s,UINT8 e){++secCalls;lastHandle=h;lastStatus=s;lastEnabled=e;}',
            '#define HCI_SUCCESS 0', function(hci, 'btu_hcif_encryption_change_evt'),
            function(hci, 'btu_hcif_encryption_key_refresh_cmpl_evt')]
    for name in ('tSMP_STATUS', 'tSMP_SEC_LEVEL', 'tSMP_AUTH_REQ'):
        cpp.append(typedef(api, name))
    cpp += [typedef(common, 'BOOLEAN'), block(api, 'tSMP_CMPL', 'struct'), r'''
// Private record layout intentionally modeled; only original mode selection runs.
union tSMP_EVT_DATA { tSMP_CMPL cmplt; };
using tSMP_CALLBACK = UINT8(UINT8, UINT8 *, tSMP_EVT_DATA *);
struct tSMP_CB {
    tSMP_CALLBACK *p_callback;
    BD_ADDR pairing_bda;
    UINT8 status, smp_over_br, sec_level, auth_mode;
    bool is_pair_cancel;
};
struct tBTM_SEC_DEV_REC { struct { UINT16 auth_mode; } ble; };
static tBTM_SEC_DEV_REC record;
static bool recordExists=true, wasReset=false;
static tSMP_CMPL result;
#define SMP_SUCCESS 0
#define SMP_COMPLT_EVT 7
#define BD_ADDR_LEN 6
static tBTM_SEC_DEV_REC *btm_find_dev(UINT8 *) {return recordExists?&record:nullptr;}
static void smp_reset_control_value(tSMP_CB *cb) { cb->auth_mode=0; wasReset=true; }
static void smp_clear_local_oob_data() {}
static UINT8 callback(UINT8 event,UINT8 *address,tSMP_EVT_DATA *data) {
    assert(wasReset && event==7 && address[0]==1);
    result=data->cmplt;
    return 0;
}
''',
            # Original C uses the valid zero initializer {0}; silence only the
            # C++ compatibility warning around that unchanged original function.
            '#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wmissing-field-initializers"',
            function(util, 'smp_proc_pairing_cmpl'), '#pragma GCC diagnostic pop', r'''
static tSMP_CMPL completion(UINT8 fresh,UINT16 stored,bool exists=true,UINT8 status=0,bool cancelled=false) {
    record.ble.auth_mode=stored;recordExists=exists;wasReset=false;
    tSMP_CB cb={callback,{1,2,3,4,5,6},status,0,4,fresh,cancelled};
    smp_proc_pairing_cmpl(&cb);
    assert(wasReset && cb.auth_mode==0);
    return result;
}
int main() {
    UINT8 encryption[4]={0,0x23,1,1}, refresh[3]={0,0x23,1};
    btu_hcif_encryption_change_evt(encryption);
    assert(aclCalls==1 && secCalls==1 && lastHandle==0x123 && lastStatus==0 && lastEnabled==1);
    btu_hcif_encryption_key_refresh_cmpl_evt(refresh);
    assert(aclCalls==1 && secCalls==2 && lastHandle==0x123 && lastStatus==0 && lastEnabled==1);
    unsigned cases=0;
    for(UINT8 mode: {UINT8(13),UINT8(29)}) {
        const auto fresh=completion(mode,1); assert(fresh.auth_mode==mode && record.ble.auth_mode==mode);++cases;
        const auto restored=completion(0,mode); assert(std::memcmp(&fresh,&restored,sizeof(fresh))==0);++cases;
    }
    // Original record is 16-bit, callback is 8-bit: retained high bits are lost.
    assert(completion(0,0x10d).auth_mode==13);++cases;
    assert(completion(0,0x11d).auth_mode==29);++cases;
    assert(completion(0,0).auth_mode==0);++cases;
    assert(completion(0,13,false).auth_mode==0);++cases;
    assert(completion(13,13,true,1).auth_mode==0);++cases;
    assert(completion(0,13,true,0,true).is_pair_cancel);++cases;
    std::printf("HCI prefix/4 offsets/call ABI and 2 original dispatcher cases passed; %u original SMP completion cases: fresh/restore modes 13 and 29 are byte-identical at callback; modeled private records, no restore grant\n",cases);
}
''']
    cpp.insert(0, '#include <initializer_list>')
    with tempfile.TemporaryDirectory(prefix='larp-f1-origin-') as temp:
        source, binary = Path(temp) / 'original.cpp', Path(temp) / 'original.exe'
        source.write_text('\n'.join(cpp), encoding='utf-8')
        env = os.environ.copy()
        env['PATH'] = str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
        subprocess.run([str(args.compiler), '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', str(source), '-o', str(binary)], env=env, check=True)
        subprocess.run([str(binary)], env=env, check=True)
    print('7 unmodified SDK sources SHA256 verified; HCI dispatch selection/BTU owner checked; no SDK/RF run')


if __name__ == '__main__':
    main()
