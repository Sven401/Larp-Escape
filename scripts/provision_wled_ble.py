"""Export this box's LED access over trusted USB to a new private JSON file.

Install pyserial. This does not flash firmware or change the box's stored key.
Import the resulting file through the companion app's Box-Zugang action, then
remove unneeded transport copies. Never commit the file or paste its contents.
"""
import argparse
import json
import os
from pathlib import Path
import re
import time

ROOT = Path(__file__).resolve().parents[1]


def validated_access(line):
    try:
        value = json.loads(line)
    except (UnicodeError, ValueError):
        return None
    if not isinstance(value, dict) or type(value.get("schemaVersion")) is not int or value["schemaVersion"] != 1:
        return None
    for key, size in (("deviceId", 32), ("secret", 64)):
        text = value.get(key)
        if not isinstance(text, str) or not re.fullmatch(r"[0-9a-f]{%d}" % size, text) or not int(text, 16):
            return None
    return {"schemaVersion": 1, "deviceId": value["deviceId"],
            "secret": value["secret"], "name": "Larp WLED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Confirmed box USB port, e.g. COM3")
    parser.add_argument("--output", required=True, type=Path, help="New file outside the repository")
    args = parser.parse_args()
    destination = args.output.expanduser().resolve()
    if destination.is_relative_to(ROOT) or destination.exists() or not destination.parent.is_dir():
        parser.error("Use a new file in an existing private directory outside this repository.")
    try:
        import serial
        # Set DTR/RTS before opening; no deliberate reset or bootloader entry.
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.5, write_timeout=2)
        connection.dtr = False
        connection.rts = False
        connection.port = args.port
        with connection:
            connection.reset_input_buffer()
            connection.write(b"LARP-PROVISION-V1\n")
            connection.flush()
            until = time.monotonic() + 10
            access = None
            while time.monotonic() < until:
                line = connection.readline(1025)
                if len(line) > 1024 or not line.endswith(b"\n"):
                    continue
                access = validated_access(line)
                if access is not None:
                    break
            if access is None:
                raise RuntimeError("No valid provisioning response within ten seconds")
        # Exclusive creation prevents accidental replacement. POSIX mode 0600;
        # Windows uses the chosen private directory's inherited user ACL.
        descriptor = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(access, stream, separators=(",", ":"))
            stream.write("\n")
        print(f"Box access saved to {destination}. Keep private; import in the companion app.")
    except ImportError:
        parser.exit(1, "pyserial is required: python -m pip install pyserial==3.5\n")
    except Exception:
        # Serial errors can contain received data. Never echo raw exceptions or JSON.
        parser.exit(1, "Provisioning failed. Check the box port, product firmware and private output path.\n")


if __name__ == "__main__":
    main()
