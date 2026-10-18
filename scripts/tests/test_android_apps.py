"""Negative cases for the built-app separation guard."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_android_apps import ATTR, DEV, GAME, validate_pair


def manifest(package):
    return ET.fromstring(f'''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="{package}">
    <application android:allowBackup="false" android:icon="@mipmap/ic_launcher">
      <activity android:name="{package}.MainActivity" />
      <provider android:name="androidx.startup.InitializationProvider" android:authorities="{package}.androidx-startup" />
    </application></manifest>''')


class AndroidAppSeparationTest(unittest.TestCase):
    def setUp(self):
        self.dev, self.game = manifest(DEV), manifest(GAME)
        ET.SubElement(self.dev.find("application"), "service", {ATTR + "name": DEV + ".platform.nfc.PhoneNfcTestService"})

    def test_independent_apps(self):
        validate_pair(self.dev, self.game)

    def test_changed_dev_identity_rejected(self):
        self.dev.set("package", DEV + ".dev")
        with self.assertRaisesRegex(ValueError, "application ID"):
            validate_pair(self.dev, self.game)

    def test_shared_uid_rejected(self):
        self.game.set(ATTR + "sharedUserId", DEV)
        with self.assertRaisesRegex(ValueError, "Shared UID"):
            validate_pair(self.dev, self.game)

    def test_provider_collision_rejected(self):
        self.game.find("application/provider").set(ATTR + "authorities", DEV + ".androidx-startup")
        with self.assertRaisesRegex(ValueError, "authority collision"):
            validate_pair(self.dev, self.game)

    def test_dev_service_loss_rejected(self):
        self.dev.find("application").remove(self.dev.find("application/service"))
        with self.assertRaisesRegex(ValueError, "removed"):
            validate_pair(self.dev, self.game)

    def test_dev_component_leak_rejected(self):
        ET.SubElement(self.game.find("application"), "service", {ATTR + "name": DEV + ".platform.nfc.PhoneNfcTestService"})
        with self.assertRaisesRegex(ValueError, "leaked"):
            validate_pair(self.dev, self.game)

    def test_unnecessary_permission_rejected(self):
        ET.SubElement(self.game, "uses-permission", {ATTR + "name": "android.permission.NFC"})
        with self.assertRaisesRegex(ValueError, "hardware/network"):
            validate_pair(self.dev, self.game)

    def test_backup_change_rejected(self):
        self.dev.find("application").set(ATTR + "allowBackup", "true")
        with self.assertRaisesRegex(ValueError, "Backup"):
            validate_pair(self.dev, self.game)


if __name__ == "__main__":
    unittest.main()
