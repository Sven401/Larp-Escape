"""Capture one named box-diagnostic interval without changing firmware or restoring flash."""

import argparse
import pathlib
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", required=True, type=int)
    parser.add_argument("--output", required=True)
    parser.add_argument("--command")
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 1800:
        parser.error("seconds must be between 1 and 1800")
    if args.command and ("\n" in args.command or "\r" in args.command):
        parser.error("command must be one line")
    target = pathlib.Path(args.output)
    if target.exists():
        parser.error("evidence file already exists: " + str(target))
    target.parent.mkdir(parents=True, exist_ok=True)

    import serial

    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    try:
        if args.reset:
            port.rts = True
            time.sleep(0.12)
            port.rts = False
        if args.command:
            port.write((args.command + "\n").encode("ascii"))
        stop = time.monotonic() + args.seconds
        raw = bytearray()
        while time.monotonic() < stop:
            raw.extend(port.read(4096))
            if len(raw) > 2 * 1024 * 1024:
                raise ValueError("capture exceeded 2 MiB")
    finally:
        port.close()
    target.write_bytes(raw)
    print("evidence={} bytes={}".format(target, len(raw)))
    print(raw.decode("utf-8", "replace")[-5000:])


if __name__ == "__main__":
    main()
