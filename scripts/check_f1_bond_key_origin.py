"""Execute pinned original BLE key selection against a deliberately private host model.

This checks source behavior, not the target tBTM_SEC_DEV_REC ABI or actual bond use.
No key material is printed, captured from a device, or exported.
"""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "test/sdk_original/esp-idf-v4.4.6"
LIBBT = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/lib/libbt.a")
LIBBT_PIN = "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787"
PINS = {
    "btm_int.h": "002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3",
    "btm_ble.c": "2a1805a0643666d671d24ca5a1e5c5072a509aa88fb53140b513c3161d5a7f65",
}


def checked(name):
    data = (SOURCE / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != PINS[name]:
        raise ValueError("original source pin changed: " + name)
    return data.decode("utf-8")


def original_function(source, name):
    match = re.search(r"(?m)^void\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", source)
    if not match:
        raise ValueError("original function missing: " + name)
    depth, end = 1, match.end()
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError("unbalanced original function: " + name)
    return source[match.start():end]


header = checked("btm_int.h")
source = checked("btm_ble.c")
if hashlib.sha256(LIBBT.read_bytes()).hexdigest() != LIBBT_PIN:
    raise ValueError("linked ESP32 libbt.a pin changed")
for field in ("BT_OCTET16          pltk", "BT_OCTET16          lltk",
              "BT_OCTET8           rand", "UINT16              ediv",
              "tBTM_LE_KEY_TYPE    key_type", "UINT16 auth_mode",
              "UINT16              ble_hci_handle", "tBTM_SEC_BLE        ble"):
    if header.count(field) != 1:
        raise ValueError("original record declaration missing or ambiguous: " + field)

cpp = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
using UINT8=std::uint8_t; using UINT16=std::uint16_t;
using BOOLEAN=UINT8; using tBTM_STATUS=int;
using BD_ADDR=UINT8[6]; using BT_OCTET8=UINT8[8]; using BT_OCTET16=UINT8[16];
#define TRUE 1
#define FALSE 0
#define SMP_INCLUDED TRUE
#define BT_OCTET8_LEN 8
#define BTM_WRONG_MODE 1
#define BTM_BUSY 2
#define BTM_NO_RESOURCES 3
#define BTM_CMD_STARTED 4
#define BTM_SEC_STATE_IDLE 0
#define BTM_SEC_STATE_ENCRYPTING 2
#define BTM_LE_KEY_PENC 1
#define BTM_LE_KEY_LENC 2
#define BTM_TRACE_DEBUG(...) ((void)0)
#define BTM_TRACE_WARNING(...) ((void)0)
#define BTM_TRACE_ERROR(...) ((void)0)
// This is a host selection model, intentionally NOT the private target ABI.
struct KeyModel { UINT8 rand[8]; UINT16 ediv; UINT8 pltk[16], lltk[16], key_size; };
struct BleModel { UINT16 key_type, auth_mode; KeyModel keys; };
struct tBTM_SEC_DEV_REC { UINT8 bd_addr[6]; UINT16 ble_hci_handle; UINT8 sec_state; BleModel ble; };
struct tBTM_CB { UINT16 enc_handle, ediv; UINT8 enc_rand[8], key_size; };
static tBTM_CB btm_cb{};
static tBTM_SEC_DEV_REC records[2]{};
static int hci_start=0, hci_reply=0, hci_negative=0, smp_calls=0;
static UINT16 observed_handle=0, observed_ediv=0;
static const UINT8 *observed_key=nullptr, *observed_rand=nullptr;
static bool smp_owns_request=false;
static tBTM_SEC_DEV_REC *btm_find_dev(const UINT8 *address) {
  for (auto &record: records) if (!std::memcmp(record.bd_addr,address,6)) return &record;
  return nullptr;
}
static tBTM_SEC_DEV_REC *btm_find_dev_by_handle(UINT16 handle) {
  for (auto &record: records) if (record.ble_hci_handle==handle) return &record;
  return nullptr;
}
static bool smp_proc_ltk_request(const UINT8 *) { ++smp_calls; return smp_owns_request; }
static bool btsnd_hcic_ble_start_enc(UINT16 handle,const UINT8 *rand,UINT16 ediv,const UINT8 *key) {
  ++hci_start; observed_handle=handle; observed_rand=rand; observed_ediv=ediv; observed_key=key; return true;
}
static void btsnd_hcic_ble_ltk_req_reply(UINT16 handle,const UINT8 *key) {
  ++hci_reply; observed_handle=handle; observed_key=key;
}
static void btsnd_hcic_ble_ltk_req_neg_reply(UINT16 handle) {
  ++hci_negative; observed_handle=handle;
}
void btm_ble_ltk_request_reply(BD_ADDR,BOOLEAN,BT_OCTET16);
'''
for name in ("btm_ble_start_encrypt", "btm_ble_ltk_request_reply", "btm_ble_ltk_request"):
    # btm_ble_start_encrypt has a non-void return and is extracted separately below.
    if name == "btm_ble_start_encrypt":
        match = re.search(r"(?m)^tBTM_STATUS\s+btm_ble_start_encrypt\s*\([^;]*?\)\s*\{", source)
        if not match:
            raise ValueError("original start-encrypt function missing")
        depth, end = 1, match.end()
        while depth and end < len(source):
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        if depth:
            raise ValueError("unbalanced start-encrypt function")
        cpp += source[match.start():end] + "\n"
    else:
        cpp += original_function(source, name) + "\n"

cpp += r'''
int main() {
  for (int i=0;i<2;++i) {
    records[i].bd_addr[0]=UINT8(i+1);
    records[i].ble_hci_handle=UINT16(0x40+i);
    records[i].ble.keys.ediv=UINT16(0x300+i);
    records[i].ble.keys.key_size=16;
    records[i].ble.key_type=BTM_LE_KEY_PENC|BTM_LE_KEY_LENC;
    records[i].ble.auth_mode=UINT16(i ? 0x010d : 0x000d);
  }
  assert(btm_ble_start_encrypt(records[1].bd_addr,FALSE,nullptr)==BTM_CMD_STARTED);
  assert(hci_start==1 && observed_handle==0x41 && observed_ediv==0x301);
  assert(observed_rand==records[1].ble.keys.rand && observed_key==records[1].ble.keys.pltk);
  assert(observed_key!=records[0].ble.keys.pltk);
  records[1].sec_state=BTM_SEC_STATE_IDLE;
  records[1].ble.key_type=BTM_LE_KEY_LENC;
  assert(btm_ble_start_encrypt(records[1].bd_addr,FALSE,nullptr)==BTM_NO_RESOURCES);
  assert(hci_start==1);
  records[1].ble.key_type=BTM_LE_KEY_PENC|BTM_LE_KEY_LENC;
  UINT8 rand[8]{};
  btm_ble_ltk_request(0x41,rand,0x55);
  assert(smp_calls==1 && hci_reply==1 && observed_handle==0x41);
  assert(observed_key==records[1].ble.keys.lltk && observed_key!=records[1].ble.keys.pltk);
  assert(btm_cb.ediv==0x55 && !std::memcmp(btm_cb.enc_rand,rand,8));
  records[1].ble.key_type=BTM_LE_KEY_PENC;
  btm_ble_ltk_request(0x41,rand,0x56);
  assert(hci_negative==1 && hci_reply==1);
  smp_owns_request=true;
  btm_ble_ltk_request(0x41,rand,0x57);
  assert(smp_calls==3 && hci_reply==1 && hci_negative==1);
  btm_ble_ltk_request(0x99,rand,0x58);
  assert(smp_calls==3 && hci_reply==1 && hci_negative==1);
  // A mode value does not bind the source record to a connection birth.
  assert(records[0].ble.auth_mode!=records[1].ble.auth_mode);
  return 0;
}
'''

compiler = Path("C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe")
if not compiler.is_file():
    raise FileNotFoundError(compiler)
with tempfile.TemporaryDirectory(prefix="f1-bond-source-", dir=ROOT / ".pio") as temporary:
    directory = Path(temporary)
    unit, executable = directory / "source.cpp", directory / "source.exe"
    unit.write_text(cpp, encoding="utf-8")
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    compile_result = subprocess.run([str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                     str(unit), "-o", str(executable)], capture_output=True, text=True, env=environment)
    if compile_result.returncode:
        raise RuntimeError(f"host compiler failed ({compile_result.returncode}):\n" + compile_result.stdout + compile_result.stderr)
    subprocess.run([str(executable)], check=True, env=environment)
print("Pinned SDK BLE key-selection source model passed; target record ABI and actual key use remain unproved.")
