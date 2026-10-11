"""Check why a BLE encryption result cannot attest one restored key/birth.

Read only pinned upstream source and the installed Arduino target configuration.
No keys, bonds, controller, or product grant are accessed.
"""

import hashlib
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "test/sdk_original/esp-idf-v4.4.6/btm_ble.c"
CONFIG = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/dio_qspi/include/sdkconfig.h")
ARCHIVE = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/lib/libbt.a")
PINS = {
    SOURCE: "2a1805a0643666d671d24ca5a1e5c5072a509aa88fb53140b513c3161d5a7f65",
    CONFIG: "5a87fa1f3311cb7c832dce58b9b842cb31642907d74ebf9e94fec4b502c07f3b",
    ARCHIVE: "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787",
}
for path, expected in PINS.items():
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("pinned Arduino/upstream input changed: " + str(path))

source = SOURCE.read_text(encoding="utf-8")
config = CONFIG.read_text(encoding="utf-8")
for option in ("CONFIG_BT_ENABLED", "CONFIG_BT_BLUEDROID_ENABLED", "CONFIG_BT_BLE_ENABLED",
               "CONFIG_BT_BLE_SMP_ENABLE", "CONFIG_BT_GATTS_ENABLE"):
    if not re.search(r"(?m)^#define " + option + r" 1$", config):
        raise ValueError("Arduino ESP32 BLE configuration changed: " + option)


def body(name, return_type):
    matches = list(re.finditer(r"(?m)^" + return_type + r"\s+" + name + r"\s*\([^;]*?\)\s*\{", source))
    if len(matches) != 1:
        raise ValueError("original function absent or ambiguous: " + name)
    end, depth = matches[0].end(), 1
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError("unbalanced original function: " + name)
    return source[matches[0].start():end]


master = body("btm_ble_start_encrypt", "tBTM_STATUS")
slave = body("btm_ble_ltk_request_reply", "void")
master_calls = re.findall(r"btsnd_hcic_ble_start_enc\s*\(([^;]+?)\)\s*\)", master, re.DOTALL)
slave_calls = re.findall(r"btsnd_hcic_ble_ltk_req_reply\s*\(([^;]+?)\)\s*;", slave, re.DOTALL)
if len(master_calls) != 2 or len(slave_calls) != 2:
    raise ValueError("HCI key-send site count changed")
for expected in ("dummy_rand, 0, stk", "p_rec->ble.keys.ediv, p_rec->ble.keys.pltk"):
    if not any(expected in " ".join(call.split()) for call in master_calls):
        raise ValueError("master STK/PENC branch changed: " + expected)
for expected in ("btm_cb.enc_handle, stk", "btm_cb.enc_handle, p_rec->ble.keys.lltk"):
    if not any(expected in " ".join(call.split()) for call in slave_calls):
        raise ValueError("slave STK/LENC branch changed: " + expected)
for name, branch in (("master", master), ("slave", slave)):
    if "auth_mode" in branch or "birth" in branch:
        raise ValueError(name + " key-send branch gained a mode/birth witness; re-audit")

# Identical controller Encryption Change bytes admit different SDK histories.
# The event carries status, handle and enabled, with no key, send, or birth ID.
histories = (
    (0, 0x41, 1, "restored PENC", 0x000D, "birth-A"),
    (0, 0x41, 1, "restored PENC", 0x010D, "birth-A"),
    (0, 0x41, 1, "SMP STK", 0x000D, "birth-A"),
    (0, 0x41, 1, "restored PENC", 0x000D, "birth-B"),
)
assert len({history[:3] for history in histories}) == 1
assert len({history[3:] for history in histories}) == 4

print("Pinned Arduino BLE target configuration and four original STK/PENC/LENC HCI send sites verified; one Encryption Change tuple admits four distinct histories. No exclusive send, selected full mode, key, or original-birth witness; F1 grant closed.")
