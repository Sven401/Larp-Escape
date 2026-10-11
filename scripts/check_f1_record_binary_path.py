"""Pin the linked ESP32 BLE archive and audit its static key-selection branches.

Relevant instruction offsets are binary facts, not a complete private-record ABI
or evidence of any runtime-selected bond. No key material or NVS is read.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SDK = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32")
TOOLCHAIN = Path("C:/Users/seide/.platformio/packages/toolchain-xtensa-esp32/bin")
ARCHIVE = SDK / "lib/libbt.a"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--map", type=Path, default=ROOT / ".pio/build/esp32-gatts-abi-probe/firmware.map")
parser.add_argument("--expect-no-host-ble", action="store_true",
                    help="verify that the regular product ELF does not link the BLE host key path")
args = parser.parse_args()
MAP = args.map
HEADER = ROOT / "test/sdk_original/esp-idf-v4.4.6/btm_int.h"
PINS = {
    ARCHIVE: "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787",
    HEADER: "002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3",
}
for path, expected in PINS.items():
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("pinned SDK input changed: " + str(path))
map_text = MAP.read_text(encoding="utf-8", errors="replace")
if args.expect_no_host_ble:
    if "libbt.a(btm_ble.c.obj)" in map_text or any(
            re.search(r"(?m)^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", map_text)
            for symbol in ("btm_ble_start_encrypt", "btm_ble_ltk_request_reply")):
        raise ValueError("unexpected BLE host key path in product map")
    print("Regular ESP32 product map has no linked BLE host key path; no product F1 key branch is selected.")
    sys.exit(0)
if not re.search(r"libbt\.a\(btm_ble\.c\.obj\)", map_text):
    raise ValueError("BLE archive object missing from ABI probe map")
for symbol in ("BTM_SecAddBleDevice", "btm_ble_start_encrypt",
               "btm_ble_ltk_request", "btm_ble_ltk_request_reply"):
    if not re.search(r"(?m)^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", map_text):
        raise ValueError("linked ABI probe lacks symbol: " + symbol)
header = HEADER.read_text(encoding="utf-8")
for declaration in ("BT_OCTET16          pltk", "BT_OCTET16          lltk",
                    "BT_OCTET8           rand", "UINT16              ediv",
                    "tBTM_LE_KEY_TYPE    key_type", "UINT16 auth_mode",
                    "UINT16              ble_hci_handle"):
    if header.count(declaration) != 1:
        raise ValueError("original declaration missing/ambiguous: " + declaration)

environment = os.environ.copy()
environment["PATH"] = str(TOOLCHAIN) + os.pathsep + environment.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="f1-binary-", dir=ROOT / ".pio") as temporary:
    work = Path(temporary)
    subprocess.run([str(TOOLCHAIN / "xtensa-esp32-elf-ar.exe"), "x", str(ARCHIVE),
                    "btm_ble.c.obj"], cwd=work, env=environment, check=True)
    obj = work / "btm_ble.c.obj"
    if hashlib.sha256(obj.read_bytes()).hexdigest() != "0580200ccf2118e8e3056793ab571a71ecc0b6a17d9c0148d9ab85614423d5b1":
        raise ValueError("BLE archive member changed")

    def instructions(symbol):
        output = subprocess.run([str(TOOLCHAIN / "xtensa-esp32-elf-objdump.exe"),
                                 "-dr", "--disassemble=" + symbol, str(obj)],
                                env=environment, check=True, capture_output=True, text=True).stdout
        marker = "<" + symbol + ">:"
        if output.count(marker) != 1:
            raise ValueError("binary symbol missing/ambiguous: " + symbol)
        return output.split(marker, 1)[1].split("Disassembly of section", 1)[0]

    master = instructions("btm_ble_start_encrypt")
    slave = instructions("btm_ble_ltk_request_reply")
    request = instructions("btm_ble_ltk_request")
    restore = instructions("BTM_SecAddBleDevice")
    link_encrypted = instructions("btm_ble_link_encrypted")
    connected = instructions("btm_ble_connected")
    expected = {
        "record allocation size 328": (restore, r"movi\s+a10, 0x148[\s\S]{0,180}R_XTENSA_ASM_EXPAND\s+malloc"),
        "record initialization size 328": (restore, r"movi\s+a12, 0x148[\s\S]{0,400}R_XTENSA_ASM_EXPAND\s+memset"),
        "record address offset 32 six-byte copy": (restore, r"movi\.n\s+a12, 6[\s\S]{0,220}addi\s+a10, a5, 32[\s\S]{0,220}R_XTENSA_ASM_EXPAND\s+memcpy"),
        "BLE subrecord base 176": (restore, r"movi\s+a10, 176[\s\S]{0,1600}add\.n\s+a10, a5, a10"),
        "restored mode offset 304 width 16": (restore, r"s16i\s+a6, a5, 0x130"),
        "security state offset 150": (master, r"l8ui\s+a2, a10, 150"),
        "role_master read offset 152": (link_encrypted, r"l8ui\s+a2, a4, 152"),
        "role_master write offset 152": (connected, r"s8i\s+a3, a8, 152"),
        "master HCI handle": (master, r"l16ui\s+a10, a10, 166"),
        "master PENC mask": (master, r"(?s)l8ui\s+a2, a5, 199.{0,100}bbci\s+a2, 0,"),
        "master pltk offset": (master, r"movi\s+a13, 216"),
        "master rand offset": (master, r"movi\s+a11, 0x118"),
        "master ediv offset": (master, r"l16ui\s+a12, a5, 0x120"),
        "master HCI send": (master, r"R_XTENSA_ASM_EXPAND\s+btsnd_hcic_ble_start_enc"),
        "slave HCI handle": (slave, r"l16ui\s+a10, a10, 166"),
        "slave LENC mask": (slave, r"(?s)l8ui\s+a9, a8, 199.{0,100}bbci\s+a9, 4,"),
        "slave key size offset 293": (slave, r"l8ui\s+a11, a11, 37"),
        "slave lltk offset": (slave, r"movi\s+a11, 248"),
        "slave positive reply": (slave, r"R_XTENSA_ASM_EXPAND\s+btsnd_hcic_ble_ltk_req_reply"),
        "slave negative reply": (slave, r"R_XTENSA_ASM_EXPAND\s+btsnd_hcic_ble_ltk_req_neg_reply"),
        "request handle lookup": (request, r"R_XTENSA_ASM_EXPAND\s+btm_find_dev_by_handle"),
        "request SMP ownership": (request, r"R_XTENSA_ASM_EXPAND\s+smp_proc_ltk_request"),
        "request reply call": (request, r"R_XTENSA_ASM_EXPAND\s+btm_ble_ltk_request_reply"),
    }
    for label, (body, pattern) in expected.items():
        if not re.search(pattern, body):
            raise ValueError("static binary evidence missing: " + label)

print(f"Pinned ESP32 BLE archive and linked ABI-probe object: {len(expected)} static checks passed; binary allocation size 328 and relevant offsets established. Runtime-selected key remains unproved.")
