"""Independent struct/UUID wire oracle; no Kotlin implementation or device IO.

Run --check to compare the committed Goldenbytes; without arguments write them.
The firmware-grant vector is the actual C++ response checked by firmware runner
scripts/check_diagnostic_transport.py in source commit 3209275.
"""
import json
from pathlib import Path
import struct
import sys
import uuid

ROOT = Path(__file__).resolve().parent
TARGET = ROOT.parent.parent / "android/core/protocol/src/test/resources/diagnostic-control-v1.json"
DEVICE = "01000000000000000000000000000000"
RUN = "00000000-0000-4000-8000-000000000002"
GRANT = "00000000-0000-4000-8000-00000000000b"
OUTCOMES = ["ACCEPTED", "PERMISSION_DENIED", "INVALID", "BUSY", "CONSUMED", "EXPIRED", "OWNER_ERROR", "CAPACITY"]


def vector(name, command, message, device=DEVICE, boot="000000000000007B",
           run=None, grant=None, ttl=0, consent=False, result=0,
           issued=0, expires=0, reply_grant=None):
    identifier = lambda value: uuid.UUID(value).bytes if value is not None else bytes(16)
    request = (b"DG\x01" + bytes([command]) + struct.pack("<I", message)
               + bytes.fromhex(device) + struct.pack("<Q", int(boot, 16))
               + identifier(run) + identifier(grant) + struct.pack("<IB", ttl, consent))
    reply = (b"DG\x01" + bytes([command | 0x80]) + struct.pack("<I", message)
             + bytes.fromhex(device) + struct.pack("<Q", int(boot, 16))
             + identifier(run) + identifier(reply_grant if command == 2 else grant)
             + bytes([result, 3, 1, 0]) + struct.pack("<QQI", issued, expires, 60000))
    assert len(request) == 69 and len(reply) == 88
    return dict(name=name, command=[None, "CAPABILITY", "GRANT", "CANCEL"][command],
                message=message, deviceId=device, bootId=boot, runId=run,
                grantId=grant, ttlMs=ttl, explicitConsent=consent,
                outcome=OUTCOMES[result], issuedDeviceMs=issued,
                expiresDeviceMs=expires, replyGrantId=reply_grant if command == 2 else grant,
                requestHex=request.hex(), replyHex=reply.hex())


def golden():
    vectors = [vector("capability", 1, 1),
               vector("firmware-grant", 2, 1, run=RUN, ttl=10000, consent=True,
                      issued=110, expires=10110, reply_grant=GRANT),
               vector("cancel", 3, 3, run=RUN, grant=GRANT),
               vector("cancel-invalid", 3, 4, run=RUN, grant=GRANT, result=2),
               vector("unsigned-context", 1, 0xffffffff, device="F" * 32, boot="F" * 16),
               vector("last-exact-device-time", 2, 0xffffffff, boot="FEDCBA9876543210",
                      run=RUN, ttl=1, consent=True, issued=9007199254740990,
                      expires=9007199254740991, reply_grant=GRANT),
               vector("maximum-ttl", 2, 42, run=RUN, ttl=60000, consent=True,
                      issued=0, expires=60000, reply_grant=GRANT)]
    vectors += [vector("grant-" + OUTCOMES[code].lower(), 2, 100 + code,
                       run=RUN, ttl=10000, consent=True, result=code) for code in range(1, 8)]
    return (json.dumps(dict(firmwareCommit="3209275136f96d3a6a0013c485e70d692e428cf4",
                           synthetic=True, controlOnly=True, vectors=vectors),
                       indent=2, sort_keys=True) + "\n").encode("ascii")


if __name__ == "__main__":
    payload = golden()
    if sys.argv[1:] == ["--check"]:
        if TARGET.read_bytes() != payload:
            raise SystemExit("DG vectors differ from independent struct/UUID oracle")
    elif not sys.argv[1:]:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_bytes(payload)
    else:
        raise SystemExit("usage: generate_vectors.py [--check]")
    print("14 independent DG-Control vectors verified; synthetic control only.")
