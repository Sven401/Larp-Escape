"""Check the pinned SDK encryption result's attribution limit.

Pass the unmodified official ESP-IDF v4.4.6 btu_hcif.c and btm_sec.c. This
proves a negative protocol fact: the Encryption Change payload has no key,
command identifier or GATT-birth token. It is not a live security probe.
"""
import argparse
import hashlib
from pathlib import Path
import re

PINS = {
    "hci": "e4485a0f4a035119827050596c0ac707158ebafc3946afecd22725efc8e1d81f",
    "security": "d3207f4b4a369cf534f128c124fca8192979769a01986ed46bcb15ace4c8b394",
}


def checked(path, pin):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != pin:
        raise ValueError("official source changed: " + str(path))
    return data.decode("utf-8")


def function(source, signature):
    matches = list(re.finditer(signature + r"\s*\{", source, re.MULTILINE))
    if len(matches) != 1:
        raise ValueError("original function missing or ambiguous: " + signature)
    depth, end = 1, matches[0].end()
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError("unbalanced original function")
    return source[matches[0].start():end]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--hci-source", type=Path, required=True)
parser.add_argument("--security-source", type=Path, required=True)
args = parser.parse_args()
hci = checked(args.hci_source, PINS["hci"])
security = checked(args.security_source, PINS["security"])
event = function(hci, r"static void btu_hcif_encryption_change_evt\s*\(UINT8 \*p\)")
reads = re.findall(r"STREAM_TO_(UINT8|UINT16)\s*\(\s*(\w+)\s*,\s*p\s*\)", event)
if reads != [("UINT8", "status"), ("UINT16", "handle"), ("UINT8", "encr_enable")]:
    raise ValueError("encryption result payload changed")
if not re.search(r"btm_acl_encrypt_change\s*\(handle, status, encr_enable\);\s*"
                 r"btm_sec_encrypt_change\s*\(handle, status, encr_enable\);", event):
    raise ValueError("original encryption result forwarding changed")
security_event = function(security, r"void btm_sec_encrypt_change\s*\(UINT16 handle, UINT8 status, UINT8 encr_enable\)")
for part in ("btm_find_dev_by_handle (handle)",
             "(status == HCI_SUCCESS) && encr_enable",
             "p_acl->transport == BT_TRANSPORT_LE",
             "btm_ble_link_encrypted(p_dev_rec->ble.pseudo_addr, encr_enable)"):
    if part not in security_event:
        raise ValueError("security result path changed: " + part)

# Two distinct histories collapse to exactly the same original event bytes.
# The product must reject a reused handle and any second key selection unless
# the source actor proves an exclusive command/result lifetime.
old_birth_result = (0, 0x123, 1)
new_birth_result = (0, 0x123, 1)
other_key_result = (0, 0x123, 1)
assert old_birth_result == new_birth_result == other_key_result
print("Pinned SDK Encryption Change reads only status/handle/enabled; recycled-birth and second-key histories are indistinguishable from this event. No controller-key or GATT-birth witness follows.")
