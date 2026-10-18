"""Check the built Dev/Spielleitung APKs and their merged manifests; never install."""
from pathlib import Path
import argparse
import hashlib
import os
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "companion/android"
ATTR = "{http://schemas.android.com/apk/res/android}"
DEV = "de.larpescape.companion"
GAME = "de.larpescape.gamemaster"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def authorities(manifest):
    return {value for node in manifest.findall("application/provider")
            for value in node.get(ATTR + "authorities", "").split(";") if value}


def validate_pair(dev, game):
    for manifest, package in [(dev, DEV), (game, GAME)]:
        require(manifest.get("package") == package, "Unexpected application ID: " + package)
        require(manifest.get(ATTR + "sharedUserId") is None, "Shared UID is forbidden")
        app = manifest.find("application")
        require(app is not None and app.get(ATTR + "allowBackup") == "false", "Backup must stay disabled")
        require(app.get(ATTR + "icon") is not None, "Distinct launcher icon required")
        require(any(node.get(ATTR + "name") == package + ".MainActivity"
                    for node in app.findall("activity")), "Missing independent entry point")
    require(not (authorities(dev) & authorities(game)), "Provider authority collision")
    require(any(node.get(ATTR + "name") == DEV + ".platform.nfc.PhoneNfcTestService"
                for node in dev.findall("application/service")), "Dev NFC test service was removed")
    require(not any(DEV in str(node.attrib) for node in game.iter()), "Dev component leaked into Spielleitung")
    permissions = {node.get(ATTR + "name", "") for node in game.findall("uses-permission")}
    allowed = {"android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT"}
    require(allowed <= permissions, "Required product Bluetooth permission missing")
    require(not any(p.startswith("android.permission.") and p not in allowed for p in permissions),
            "Unexpected product hardware/network permission")
    require(not any(node.get(ATTR + "name") == "android.nfc.cardemulation.action.HOST_APDU_SERVICE"
                    for node in game.iter("action")), "HCE test service leaked into Spielleitung")


def run(args):
    return subprocess.check_output([str(arg) for arg in args], text=True, encoding="utf-8", errors="replace")


def identity(aapt, apk):
    output = run([aapt, "dump", "badging", apk])
    match = re.search(r"package: name='([^']+)' versionCode='([0-9]+)'", output)
    require(match is not None, "Missing APK identity: " + str(apk))
    return match[1], int(match[2]), output


def certificate(sdk_tools, apk):
    java = Path(os.environ["JAVA_HOME"]) / "bin" / ("java.exe" if os.name == "nt" else "java")
    output = run([java, "-jar", sdk_tools / "lib/apksigner.jar", "verify", "--print-certs", apk])
    digests = re.findall(r"Signer #[0-9]+ certificate SHA-256 digest: ([0-9a-f]+)", output)
    require(bool(digests), "Missing verified signing certificate")
    return sorted(digests)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dev-baseline", type=Path, help="Optional known Dev APK; verify update ID, version and signer")
    args = parser.parse_args()
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    require(bool(sdk), "Set ANDROID_HOME")
    sdk_tools = Path(sdk) / "build-tools/35.0.0"
    aapt = sdk_tools / ("aapt2.exe" if os.name == "nt" else "aapt2")
    for variant in ("debug", "release"):
        manifests = []
        for module, package, label in [("app", DEV, "Larp Dev"), ("gamemaster", GAME, "Larp GM")]:
            folder = ANDROID / module / "build"
            candidates = list((folder / "intermediates/merged_manifests" / variant).rglob("AndroidManifest.xml"))
            require(len(candidates) == 1, "Build one merged manifest first: " + module + "/" + variant)
            manifests.append(ET.parse(candidates[0]).getroot())
            suffix = "-unsigned" if variant == "release" else ""
            apk = folder / "outputs/apk" / variant / f"{module}-{variant}{suffix}.apk"
            actual, version, badging = identity(aapt, apk)
            require(actual == package, "Wrong packaged application ID")
            require(f"application-label:'{label}'" in badging, "Wrong launcher label")
            require("minSdkVersion:'33'" in badging and "targetSdkVersion:'35'" in badging, "SDK drift")
            require(version >= (2 if module == "app" else 1), "Version code regressed")
            if module == "gamemaster":
                with zipfile.ZipFile(apk) as archive:
                    for name in archive.namelist():
                        if name.endswith(".dex"):
                            require(b"Lde/larpescape/companion/" not in archive.read(name), "Dev code packaged in Spielleitung")
            if variant == "debug":
                certificate(sdk_tools, apk)
            print(f"OK {module}/{variant}: {actual} v{version}, SHA256 {hashlib.sha256(apk.read_bytes()).hexdigest()}")
        validate_pair(*manifests)
    if args.dev_baseline:
        current = ANDROID / "app/build/outputs/apk/debug/app-debug.apk"
        old_id, old_version, _ = identity(aapt, args.dev_baseline)
        new_id, new_version, _ = identity(aapt, current)
        require(old_id == new_id == DEV, "Baseline is not the retained Dev application")
        require(new_version >= old_version, "Dev update would downgrade")
        require(certificate(sdk_tools, current) == certificate(sdk_tools, args.dev_baseline), "Dev signing certificate differs; do not uninstall to bypass")
        print("OK Dev baseline: same application ID and signer, version does not decrease. Not a device/data-retention test.")
    print("OK separate APKs/manifests; no installation or device-state claim")


if __name__ == "__main__":
    main()
