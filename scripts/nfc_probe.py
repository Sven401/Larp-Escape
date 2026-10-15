#!/usr/bin/env python3
"""Run one explicit USB command against firmware/nfc-probe; keep its evidence."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--command", choices=("INFO", "READ", "CHANGE", "OFF"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=40)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 120:
        parser.error("timeout must be between 1 and 120 seconds")
    if args.out.exists():
        parser.error("output already exists; use a new evidence filename")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    result = {"startedUtc": datetime.now(timezone.utc).isoformat(),
              "port": args.port, "command": args.command, "lines": [],
              "terminalStatus": None}
    device = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
    # Do not deliberately toggle ESP32 reset/boot pins when opening this session.
    device.dtr = False
    device.rts = False
    device.port = args.port
    try:
        device.open()
        device.reset_input_buffer()
        device.write((args.command + "\n").encode("ascii"))
        device.flush()
        deadline = time.monotonic() + args.timeout
        pending = bytearray()
        marker = "NFC END " + args.command + " "
        while time.monotonic() < deadline:
            pending.extend(device.read(256))
            if len(pending) > 8192:
                raise RuntimeError("unbounded serial line")
            while b"\n" in pending:
                raw, _, rest = pending.partition(b"\n")
                pending = bytearray(rest)
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                result["lines"].append(line)
                print(line, flush=True)
                if len(result["lines"]) > 1024:
                    raise RuntimeError("too many serial lines")
                if line.startswith(marker):
                    result["terminalStatus"] = line[len(marker):]
                    return 0 if result["terminalStatus"] in ("OK", "VERIFIED") else 3
        result["error"] = "No matching terminal response before timeout; no retry was sent."
        if args.command == "CHANGE":
            result["error"] += " Write outcome is unknown; inspect with READ before any further write."
        print(result["error"], file=sys.stderr)
        return 2
    except Exception as exc:
        result["error"] = str(exc)
        if args.command == "CHANGE":
            result["error"] += " Write outcome may be unknown."
        print(result["error"], file=sys.stderr)
        return 2
    finally:
        device.close()
        result["finishedUtc"] = datetime.now(timezone.utc).isoformat()
        with args.out.open("x", encoding="utf-8") as output:
            json.dump(result, output, indent=2, ensure_ascii=False)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
