"""Pin the original BLE getter/HCI-send signatures and their result boundary.

This verifies declarations and source control flow, not the complete private
device-record layout, controller acceptance or a live GATT-birth witness.
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
TOOLCHAIN = Path("C:/Users/seide/.platformio/packages/toolchain-xtensa-esp32/bin")
LIBBT = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/lib/libbt.a")
LIBBT_PIN = "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787"
PINS = {
    "btm_int.h": "002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3",
    "btm_dev.c": "9212e1c3d62efaaf582e7f0bffd60487acf328ef8b0a2b40aeaf230e5fae39f7",
    "hcimsgs.h": "31ba1c2c3e3371e2f22dff60629a78f3b4dd9a6f777af6d06bf14a37ed051956",
    "hciblecmds.c": "c3e357e85e6baf9ab53283fa4a36c9e03133172820bd60d5f99d34db7da301ee",
}


def checked(name):
    data = (SOURCE / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != PINS[name]:
        raise ValueError("official v4.4.6 source changed: " + name)
    return data.decode("utf-8")


def unique(pattern, source, label):
    matches = list(re.finditer(pattern, source, re.MULTILINE | re.DOTALL))
    if len(matches) != 1:
        raise ValueError("original declaration missing or ambiguous: " + label)
    return matches[0].group(0)


header = checked("btm_int.h")
getter = checked("btm_dev.c")
hci_header = checked("hcimsgs.h")
hci_send = checked("hciblecmds.c")
if hashlib.sha256(LIBBT.read_bytes()).hexdigest() != LIBBT_PIN:
    raise ValueError("installed ESP32 Bluetooth archive changed")
for size, expected in (("RAND_DI", "8"), ("ENCRYT_KEY", "16")):
    values = re.findall(r"#define\s+HCIC_BLE_" + size + r"_SIZE\s+(\d+)", hci_header)
    if not values or any(value != expected for value in values):
        raise ValueError("original HCI array size changed: " + size)

declarations = [
    unique(r"tBTM_SEC_DEV_REC\s*\*btm_find_dev\s*\(BD_ADDR bd_addr\);", header, "address getter"),
    unique(r"tBTM_SEC_DEV_REC\s*\*btm_find_dev_by_handle\s*\(UINT16 handle\);", header, "handle getter"),
    unique(r"BOOLEAN\s+btsnd_hcic_ble_start_enc\s*\(\s*UINT16 handle,\s*UINT8 rand\[HCIC_BLE_RAND_DI_SIZE\],\s*UINT16 ediv, UINT8 ltk\[HCIC_BLE_ENCRYT_KEY_SIZE\]\);", hci_header, "master HCI send"),
    unique(r"BOOLEAN\s+btsnd_hcic_ble_ltk_req_reply\s*\(UINT16 handle, UINT8 ltk\[HCIC_BLE_ENCRYT_KEY_SIZE\]\);", hci_header, "slave HCI reply"),
]

for name in ("btsnd_hcic_ble_start_enc", "btsnd_hcic_ble_ltk_req_reply"):
    body = unique(r"BOOLEAN\s+" + name + r"\s*\([^;]*?\)\s*\{.*?\n\}", hci_send, name)
    if not re.search(r"HCI_GET_CMD_BUF\([^)]*\).*?return \(FALSE\);", body, re.DOTALL):
        raise ValueError(name + " lost allocation-failure result")
    if not re.search(r"btu_hcif_send_cmd\s*\([^;]*\);\s*return \(TRUE\);", body):
        raise ValueError(name + " no longer returns enqueue-only success")

for phrase in (
    "context.type                = SEC_DEV_HDL;",
    "context.type               = SEC_DEV_BTDM_BDA;",
    "p_context->context.handle == p_sec_dev->hci_handle",
    "p_context->context.handle == p_sec_dev->ble_hci_handle",
    "p_sec_dev->ble.pseudo_addr",
    "btm_ble_addr_resolvable(p_context->context.p_bd_addr, p_sec_dev)",
):
    if phrase not in getter:
        raise ValueError("getter lookup behavior changed: " + phrase)

unit = """#include <cstdint>
#include <type_traits>
using UINT8=std::uint8_t; using UINT16=std::uint16_t; using BOOLEAN=UINT8;
using BD_ADDR=UINT8[6]; struct tBTM_SEC_DEV_REC;
#define HCIC_BLE_RAND_DI_SIZE 8
#define HCIC_BLE_ENCRYT_KEY_SIZE 16
extern \"C\" {
""" + "\n".join(declarations) + """
}
static_assert(sizeof(void*)==4 && sizeof(BOOLEAN)==1, "pinned ESP32 ABI width");
static_assert(std::is_same<decltype(&btm_find_dev),tBTM_SEC_DEV_REC*(*)(UINT8*)>::value,"address getter");
static_assert(std::is_same<decltype(&btm_find_dev_by_handle),tBTM_SEC_DEV_REC*(*)(UINT16)>::value,"handle getter");
static_assert(std::is_same<decltype(&btsnd_hcic_ble_start_enc),BOOLEAN(*)(UINT16,UINT8*,UINT16,UINT8*)>::value,"master HCI send");
static_assert(std::is_same<decltype(&btsnd_hcic_ble_ltk_req_reply),BOOLEAN(*)(UINT16,UINT8*)>::value,"slave HCI send");
"""
compiler = TOOLCHAIN / "xtensa-esp32-elf-g++.exe"
environment = os.environ.copy()
environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
symbols = subprocess.run([str(TOOLCHAIN / "xtensa-esp32-elf-nm.exe"), "-g", str(LIBBT)],
                         env=environment, check=True, capture_output=True, text=True).stdout
for name in ("btm_find_dev", "btm_find_dev_by_handle", "btsnd_hcic_ble_start_enc",
             "btsnd_hcic_ble_ltk_req_reply"):
    if not re.search(r"(?m)^[0-9a-f]+ T " + name + r"$", symbols):
        raise ValueError("pinned archive does not export " + name)
with tempfile.TemporaryDirectory(prefix="f1-hci-abi-", dir=ROOT / ".pio") as temporary:
    source = Path(temporary) / "abi.cpp"
    source.write_text(unit, encoding="utf-8")
    subprocess.run([str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-c", str(source), "-o", str(Path(temporary) / "abi.o")],
                   env=environment, check=True)
print("Four pinned v4.4.6 getter/HCI declarations compile for Xtensa; getters can match classic handle or resolvable address; both HCI booleans mean command enqueued, not controller acceptance. Full record ABI and live birth remain unproved.")
