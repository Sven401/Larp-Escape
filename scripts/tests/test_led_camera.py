"""Reject invalid mapping/evidence and verify extraction with synthetic optical input."""
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "led_camera.py"
spec = importlib.util.spec_from_file_location("led_camera", SCRIPT)
camera = importlib.util.module_from_spec(spec)
spec.loader.exec_module(camera)


class CameraTests(unittest.TestCase):
    def test_compare_detects_inversion_and_refuses_dark_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            reference = root / "reference.csv"
            samples = root / "samples.csv"
            for path, mode in ((reference, "reference"), (samples, "measured")):
                with path.open("w", newline="") as stream:
                    writer = csv.writer(stream)
                    writer.writerow(["t_ms", "led", "red", "green", "blue"])
                    for t in range(20):
                        for led in range(23):
                            value = t if mode == "reference" else 20-t if led == 0 else 0 if led == 1 else t
                            writer.writerow([t*33, led, value, value, value])
            args = SimpleNamespace(samples=samples, reference=reference, offset_ms=0,
                                   horizon_ms=627, minimum_range=5, output=root / "report.json")
            camera.compare(args)
            report = json.loads(args.output.read_text())
            self.assertAlmostEqual(report["leds"][0]["temporal_correlation"], -1)
            self.assertIsNone(report["leds"][1]["temporal_correlation"])
            self.assertAlmostEqual(report["leds"][2]["temporal_correlation"], 1)
            self.assertEqual(report["verdict"], "unassessed")
            args.output = root / "no-overlap.json"
            args.offset_ms = 9000
            with self.assertRaises(ValueError):
                camera.compare(args)

    def calibration(self):
        return {"schema": 1, "mapping_status": "verified", "image_size": [230, 20], "leds": [
            {"index": i, "x": 5 + i * 10, "y": 10, "radius": 2} for i in range(23)]}

    def test_mapping_rejects_wrong_size_order_and_bounds(self):
        for kind in ("size", "order", "bounds", "unverified"):
            config = self.calibration()
            if kind == "size": config["image_size"] = [231, 20]
            if kind == "order": config["leds"][0]["index"] = 1
            if kind == "bounds": config["leds"][0]["x"] = 0
            if kind == "unverified": config["mapping_status"] = "unverified"
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                camera.regions(config, 230, 20)

    def test_rgb_clipping_and_tamper_rejection(self):
        import cv2
        import numpy as np
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            image = np.zeros((20, 230, 3), dtype=np.uint8)
            image[:, :] = (20, 40, 255)
            path = root / "frame.png"
            self.assertTrue(cv2.imwrite(str(path), image))
            capture = {"status": "recorded", "frames": [{"file": "frame.png", "t_ms": 42,
                "width": 230, "height": 20, "sha256": camera.digest(path)}]}
            (root / "capture.json").write_text(json.dumps(capture))
            config = root / "calibration.json"
            config.write_text(json.dumps(self.calibration()))
            args = SimpleNamespace(capture=root, calibration=config, output=root / "samples.csv")
            camera.extract(args)
            with args.output.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 23)
            self.assertEqual([float(rows[0][k]) for k in ("red", "green", "blue", "clipped_fraction")],
                             [255, 40, 20, 1])
            path.write_bytes(b"tampered")
            args.output = root / "tampered.csv"
            with self.assertRaisesRegex(ValueError, "Changed frame"):
                camera.extract(args)


if __name__ == "__main__":
    unittest.main()
