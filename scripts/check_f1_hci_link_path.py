"""Verify that the pinned ABI-probe ELF routes original SDK calls through F1 observers.

This is a link-path check only. It never starts Bluetooth, accesses a device,
reads bond keys, or treats an HCI event as proof of a selected key.
"""

import hashlib
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SDK = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32")
TOOLCHAIN = Path("C:/Users/seide/.platformio/packages/toolchain-xtensa-esp32/bin")
BUILD = ROOT / ".pio/build/esp32-gatts-abi-probe"
ELF, MAP = BUILD / "firmware.elf", BUILD / "firmware.map"
ARCHIVE = SDK / "lib/libbt.a"
if hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() != "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787":
    raise ValueError("linked ESP32 Bluetooth archive changed")

platform = (ROOT / "platformio.ini").read_text(encoding="utf-8")
section = re.search(r"(?ms)^\[env:esp32-gatts-abi-probe\]\s*(.*?)(?=^\[|\Z)", platform)
if not section:
    raise ValueError("ABI probe environment missing")
hooks = {
    "btu_hcif_process_event": ("btu_hcif.c.obj", 1),
    "btm_sec_encrypt_change": ("btm_sec.c.obj", 2),
    "smp_proc_pairing_cmpl": ("smp_utils.c.obj", 3),
}
mapping = MAP.read_text(encoding="utf-8", errors="replace")
if "-Wl,--wrap=GATT_Register" not in section.group(1) or "-Wl,--undefined=__wrap_GATT_Register" not in section.group(1):
    raise ValueError("GATT birth registration wrap missing")
if not re.search(r"(?m)^\s*GATT_Register\s+.*libbt\.a\(gatt_api\.c\.obj\)$", mapping):
    raise ValueError("original GATT registration implementation missing")
if not re.search(r"(?m)^\s*__wrap_GATT_Register\s+.*libGattsWriteAdmission\.a\(Esp32GattsSource\.cpp\.o\)$", mapping):
    raise ValueError("GATT birth wrapper missing")
if not re.search(r"\.text\._ZN4larp9companion12_GLOBAL__N_118connectionCallbackEhPhtbth", mapping):
    raise ValueError("selected original GATT connection callback missing")
for symbol, (member, _) in hooks.items():
    if f"-Wl,--wrap={symbol}" not in section.group(1) or f"-Wl,--undefined=__wrap_{symbol}" not in section.group(1):
        raise ValueError("probe wrap/link retention missing: " + symbol)
    if not re.search(r"(?m)^\s*\.text\." + re.escape(symbol) + r"\s*\n\s*0x[0-9a-f]+\s+0x[0-9a-f]+\s+.*libbt\.a\(" + re.escape(member) + r"\)", mapping):
        raise ValueError("pinned SDK member not linked: " + symbol)
    if not re.search(r"(?m)^\s*__wrap_" + re.escape(symbol) + r"\s+\.pio/build/esp32-gatts-abi-probe/src/GattsAbiProbe/F1EncryptAbi\.cpp\.o$", mapping):
        raise ValueError("probe observer source not linked: " + symbol)

objdump = TOOLCHAIN / "xtensa-esp32-elf-objdump.exe"
assembly = subprocess.run([str(objdump), "-d", str(ELF)], check=True,
                          capture_output=True, text=True).stdout
gatt_wrapper = re.search(r"(?m)^[0-9a-f]+ <__wrap_GATT_Register>:$", assembly)
gatt_calls = re.findall(r"(?m)^\s*[0-9a-f]+:\s*[0-9a-f]+\s+call8\s+[0-9a-f]+\s+<__wrap_GATT_Register>$", assembly)
gatt_original = re.findall(r"(?m)^\s*[0-9a-f]+:\s*[0-9a-f]+\s+call8\s+[0-9a-f]+\s+<GATT_Register>$", assembly)
if not gatt_wrapper or len(gatt_calls) != 2 or len(gatt_original) != 2:
    raise ValueError("original GATT registration wrapper call path changed")
gatt_tail = assembly[gatt_wrapper.end():]
gatt_next = re.search(r"(?m)^[0-9a-f]+ <[^>]+>:$", gatt_tail)
if not gatt_next or any(call not in gatt_tail[:gatt_next.start()] for call in gatt_original):
    raise ValueError("original GATT registration call bypasses birth wrapper")
for symbol, (_, count) in hooks.items():
    definition = list(re.finditer(r"(?m)^[0-9a-f]+ <__wrap_" + re.escape(symbol) + r">:$", assembly))
    calls = re.findall(r"(?m)^\s*[0-9a-f]+:\s*[0-9a-f]+\s+call8\s+[0-9a-f]+\s+<__wrap_" + re.escape(symbol) + r">$", assembly)
    original_calls = re.findall(r"(?m)^\s*[0-9a-f]+:\s*[0-9a-f]+\s+call8\s+[0-9a-f]+\s+<" + re.escape(symbol) + r">$", assembly)
    if len(definition) != 1 or len(calls) != count or len(original_calls) != 1:
        raise ValueError(f"linked wrapper/bypass call count changed: {symbol}, definition={len(definition)}, observed={len(calls)}, original={len(original_calls)}")
    tail = assembly[definition[0].end():]
    next_definition = re.search(r"(?m)^[0-9a-f]+ <[^>]+>:$", tail)
    if not next_definition or original_calls[0] not in tail[:next_definition.start()]:
        raise ValueError("original SDK function call bypasses observer: " + symbol)
print("Pinned ABI-probe ELF: 2 GATT registration, 1 BTU dispatch, 2 security-result and 3 SMP-completion SDK call sites reach source wrappers; no direct bypass in this ELF. Product firmware and selected-key provenance remain unproved.")
