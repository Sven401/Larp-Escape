"""Capture box startup evidence; restore a saved flash only on explicit request."""

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import time


def inspect_log(raw):
    text = raw.replace("\r", "")
    required = (
        "MCP23XXX found at address 32",
        "MCP23XXX found at address 33",
        "NFC firmware:",
        "Runtime ready",
    )
    missing = [item for item in required if item not in text]
    reports = [int(value) for value in re.findall(r"Runtime max loop us: (\d+)", text)]
    led_stack = [int(value) for value in re.findall(r"LED min free stack bytes: (\d+)", text)]
    led_render = [int(value) for value in re.findall(r"LED max render us: (\d+)", text)]
    led_missed = [int(value) for value in re.findall(r"LED missed frame deadlines: (\d+)", text)]
    led_drops = [int(value) for value in re.findall(r"LED command drops: (\d+)", text)]
    radio_drops = [int(value) for value in re.findall(r"ESP-NOW status drops: (\d+)", text)]
    boot_count = text.count("rst:0x")
    faults = [
        marker
        for marker in ("Guru Meditation", "Brownout detector", "Failed to initialize", "abort()")
        if marker in text
    ]
    failures = []
    if missing:
        failures.append("missing startup markers: " + ", ".join(missing))
    if boot_count != 1:
        failures.append("expected one boot, observed {}".format(boot_count))
    if len(reports) < 3:
        failures.append("expected at least three runtime reports")
    for label, values in (
        ("LED stack", led_stack),
        ("LED render time", led_render),
        ("LED missed deadlines", led_missed),
        ("LED command drops", led_drops),
        ("ESP-NOW status drops", radio_drops),
    ):
        if len(values) != len(reports):
            failures.append("incomplete {} metrics".format(label))
    if faults:
        failures.append("fault markers: " + ", ".join(faults))
    for label, values in (
        ("LED missed deadlines", led_missed),
        ("LED command drops", led_drops),
        ("ESP-NOW status drops", radio_drops),
    ):
        if values and values[-1] != 0:
            failures.append("{} = {}".format(label, values[-1]))
    if led_stack and min(led_stack) <= 0:
        failures.append("no positive LED stack reserve")
    print("boot_count={} runtime_reports={} result={}".format(
        boot_count, len(reports), "FAIL" if failures else "PASS"
    ))
    if reports:
        print("max_loop_us={} min_led_stack_bytes={} max_led_render_us={}".format(
            max(reports), min(led_stack) if led_stack else "unknown",
            max(led_render) if led_render else "unknown",
        ))
    if led_missed and led_drops and radio_drops:
        print("led_missed={} led_drops={} radio_drops={}".format(
            led_missed[-1], led_drops[-1], radio_drops[-1]
        ))
    if "GENERIC fallback clockless controller" in text:
        print("warning: FastLED uses the generic clockless fallback")
    for failure in failures:
        print("failure: " + failure, file=sys.stderr)
    return 1 if failures else 0


def capture(args):
    import serial

    target = pathlib.Path(args.output)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        raise ValueError("evidence file already exists: " + str(target))
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    try:
        port.rts = True
        time.sleep(0.12)
        port.rts = False
        end = time.monotonic() + args.seconds
        data = bytearray()
        while time.monotonic() < end:
            data.extend(port.read(min(4096, port.in_waiting or 1)))
            if len(data) > 512 * 1024:
                raise ValueError("serial capture exceeded 512 KiB")
    finally:
        port.close()
    target.write_bytes(data)
    print("evidence={} bytes={}".format(target, len(data)))
    return inspect_log(data.decode("utf-8", "replace"))


def restore_original(args):
    if not args.confirm_restore_original:
        raise ValueError("restore requires --confirm-restore-original")
    manifest_path = pathlib.Path(args.manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    backup_name = manifest["backup_file"]
    if pathlib.Path(backup_name).name != backup_name:
        raise ValueError("backup_file must be a name beside the manifest")
    backup = manifest_path.parent / backup_name
    if backup.stat().st_size != manifest["flash_bytes"]:
        raise ValueError("backup size differs from manifest")
    digest = hashlib.sha256(backup.read_bytes()).hexdigest()
    if digest.lower() != manifest["flash_sha256"].lower():
        raise ValueError("backup hash differs from manifest")
    command = [args.python, args.esptool, "--chip", "esp32", "--port", args.port]
    identity = subprocess.run(command + ["flash_id"], check=True, capture_output=True, text=True).stdout
    mac = re.search(r"MAC: ((?:[0-9a-f]{2}:){5}[0-9a-f]{2})", identity, re.I)
    size = re.search(r"Detected flash size: (\d+)MB", identity)
    if mac is None or mac.group(1).lower() != manifest["chip_mac"].lower():
        raise ValueError("connected chip differs from backup manifest")
    if size is None or int(size.group(1)) * 1024 * 1024 != manifest["flash_bytes"]:
        raise ValueError("connected flash size differs from backup manifest")
    print("Explicit restore requested; backup size, hash and device identity verified.", flush=True)
    subprocess.run(command + ["--baud", "460800", "write_flash", "0x0", str(backup)], check=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    observed = commands.add_parser("capture", help="reset and capture current firmware; never restore")
    observed.add_argument("--port", required=True)
    observed.add_argument("--seconds", type=int, default=45)
    observed.add_argument("--output", required=True)
    parsed = commands.add_parser("inspect", help="evaluate a saved serial log offline")
    parsed.add_argument("log")
    restore = commands.add_parser("restore-original", help="explicitly overwrite flash with the saved original")
    restore.add_argument("--port", required=True)
    restore.add_argument("--manifest", required=True)
    restore.add_argument("--python", default=sys.executable)
    restore.add_argument("--esptool", required=True)
    restore.add_argument("--confirm-restore-original", action="store_true")
    args = parser.parse_args()
    if args.action == "capture":
        if args.seconds < 30 or args.seconds > 3600:
            raise ValueError("capture duration must be 30 to 3600 seconds")
        return capture(args)
    if args.action == "inspect":
        return inspect_log(pathlib.Path(args.log).read_text(encoding="utf-8", errors="replace"))
    return restore_original(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("error: " + str(error), file=sys.stderr)
        sys.exit(2)
