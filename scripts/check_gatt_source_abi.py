"""Compare the narrow private ABI with types extracted from pinned official sources.

Read only input sources, compile a generated temporary reference, perform no SDK
initialization or network IO. Hashes pin the unmodified ESP-IDF v4.4.6 source.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile

PINS={
"bt_common.h":"3a96651561a198a05500fbd57152d30d8b2a530109608d523ec3b92093ac1fdc",
"bt_types.h":"b1a8d8c8d84155be65646f2cac35a1bd8abdd656f194c2309b58866999d319a0",
"btm_ble_api.h":"539f0c805cd5ee6cdcfA44676d7e6ada9d86dc57ab89bd9d5ce50a15b2820402".lower(),
"gatt_api.h":"811d9a52e85877a30635a3d80ce58b918dc10e6184128972e3f7c435fe24d87b",
"gatt_int.h":"1c392e95e468a4c5482af7306a6c401b39c1b58b57c500fd163a839ca552e657",
"gatt_api.c":"4b6ed1bbdbe99412395d96cc432e8b8c2477fd852a4a8f306c4f6af680fa9658",
"gatt_sr.c":"e075707d55d53db721189c881f39d4b01f89565dca36eaa83bb1dce2712027da",
"att_protocol.c":"2c05d16316b509a46834c0146b5003a5610e22da7000f85cf6320d0e29addc9a",
"btu_task.c":"a1669a352dff01f0fb412aeb54e98eed566f5dda2355f10dbdf69285870bba03",
"osi_thread.c":"42702b54271f07e9e2b7116eaa4631d1e19586798b1672199e99df42cb9fffd1",
"osi_thread.h":"2e7dcafef285d903c734e6cfedb2bf35b2290e1fee98b323fad34e523ba24974",
"bta_gatts_act.c":"e98946184f528686f8a95af8a5e448f37c01d94b4d81b4dcba4688cd545fe79d",
}

def block(source,name,kind):
    end=re.search(r"\}\s*"+re.escape(name)+r"\s*;",source)
    if not end: raise ValueError(f"source typedef absent: {name}")
    start=source.rfind("typedef "+kind,0,end.start())
    if start<0: raise ValueError(f"source typedef start absent: {name}")
    return source[start:end.end()]

def typedef(source,name):
    found=re.search(r"typedef\s+\w+\s+"+re.escape(name)+r"\s*;",source)
    if not found: raise ValueError(f"source scalar absent: {name}")
    return found[0]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir",type=Path,required=True)
    parser.add_argument("--compiler",type=Path,required=True)
    args=parser.parse_args()
    source={}
    for name,pin in PINS.items():
        data=(args.source_dir/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=pin:
            raise ValueError(f"source changed: {name}")
        source[name]=data.decode("utf-8")
    common=source["bt_common.h"]; api=source["gatt_api.h"]
    if not re.search(r"GATTS_REQ_TYPE_READ\s*=\s*1",api): raise ValueError("original read callback kind changed")
    for token in ("case GATT_RSP_READ_BLOB:", "case GATT_RSP_READ:",
                  "len = payload_size - p_buf->len;", "ARRAY_TO_STREAM (p, p_data, len);"):
        if token not in source["att_protocol.c"]: raise ValueError("original read truncation changed")
    for token in ("if (p_tcb->sr_cmd.trans_id != trans_id)", "gatt_sr_process_app_rsp"):
        if token not in source["gatt_api.c"]: raise ValueError("original transaction check changed")
    declarations=["#include <cstddef>\n#include <cstdint>\n#include <type_traits>\n",
        "#define LARP_GATT_SOURCE_HOST_MODEL 1\n",
        '#include "'+(Path(__file__).resolve().parents[1]/"lib/GattsWriteAdmission/SdkGattSourceAbi.h").as_posix()+'"\n',
        "namespace reference {\n",
        typedef(common,"UINT8"),typedef(common,"UINT16"),typedef(common,"UINT32"),
        typedef(common,"BOOLEAN"),"#define MAX_UUID_SIZE 16\n#define GATT_MAX_ATTR_LEN 600\n",
        block(common,"tBT_UUID","struct"),"typedef UINT8 BD_ADDR[6];",
        typedef(source["btm_ble_api.h"],"tGATT_IF"),typedef(source["bt_types.h"],"tBT_TRANSPORT"),
        typedef(api,"tGATT_STATUS"),typedef(api,"tGATT_DISCONN_REASON"),
        typedef(api,"tGATT_AUTH_REQ"),typedef(api,"tGATTS_REQ_TYPE"),
        typedef(api,"tGATT_EXEC_FLAG"),typedef(api,"tGATTC_OPTYPE"),typedef(api,"tGATT_DISC_TYPE"),
        block(api,"tGATT_VALUE","struct"),block(api,"tGATTS_RSP","union"),
        block(api,"tGATT_CL_COMPLETE","union"),block(api,"tGATT_READ_REQ","struct"),
        block(api,"tGATT_WRITE_REQ","struct"),block(api,"tGATTS_DATA","union"),
        "struct tGATT_DISC_RES;\n"]
    for name in ("tGATT_CONN_CBACK","tGATT_REQ_CBACK","tGATT_CMPL_CBACK","tGATT_DISC_RES_CB",
                 "tGATT_DISC_CMPL_CB","tGATT_ENC_CMPL_CB","tGATT_CONGESTION_CBACK"):
        found=re.search(r"typedef\s+void\s*\(\s*"+name+r"\s*\)[\s\S]*?;",api)
        if not found: raise ValueError(f"callback declaration absent: {name}")
        declarations.append(found[0])
    declarations.extend([block(api,"tGATT_CBACK","struct"),"}\nnamespace actual=larp::companion::sdk446;\n"])
    for ours,ref in (("Uuid","tBT_UUID"),("Read","tGATT_READ_REQ"),("Write","tGATT_WRITE_REQ"),
                     ("Request","tGATTS_DATA"),("Value","tGATT_VALUE"),("Response","tGATTS_RSP"),
                     ("Complete","tGATT_CL_COMPLETE"),("Callbacks","tGATT_CBACK")):
        declarations.append(f'static_assert(sizeof(actual::{ours})==sizeof(reference::{ref}),"{ours} size");')
        declarations.append(f'static_assert(alignof(actual::{ours})==alignof(reference::{ref}),"{ours} alignment");')
    for ours,ref,pairs in (("Read","tGATT_READ_REQ",(("handle","handle"),("offset","offset"),("isLong","is_long"),("needResponse","need_rsp"))),
                         ("Write","tGATT_WRITE_REQ",(("handle","handle"),("offset","offset"),
                           ("length","len"),("value","value"),("needResponse","need_rsp"),("prepared","is_prep"))),
                         ("Value","tGATT_VALUE",(("connection","conn_id"),("handle","handle"),("offset","offset"),("length","len"),("authentication","auth_req"),("value","value"))),
                         ("Callbacks","tGATT_CBACK",(("connection","p_conn_cb"),("complete","p_cmpl_cb"),
                          ("discovery","p_disc_res_cb"),("discoveryComplete","p_disc_cmpl_cb"),
                          ("request","p_req_cb"),("encrypted","p_enc_cmpl_cb"),("congestion","p_congestion_cb")))):
        for own,original in pairs:
            declarations.append(f'static_assert(offsetof(actual::{ours},{own})==offsetof(reference::{ref},{original}),"{ours}.{own} offset");')
    declarations.append('static_assert(std::is_same<actual::ConnectionCallback,reference::tGATT_CONN_CBACK *>::value,"connection callback exact scalar types");')
    with tempfile.TemporaryDirectory(prefix="larp-source-abi-") as directory:
        path=Path(directory)/"reference.cpp"
        path.write_text("\n".join(declarations),encoding="utf-8")
        environment=os.environ.copy()
        environment["PATH"]=str(args.compiler.parent)+os.pathsep+environment.get("PATH", "")
        result=subprocess.run([str(args.compiler),"-std=c++11","-Wall","-Wextra","-Wpedantic","-Werror","-fsyntax-only",str(path)],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=60,env=environment)
        if result.returncode: raise ValueError(f"compiler Exit{result.returncode}: "+result.stdout+result.stderr)
    print(f"{len(PINS)} pinned official sources verified; 8 layouts/alignments, 23 offsets and exact connection callback types agree. Compile only; no SDK/RF run.")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
