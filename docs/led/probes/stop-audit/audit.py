"""Offline diagnostic of recorded NearGlow stop windows; never grants acceptance.

Uses the original CAL masks and original assessment frame selections unchanged.
Peripheral samples are exploratory, potentially illuminated paper, NOT a dark
control and NOT a replacement baseline. No device access or time fitting.
"""
import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence
    cal_path = root / "paper-calibration-e8.json"
    roi_path = root / "paper-rois-e8.json"
    cal, roi = read(cal_path), read(roi_path)
    assert cal["mapping_status"] == "verified"
    assert digest(roi_path) == cal["roi_layout_sha256"]
    gold_path = root / "paper-gold-e8-complete/gold-analysis.json"
    gold = read(gold_path)
    maxima = np.array([led["gold_maximum_delta_mean_rgb"] for led in gold["leds"]])
    masks, edges, overlaps = [], [], []
    yy, xx = np.indices((400, 400))
    for led in cal["leds"]:
        offsets = np.array(led["offsets"], dtype=int)
        x, y = led["x"] + offsets[:, 0], led["y"] + offsets[:, 1]
        masks.append((y, x))
        # Map CAL tile back to its acquisition aperture; never use zero padding.
        tile = (led["y"] // 80) * roi["columns"] + led["x"] // 80
        region = roi["regions"][tile]
        cx, cy = (led["x"] // 80) * 80 + 40, (led["y"] // 80) * 80 + 40
        radius = region["aperture_radius"]
        distance = (xx - cx) ** 2 + (yy - cy) ** 2
        edge = (distance >= (.70 * radius) ** 2) & (distance <= (.90 * radius) ** 2)
        core = np.zeros((400, 400), dtype=np.uint8)
        core[y, x] = 1
        edge &= core == 0
        ey, ex = np.nonzero(edge)
        assert len(ey) >= 10
        overlaps.append(float(core[ey, ex].mean()))
        edges.append((ey, ex))

    report = {"schema": 1, "source_sha256": digest(Path(__file__)),
              "calibration_sha256": digest(cal_path), "roi_sha256": digest(roi_path),
              "gold_analysis_sha256": digest(gold_path),
              "method": "Original masks/windows/5% threshold unchanged; exploratory aperture edge comparison only",
              "acceptance": "not_granted", "runs": {}}
    for name in ("walk02", "outage01"):
        directory = root / name
        assessment = read(directory / "walk-analysis.json")
        capture, camera = read(directory / "capture.json"), read(directory / "camera.json")
        assert digest(directory / "camera.json") == capture["camera_manifest_sha256"]
        assert digest(cal_path) == assessment["evidence"]["calibration_sha256"]
        assert digest(directory / "capture.json") == assessment["evidence"]["capture_sha256"]
        assert digest(directory / "camera.json") == assessment["evidence"]["camera_sha256"]
        windows = assessment["camera"]["targeted_stop_dark_checks"]
        baseline_ids = assessment["camera"]["baseline_frames"]
        selected = sorted(set(baseline_ids + [i for w in windows for i in w["frames"]]))
        values, peripheral = {}, {}
        for index in selected:
            entry = camera["frames"][index]
            path = directory / entry["file"]
            assert digest(path) == entry["sha256"], str(path)
            im = cv2.imread(str(path))
            assert im is not None and im.shape == (400, 400, 3)
            im = im[:, :, ::-1]
            values[index] = np.array([im[y, x].mean(axis=0) for y, x in masks])
            peripheral[index] = np.array([im[y, x].mean(axis=0) for y, x in edges])
        base = np.mean([values[i] for i in baseline_ids], axis=0)
        edge_base = np.mean([peripheral[i] for i in baseline_ids], axis=0)
        assert np.allclose(base, assessment["camera"]["baseline_mean_rgb_per_led"], atol=1e-10)
        result = {"capture_sha256": digest(directory / "capture.json"),
                  "camera_sha256": digest(directory / "camera.json"),
                  "original_analysis_sha256": digest(directory / "walk-analysis.json"),
                  "verified_selected_images": len(selected), "stops": []}
        for window in windows:
            ids = window["frames"]
            core = np.mean([values[i] for i in ids], axis=0) - base
            edge = np.mean([peripheral[i] for i in ids], axis=0) - edge_base
            residual = core.mean(axis=1)
            assert np.allclose(residual, window["residual_mean_rgb_per_led"], atol=1e-10)
            passing = np.maximum(0, residual) <= .05 * maxima
            assert passing.tolist() == window["below_five_percent_gold_per_led"]
            low, high = camera["frames"][ids[0]]["t_ms"], camera["frames"][ids[-1]]["t_ms"]
            tx = [e for e in capture["events"] if low <= e["host_ms"] <= high]
            assert tx and all(e["pixels"] == [[0, 0, 0]] * 23 for e in tx)
            correlation = float(np.corrcoef(residual, edge.mean(axis=1))[0, 1])
            result["stops"].append({"req": window["req"], "camera_frames": len(ids),
                "first_camera_host_ms": low, "last_camera_host_ms": high,
                "black_TX_events_in_same_host_window": len(tx),
                "original_pass_count": int(passing.sum()),
                "original_failed_leds": np.flatnonzero(~passing).tolist(),
                "core_delta_RGB_mean": core.mean(axis=0).tolist(),
                "edge_delta_RGB_mean": edge.mean(axis=0).tolist(),
                "core_edge_spatial_correlation": correlation,
                "core_delta_RGB_per_led": core.tolist(),
                "edge_delta_RGB_per_led": edge.tolist(),
                "peripheral_sampling_pixels": [len(y) for y, x in edges],
                "peripheral_overlap_with_CAL_mask_fraction": overlaps})
        report["runs"][name] = result
    # Retain the successful earlier stop as a bounded comparison, not a waiver.
    report["gold_reference"] = {
        "original_status": gold["status"],
        "checks_per_led": [led["checks"] for led in gold["leds"]],
        "limits": "Earlier separate run; does not prove the later environment or optics"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    print("Audit complete; original failures preserved")
    for name, run in report["runs"].items():
        for stop in run["stops"]:
            print(name, {k: v for k, v in stop.items() if not k.endswith("per_led") and k != "peripheral_sampling_pixels"})


if __name__ == "__main__":
    main()
