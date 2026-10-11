"""Compile the unchanged v4.4.6 security record declaration against pinned ESP32 ABI facts.

This is a static layout check, not a runtime selected-key or F1 authorization test.
No device, NVS, or key material is accessed.
"""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "test/sdk_original/esp-idf-v4.4.6"
SDK = Path("C:/Users/seide/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32")
COMPILER = Path("C:/Users/seide/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe")
PINS = {
    COMPILER: "adaee963c4ff7607c44a30edf2e906bef99e9be6e204f1da5d0608aebe098c14",
    SOURCE / "btm_int.h": "002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3",
    SOURCE / "btm_ble_int.h": "a2c6c5e146fed250dec43614e502c04a47465e3733c68e7d3f18522a4e8ef452",
    SOURCE / "bluedroid_user_config.h": "3dcd99ef49c3a4828dcf7f59732453b40576e8f175408d60fefbd0c4ccf607ce",
    SOURCE / "bt_target.h": "64e33d0928e03c4d1738851e40fb24b9fff291305ab43fd88740f99c5384ab08",
    SOURCE / "bt_types.h": "b1a8d8c8d84155be65646f2cac35a1bd8abdd656f194c2309b58866999d319a0",
    SOURCE / "bt_common.h": "3a96651561a198a05500fbd57152d30d8b2a530109608d523ec3b92093ac1fdc",
    SOURCE / "btm_api.h": "e1d307c962d21ab72aa6a2056b3234d0fd9f8c24a11a655a10698a9d9b69c84b",
    SOURCE / "hcidefs.h": "1954310b1228386b8ae8710910e3122084f954bbaf71328e371f30672484bde9",
    SDK / "dio_qspi/include/sdkconfig.h": "5a87fa1f3311cb7c832dce58b9b842cb31642907d74ebf9e94fec4b502c07f3b",
    SDK / "include/soc/esp32/include/soc/soc_caps.h": "2e065b741148bc4db9fa6dffd692b786788c3c2fef83398bb4ca936dab30bfbc",
    SDK / "lib/libbt.a": "1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787",
}

texts = {}
for path, expected in PINS.items():
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise ValueError(f"pinned input changed: {path} ({actual})")
    if path.suffix == ".h":
        texts[path.name] = data.decode("utf-8")


def one(name, pattern):
    matches = re.findall(pattern, texts[name], re.S)
    if len(matches) != 1:
        raise ValueError(f"original declaration absent or ambiguous: {name}: {pattern}")
    return matches[0]


record = texts["btm_int.h"]
keys = one("btm_int.h", r"(typedef struct \{\s*BT_OCTET16\s+irk;.*?\} tBTM_SEC_BLE_KEYS;)")
ble = one("btm_int.h", r"(typedef struct \{\s*BD_ADDR pseudo_addr;.*?\} tBTM_SEC_BLE;)")
bond = one("btm_int.h", r"(enum \{\s*BOND_TYPE_UNKNOWN,.*?typedef UINT8 tBTM_BOND_TYPE;)")
device = one("btm_int.h", r"(struct tBTM_SEC_DEV_REC\{.*?\n\};)")
conn = one("btm_ble_int.h", r"(typedef struct \{\s*UINT16\s+min_conn_int;.*?\} tBTM_LE_CONN_PRAMS;)")

# Exact source/config guards. The archive's field offsets are checked separately
# by check_f1_record_binary_path.py; no source-archive identity is assumed.
required = {
    "bluedroid_user_config.h": [
        "#define UC_BT_BLE_ENABLED                   CONFIG_BT_BLE_ENABLED",
        "#define UC_BT_SMP_ENABLE                       CONFIG_BT_SMP_ENABLE",
        "#define UC_BT_BLE_RPA_SUPPORTED            FALSE",
        "#define UC_BT_BLE_50_FEATURES_SUPPORTED            FALSE",
        "#define UC_BT_SMP_SLAVE_CON_PARAMS_UPD_ENABLE  FALSE",
    ],
    "bt_target.h": [
        "#define BLE_PRIVACY_SPT             TRUE",
        "#define CONTROLLER_RPA_LIST_ENABLE   FALSE",
        "#define BTM_DISC_DURING_RS TRUE",
        "#define L2CAP_UCD_INCLUDED                  FALSE",
        "#define BTM_MAX_REM_BD_NAME_LEN     64",
    ],
    "btm_api.h": [
        "#define BTM_SEC_MAX_SERVICES            65",
        "typedef UINT8 tBTM_BD_NAME[BTM_MAX_REM_BD_NAME_LEN + 1];",
        "typedef UINT8 tBTM_LE_KEY_TYPE;",
        "typedef UINT8 tBTM_IO_CAP;",
        "typedef UINT8 tBTM_AUTH_REQ;",
    ],
    "bt_types.h": [
        "typedef UINT8 LINK_KEY[LINK_KEY_LEN];",
        "typedef UINT8 DEV_CLASS[DEV_CLASS_LEN];",
        "typedef UINT8 BD_FEATURES[BD_FEATURES_LEN];",
        "typedef UINT8 tBLE_ADDR_TYPE;",
        "typedef UINT8 tBT_DEVICE_TYPE;",
    ],
    "bt_common.h": [
        "typedef uint8_t UINT8;",
        "typedef uint16_t UINT16;",
        "typedef uint32_t UINT32;",
        "typedef bool BOOLEAN;",
        "typedef UINT8 BD_ADDR[BD_ADDR_LEN];",
    ],
    "hcidefs.h": [
        "#define HCI_EXT_FEATURES_PAGE_2     2",
        "#define HCI_EXT_FEATURES_PAGE_MAX   HCI_EXT_FEATURES_PAGE_2",
    ],
    "sdkconfig.h": [
        "#define CONFIG_BT_BLE_ENABLED 1",
        "#define CONFIG_BT_SMP_ENABLE 1",
    ],
    "soc_caps.h": ["#define SOC_BLE_DEVICE_PRIVACY_SUPPORTED (0)"],
}
for name, needles in required.items():
    for needle in needles:
        if texts[name].count(needle) != 1:
            raise ValueError(f"feature/type source changed: {name}: {needle}")
for absent in ("CONFIG_BT_BLE_RPA_SUPPORTED", "CONFIG_BT_BLE_50_FEATURES_SUPPORTED",
               "CONFIG_BT_SMP_SLAVE_CON_PARAMS_UPD_ENABLE"):
    if re.search(r"^#define " + absent + r"\b", texts["sdkconfig.h"], re.M):
        raise ValueError(f"guard became enabled: {absent}")

prefix = r'''
#include <cstddef>
#include <cstdint>
using UINT8=std::uint8_t; using UINT16=std::uint16_t; using UINT32=std::uint32_t;
using BOOLEAN=bool;
using BD_ADDR=UINT8[6]; using DEV_CLASS=UINT8[3]; using LINK_KEY=UINT8[16];
using BD_FEATURES=UINT8[8]; using BT_OCTET8=UINT8[8]; using BT_OCTET16=UINT8[16];
using tBLE_ADDR_TYPE=UINT8; using tBTM_LE_KEY_TYPE=UINT8;
using tBTM_IO_CAP=UINT8; using tBTM_AUTH_REQ=UINT8; using tBT_DEVICE_TYPE=UINT8;
using tBTM_BD_NAME=UINT8[65];
typedef void tBTM_SEC_CALLBACK(void);
struct tBTM_SEC_SERV_REC;
#define TRUE 1
#define FALSE 0
#define BLE_INCLUDED TRUE
#define SMP_INCLUDED TRUE
#define BLE_PRIVACY_SPT TRUE
#define CONTROLLER_RPA_LIST_ENABLE FALSE
#define SMP_SLAVE_CON_PARAMS_UPD_ENABLE FALSE
#define BLE_50_FEATURE_SUPPORT FALSE
#define BTM_DISC_DURING_RS TRUE
#define L2CAP_UCD_INCLUDED FALSE
#define BTM_SEC_SERVICE_ARRAY_SIZE 3
#define HCI_EXT_FEATURES_PAGE_MAX 2
'''
asserts = r'''
static_assert(sizeof(void*)==4, "Xtensa 32-bit pointer ABI required");
static_assert(sizeof(BOOLEAN)==1, "Bluedroid bool ABI required");
static_assert(sizeof(tBTM_SEC_BLE_KEYS)==104, "full BLE key subrecord size");
static_assert(sizeof(tBTM_SEC_DEV_REC)==328, "complete record allocation size");
static_assert(offsetof(tBTM_SEC_DEV_REC, bd_addr)==32, "address");
static_assert(offsetof(tBTM_SEC_DEV_REC, sec_state)==150, "security state");
static_assert(offsetof(tBTM_SEC_DEV_REC, role_master)==152, "master role");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble_hci_handle)==166, "BLE handle");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble)==176, "BLE subrecord");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.key_type)==199, "BLE key mask");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.keys.pltk)==216, "PENC key location");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.keys.lltk)==248, "LENC key location");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.keys.rand)==280, "PENC random");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.keys.ediv)==288, "PENC EDIV");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.keys.key_size)==293, "LENC size");
static_assert(offsetof(tBTM_SEC_DEV_REC, ble.auth_mode)==304, "full 16-bit mode");
static_assert(sizeof(((tBTM_SEC_DEV_REC*)0)->ble.auth_mode)==2, "mode width");
'''
translation_unit = "\n".join((prefix, keys, ble, bond, conn, device, asserts))
with tempfile.TemporaryDirectory(prefix="f1-layout-", dir=ROOT / ".pio") as temp:
    source = Path(temp) / "record.cpp"
    source.write_text(translation_unit, encoding="utf-8")
    subprocess.run([str(COMPILER), "-std=gnu++17", "-Wall", "-Wextra", "-Werror",
                    "-fsyntax-only", str(source)], check=True)
print("Original full tBTM_SEC_DEV_REC declaration compiles with 17 Xtensa layout assertions; archive agreement still requires separate binary audit. No F1 runtime proof.")
