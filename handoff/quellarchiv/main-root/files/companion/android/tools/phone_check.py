"""Development-only USB acceptance helper. Records only this app's UI/log tag."""
import argparse
import pathlib
import re
import subprocess
import time
import xml.etree.ElementTree as ET
import sys

sys.stdout.reconfigure(encoding="utf-8")

ADB = r"C:/Program Files (x86)/Android/android-sdk/platform-tools/adb.exe"
PACKAGE = "de.larpescape.companion"
DEFAULT_OUT = pathlib.Path(__file__).resolve().parents[1] / ".local" / "evidence"

def adb(*args):
    return subprocess.run([ADB, *args], check=True, capture_output=True).stdout

parser = argparse.ArgumentParser()
parser.add_argument("action", choices=["ui", "tap", "logs", "screen", "permission-ui", "permission-tap", "permission-screen"])
parser.add_argument("text", nargs="?", default="")
parser.add_argument("--output-dir", type=pathlib.Path, default=DEFAULT_OUT)
args = parser.parse_args()
OUT = args.output_dir.resolve()
OUT.mkdir(parents=True, exist_ok=True)
if args.action == "logs":
    text = adb("logcat", "-d", "-v", "brief", "LarpEscape:I", "*:S").decode("utf-8", "replace")
    text = re.sub(r"(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", "[address omitted]", text)
    (OUT / "phone-log.txt").write_text(text, encoding="utf-8")
    print(text)
else:
    remote = f"/sdcard/larp-native-{time.time_ns()}.xml"
    result = adb("shell", "uiautomator", "dump", remote).decode("utf-8", "replace")
    if "dumped to" not in result:
        raise RuntimeError("Fresh UI dump failed")
    try:
        root = ET.fromstring(adb("shell", "cat", remote))
        nodes = list(root.iter("node"))
    finally:
        adb("shell", "rm", remote)
    permission_dialog = args.action.startswith("permission-")
    if permission_dialog:
        controller = {"com.android.permissioncontroller", "com.google.android.permissioncontroller"}
        nodes = [n for n in nodes if n.get("package") in controller]
        context = " ".join((n.get("text", "") + " " + n.get("content-desc", "")) for n in nodes)
        if "Larp Escape" not in context or not any(t in context.lower() for t in ("geräte in der nähe", "nearby devices")):
            raise RuntimeError("Expected Companion Nearby Devices permission dialog")
    elif not any(n.get("package") == PACKAGE for n in nodes):
        raise RuntimeError("Companion app is not foreground")
    if args.action in ("screen", "permission-screen"):
        (OUT / "phone.png").write_bytes(adb("exec-out", "screencap", "-p"))
    elif args.action in ("tap", "permission-tap"):
        matches = [n for n in nodes if args.text == n.get("text") and n.get("clickable") == "true"]
        if not matches:
            # Compose may place button text inside a clickable parent.
            def find_parent(node):
                for child in node:
                    if child.get("text") == args.text and node.get("clickable") == "true":
                        matches.append(node)
                    find_parent(child)
            if not permission_dialog:
                find_parent(root)
        if len(matches) != 1:
            raise RuntimeError(f"Expected one clickable match; found {len(matches)}")
        bounds = list(map(int, re.findall(r"\d+", matches[0].get("bounds"))))
        adb("shell", "input", "tap", str((bounds[0] + bounds[2]) // 2), str((bounds[1] + bounds[3]) // 2))
        print("Tapped", args.text)
    else:
        text = "\n".join(n.get("text") for n in nodes if n.get("text"))
        (OUT / "phone-ui.txt").write_text(text, encoding="utf-8")
        print(text)
