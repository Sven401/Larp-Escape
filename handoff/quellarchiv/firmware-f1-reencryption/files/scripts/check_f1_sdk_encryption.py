"""Run the pinned IDF 4.4.6 HCI encryption dispatcher with a conservative birth model.

The ACL and GATT lifecycle are host models. This is no firmware or RF acceptance.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--source-archive", type=pathlib.Path, required=True)
parser.add_argument("--manifest", type=pathlib.Path, required=True)
parser.add_argument("--compiler", type=pathlib.Path, required=True)
args = parser.parse_args()


def original_function(source, name):
    match = re.search(r"(?m)^static void\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", source)
    if not match:
        raise ValueError("Originalfunktion fehlt: " + name)
    depth, end = 1, match.end()
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError("Unausgeglichene Originalfunktion")
    return source[match.start():end]


manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
with zipfile.ZipFile(args.source_archive) as archive:
    names = set(archive.namelist())
    if names != set(manifest["files"]) | {"HERKUNFT.txt"}:
        raise ValueError("Quelleninventar weicht ab")
    sources = {}
    for name, pin in manifest["files"].items():
        data = archive.read(name)
        blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if hashlib.sha256(data).hexdigest() != pin["sha256"] or blob != pin["gitBlob"]:
            raise ValueError("Originalquelle verändert: " + name)
        sources[name] = data.decode("utf-8")

original = original_function(
    sources["components/bt/host/bluedroid/stack/btu/btu_hcif.c"],
    "btu_hcif_encryption_change_evt",
)
cpp = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <set>
using UINT8 = std::uint8_t;
using UINT16 = std::uint16_t;
#define TRUE 1
#define SMP_INCLUDED TRUE
#define STREAM_TO_UINT8(v,p) do { (v) = *(p)++; } while (0)
#define STREAM_TO_UINT16(v,p) do { (v) = UINT16((p)[0] | (UINT16((p)[1]) << 8)); (p) += 2; } while (0)
struct Birth {
    UINT16 hci=0xffff;
    unsigned gatt=0, token=0, lifecycle=0;
    bool live=false, seen=false;
};
static Birth birth;
static std::set<UINT16> used;
static unsigned calls=0, aclCalls=0, secCalls=0;
static bool connect(UINT16 hci, unsigned gatt, unsigned token, unsigned lifecycle) {
    birth.hci=hci; birth.gatt=gatt; birth.token=token; birth.lifecycle=lifecycle; birth.live=false; birth.seen=false;
    if (hci==0xffff || !gatt || !token || !lifecycle || !used.insert(hci).second)
        return false; // A recycled HCI handle has ambiguous old-event provenance.
    birth.live=true;
    return true;
}
static void close() { birth.live=false; birth.seen=false; }
static void btm_acl_encrypt_change(UINT16,UINT8,UINT8) { ++calls; ++aclCalls; }
static void btm_sec_encrypt_change(UINT16 hci,UINT8 status,UINT8 enabled) {
    assert(aclCalls==secCalls+1); // Exact original dispatcher ordering.
    ++calls; ++secCalls;
    if (birth.live && !birth.seen && birth.hci==hci && status==0 && enabled==1)
        birth.seen=true;
}
'''
cpp += "\n" + original + r'''
static void event(UINT16 hci,UINT8 status,UINT8 enabled) {
    UINT8 raw[4]={status,UINT8(hci&255),UINT8(hci>>8),enabled};
    btu_hcif_encryption_change_evt(raw);
}
int main() {
    unsigned cases=0;
    assert(connect(0x123,0x1001,1,1));
    event(0x123,0,1); assert(birth.seen); ++cases;
    close(); event(0x123,0,1); assert(!birth.seen); ++cases;
    assert(connect(0x124,0x1002,2,2));
    event(0x123,0,1); assert(!birth.seen); ++cases;
    event(0x124,1,1); assert(!birth.seen); ++cases;
    event(0x124,0,0); assert(!birth.seen); ++cases;
    event(0x124,0,1); assert(birth.seen); ++cases;
    birth.seen=false; close();
    assert(!connect(0x123,0x1003,3,3));
    event(0x123,0,1); assert(!birth.seen); ++cases;
    assert(!connect(0xffff,0x1004,4,4)); ++cases;
    assert(connect(0x125,0x1005,5,5));
    event(0x124,0,1); assert(!birth.seen); ++cases;
    event(0x125,0,1); assert(birth.seen); ++cases;
    event(0x125,0,1); assert(birth.seen && secCalls==aclCalls); ++cases;
    assert(calls==2*secCalls);
    std::printf("%u pinned original HCI dispatcher / birth-model cases passed; no firmware or RF acceptance\n",cases);
}
'''
with tempfile.TemporaryDirectory(prefix="larp-f1-hci-") as temp:
    source = pathlib.Path(temp) / "f1_hci.cpp"
    executable = pathlib.Path(temp) / "f1_hci.exe"
    source.write_text(cpp, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = str(args.compiler.parent) + os.pathsep + env.get("PATH", "")
    subprocess.run([str(args.compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", str(source), "-o", str(executable)], check=True, env=env)
    subprocess.run([str(executable)], check=True, env=env)
print(f"{len(sources)} pinned SDK source files verified")


