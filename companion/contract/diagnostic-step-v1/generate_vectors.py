"""Independent Python struct/UUID oracle for b5a8c430; wire from 04c145f, no device IO."""
import json
from pathlib import Path
import struct
import sys
import uuid

TARGET = Path(__file__).resolve().parents[2] / "android/core/protocol/src/test/resources/diagnostic-step-v1.json"
COMMIT = "b5a8c430fcf5cf37731c18ba3ac261fce3cc13f7"
DEVICE = "01000000000000000000000000000000"
RUN = "00000000-0000-4000-8000-000000000001"
GRANT = "00000000-0000-4000-8000-00000000000a"


def vector(name, operation=0, started=100, ended=200, outcome=0, reason=0,
           high=True, off=False, renders=0, overflow=False, edges=(),
           message=17, step=1, boot="000000000000007B", timeout=1000, json_accept=True):
    request = (b"DG\x01\x04" + struct.pack("<I", message) + bytes.fromhex(DEVICE)
               + struct.pack("<Q", int(boot, 16)) + uuid.UUID(RUN).bytes + uuid.UUID(GRANT).bytes
               + struct.pack("<II", step, timeout)
               + (bytes([0, 1, 0, 0, 0, 0, 0, 0]) if operation == 0
                  else bytes([1, 0, 0, 16, 0, 0]) + struct.pack("<H", 500)))
    body = (request + struct.pack("<QQ", started, ended)
            + bytes([outcome, reason, high, off, renders, overflow, len(edges), 1])
            + b"".join(struct.pack("<QBB", time, channel, raw | ((raw == high) << 1))
                       for time, channel, raw in edges))
    frames = {}
    for capacity in (13, 20, 127, 128):
        payload = capacity - 12
        frames[str(capacity)] = [(b"DG\x01\x84" + struct.pack("<IHH", message, len(body), at)
                                 + body[at:at + payload]).hex() for at in range(0, len(body), payload)]
    return dict(name=name, operation=["WAKE_CAPTURE", "LED_SINGLE"][operation], message=message,
                deviceId=DEVICE, bootId=boot, runId=RUN, grantId=GRANT, stepId=step, timeoutMs=timeout,
                startedDeviceMs=started, endedDeviceMs=ended, outcome=outcome, reason=reason,
                activeHigh=high, restoredOff=off, renderCalls=renders, overflow=overflow,
                edgeCount=len(edges), wireAccept=json_accept, jsonAccept=json_accept, requestHex=request.hex(), replyHex=body.hex(), frames=frames)


def golden():
    full = [(110 + i, 1 + i % 4, i % 2 == 0) for i in range(64)]
    vectors = [vector("firmware-wake-max64", edges=full),
               vector("firmware-led", 1, ended=600, off=True, renders=2),
               vector("wake-overflow", edges=full, overflow=True),
               vector("wake-active-low", high=False, edges=[(110, 1, False), (120, 1, True)]),
               vector("led-failed", 1, ended=600, outcome=1, reason=1),
               vector("led-cancelled", 1, ended=200, outcome=2, reason=6, off=True, renders=1),
               vector("wake-rejected", outcome=3, reason=3, ended=100),
               vector("last-exact-time", 1, started=9007199254740491, ended=9007199254740991,
                      off=True, renders=2, message=0xffffffff, step=0xffffffff, boot="FFFFFFFFFFFFFFFF"),
               vector("wire-duration-equals-timeout", ended=1100, edges=[(110, 1, True), (120, 1, False)], json_accept=False),
               vector("wire-led-short-hold", 1, ended=599, off=True, renders=2, json_accept=False)]
    return (json.dumps(dict(firmwareCommit=COMMIT, wireOriginCommit="04c145f6775b98761a8b7b018020566f9f3fa70b", synthetic=True, supplyMillivolts=None,
                           vectors=vectors), indent=2, sort_keys=True) + "\n").encode("ascii")


if __name__ == "__main__":
    payload = golden()
    if sys.argv[1:] == ["--check"]:
        if TARGET.read_bytes() != payload:
            raise SystemExit("DG Step vectors differ from independent struct/UUID oracle")
    elif not sys.argv[1:]:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_bytes(payload)
    else:
        raise SystemExit("usage: generate_vectors.py [--check]")
    print("10 independent DG-Step vectors with 13/20/127/128-byte segments verified; synthetic only.")
