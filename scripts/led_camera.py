"""Record USB-camera evidence and extract ordered LED regions; never drive firmware."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(args):
    import cv2

    args.output.mkdir(parents=True, exist_ok=False)
    backend = {"dshow": cv2.CAP_DSHOW, "msmf": cv2.CAP_MSMF,
               "any": cv2.CAP_ANY}[args.backend]
    cap = cv2.VideoCapture(args.camera, backend)
    metadata = {
        "schema": 1, "utc": datetime.now(timezone.utc).isoformat(),
        "camera_index": args.camera, "backend_requested": args.backend,
        "opencv": cv2.__version__, "status": "incomplete",
        "timestamp_meaning": "host monotonic time immediately after read; not exposure time",
        "settings": {}, "frames": [],
    }
    root = Path(__file__).resolve().parents[1]
    metadata["source_head"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    metadata["source_diff"] = subprocess.check_output(
        ["git", "status", "--porcelain"], cwd=root, text=True)
    try:
        if not cap.isOpened():
            raise RuntimeError("Camera unavailable; check process permissions and competing apps")
        settings = [("width", cv2.CAP_PROP_FRAME_WIDTH, args.width),
                    ("height", cv2.CAP_PROP_FRAME_HEIGHT, args.height),
                    ("fps", cv2.CAP_PROP_FPS, args.fps),
                    ("auto_exposure", cv2.CAP_PROP_AUTO_EXPOSURE, args.auto_exposure),
                    ("exposure", cv2.CAP_PROP_EXPOSURE, args.exposure),
                    ("auto_wb", cv2.CAP_PROP_AUTO_WB, args.auto_wb)]
        for name, prop, requested in settings:
            accepted = cap.set(prop, requested) if requested is not None else None
            metadata["settings"][name] = {
                "requested": requested, "driver_accepted": accepted, "readback": cap.get(prop)}
        for _ in range(args.warmup):
            ok, _ = cap.read()
            if not ok:
                raise RuntimeError("Camera stopped during warmup")
        start = time.perf_counter()
        while time.perf_counter() - start < args.seconds:
            before = time.perf_counter()
            ok, frame = cap.read()
            received = time.perf_counter()
            if not ok:
                raise RuntimeError("Camera read failed; incomplete evidence")
            name = f"frame-{len(metadata['frames']):06d}.png"
            path = args.output / name
            if not cv2.imwrite(str(path), frame):
                raise RuntimeError("PNG write failed")
            metadata["frames"].append({
                "file": name, "t_ms": (received - start) * 1000,
                "read_ms": (received - before) * 1000,
                "sha256": digest(path), "width": frame.shape[1], "height": frame.shape[0]})
        if len(metadata["frames"]) < 2:
            raise RuntimeError("Too few frames")
        gaps = [b["t_ms"] - a["t_ms"] for a, b in
                zip(metadata["frames"], metadata["frames"][1:])]
        metadata["timing"] = {"received_fps": 1000 / statistics.mean(gaps),
                              "max_gap_ms": max(gaps), "median_gap_ms": statistics.median(gaps)}
        metadata["status"] = "recorded"
    except Exception as error:
        metadata["error"] = str(error)
        raise
    finally:
        cap.release()
        (args.output / "capture.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "frames": len(metadata["frames"]),
                      "timing": metadata["timing"]}))


def regions(config, width, height):
    if config.get("mapping_status") != "verified":
        raise ValueError("LED-index mapping must be independently verified")
    if config.get("schema") not in (1, 2) or config.get("image_size") != [width, height]:
        raise ValueError("Calibration schema/image dimensions do not match")
    leds = config.get("leds", [])
    if [led.get("index") for led in leds] != list(range(23)):
        raise ValueError("Need exactly 23 explicitly ordered LED regions, indices 0..22")
    result = []
    for led in leds:
        x, y, radius = (led[key] for key in ("x", "y", "radius"))
        if any(type(v) is not int for v in (x, y, radius)) or radius < 1:
            raise ValueError("Regions require integer x/y and positive integer radius")
        if x - radius < 0 or y - radius < 0 or x + radius >= width or y + radius >= height:
            raise ValueError("LED region extends outside image")
        if "offsets" in led:
            offsets = led["offsets"]
            if not isinstance(offsets,list) or not 5 <= len(offsets) <= (2*radius+1)**2:
                raise ValueError("Invalid measured footprint")
            if any(not isinstance(p,list) or len(p)!=2 or any(type(v) is not int for v in p)
                   or p[0]**2+p[1]**2 > radius**2 for p in offsets):
                raise ValueError("Footprint outside region")
            if len({tuple(p) for p in offsets}) != len(offsets):
                raise ValueError("Duplicate footprint pixels")
        result.append((x, y, radius))
    return result


def locate(args):
    import cv2
    import numpy as np

    capture = json.loads((args.capture / "capture.json").read_text(encoding="utf-8"))
    if capture["status"] != "recorded":
        raise ValueError("Cannot locate regions in incomplete capture")
    peak = None
    for entry in capture["frames"]:
        path = args.capture / entry["file"]
        if digest(path) != entry["sha256"]:
            raise ValueError("Changed frame")
        frame = cv2.imread(str(path))
        if frame is None:
            raise ValueError("Unreadable frame")
        peak = frame if peak is None else np.maximum(peak, frame)
    mask = (np.min(peak, axis=2) > args.threshold).astype("uint8")
    count, _, stats, centers = cv2.connectedComponentsWithStats(mask)
    candidates = []
    for i in range(1, count):
        area = int(stats[i, cv2.CC_STAT_AREA])
        if args.min_area <= area <= args.max_area:
            x, y = [round(v) for v in centers[i]]
            number = len(candidates)
            candidates.append({"candidate": number, "x": x, "y": y, "area": area})
            cv2.circle(peak, (x, y), 9, (0, 255, 0), 1)
            cv2.putText(peak, f"C{number}", (x+10, y), cv2.FONT_HERSHEY_SIMPLEX,
                        .35, (0, 255, 0), 1)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump({"schema": 1, "mapping_status": "unverified",
                   "image_size": [peak.shape[1], peak.shape[0]], "candidates": candidates,
                   "capture_sha256": digest(args.capture / "capture.json"),
                   "threshold": args.threshold, "min_area": args.min_area, "max_area": args.max_area,
                   "note": "candidate labels are image regions, never firmware LED indices"}, stream, indent=2)
    annotated = args.output.with_suffix(".png")
    if annotated.exists():
        raise FileExistsError(annotated)
    if not cv2.imwrite(str(annotated), peak):
        raise RuntimeError("Could not write annotated image")
    print(f"{len(candidates)} unordered candidates; mapping remains unverified")


def extract(args):
    import cv2
    import numpy as np

    capture = json.loads((args.capture / "capture.json").read_text(encoding="utf-8"))
    if capture["status"] != "recorded":
        raise ValueError("Incomplete capture is not measurement evidence")
    config = json.loads(args.calibration.read_text(encoding="utf-8"))
    first = capture["frames"][0]
    rois = regions(config, first["width"], first["height"])
    with args.output.open("x", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["frame", "t_ms", "led", "red", "green", "blue", "clipped_fraction"])
        for number, entry in enumerate(capture["frames"]):
            path = args.capture / entry["file"]
            if digest(path) != entry["sha256"]:
                raise ValueError(f"Changed frame: {path}")
            frame = cv2.imread(str(path))
            if frame is None or frame.shape[:2] != (first["height"], first["width"]):
                raise ValueError("Unreadable image or changing resolution")
            for index, (x, y, radius) in enumerate(rois):
                if "offsets" in config["leds"][index]:
                    offsets=np.array(config["leds"][index]["offsets"])
                    pixels=frame[y+offsets[:,1],x+offsets[:,0]]
                else:
                    patch = frame[y-radius:y+radius+1, x-radius:x+radius+1]
                    yy, xx = np.ogrid[-radius:radius+1, -radius:radius+1]
                    pixels = patch[xx*xx + yy*yy <= radius*radius]
                blue, green, red = np.mean(pixels, axis=0)
                writer.writerow([number, entry["t_ms"], index, red, green, blue,
                                 float(np.mean(np.any(pixels >= 250, axis=1)))])
    args.output.with_suffix(args.output.suffix + ".json").write_text(json.dumps({
        "schema": 1, "capture_sha256": digest(args.capture / "capture.json"),
        "calibration_sha256": digest(args.calibration), "csv_sha256": digest(args.output),
        "meaning": "camera RGB code values, not linear radiance or firmware RGB",
        "verdict": "unassessed; needs dark/reference calibration and correlated command timeline",
    }, indent=2), encoding="utf-8")


def compare(args):
    """Compare temporal shapes, requiring an independently established time offset."""
    import numpy as np

    def load(path, time_key):
        with path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        times = sorted({float(row[time_key]) for row in rows})
        values = np.full((len(times), 23, 3), np.nan)
        lookup = {t: i for i, t in enumerate(times)}
        for row in rows:
            index = int(row["led"])
            if not 0 <= index < 23:
                raise ValueError("LED index out of range")
            slot = values[lookup[float(row[time_key])], index]
            if not np.all(np.isnan(slot)):
                raise ValueError("Duplicate LED/time sample")
            slot[:] = [float(row[c]) for c in ("red", "green", "blue")]
        if not times or not np.all(np.isfinite(values)):
            raise ValueError("Incomplete/nonfinite measurement or reference")
        return np.array(times), values

    times, measured = load(args.samples, "t_ms")
    reference_times, reference = load(args.reference, "t_ms")
    aligned = times - args.offset_ms
    # Never extrapolate outside exported time range. Each submitted frame holds
    # until the next submission. A final Off remains dark to the stated horizon.
    valid = (aligned >= 0) & (aligned <= args.horizon_ms)
    if np.sum(valid) < 10 or args.horizon_ms > 6000:
        raise ValueError("Need >=10 overlapping samples within the 6000ms reference horizon")
    expected = reference[np.searchsorted(reference_times, aligned[valid], side="right") - 1]
    measured = measured[valid]
    scores = []
    for led in range(23):
        actual = measured[:, led].mean(axis=1)
        ideal = expected[:, led].mean(axis=1)
        correlation = None
        if np.ptp(actual) >= args.minimum_range and np.ptp(ideal) > 0:
            correlation = float(np.corrcoef(actual, ideal)[0, 1])
        scores.append({"led": led, "temporal_correlation": correlation,
                       "camera_range": float(np.ptp(actual))})
    report = {"schema": 1, "samples_sha256": digest(args.samples),
              "reference_sha256": digest(args.reference), "offset_ms": args.offset_ms,
              "horizon_ms": args.horizon_ms, "overlap_frames": int(np.sum(valid)),
              "leds": scores, "verdict": "unassessed",
              "scope": "temporal brightness shape only; no color, absolute brightness, stop or protocol verdict",
              "limitations": "requires independently verified mapping/offset; thresholds not validated on hardware"}
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, allow_nan=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    rec = commands.add_parser("record")
    rec.add_argument("--output", type=Path, required=True)
    rec.add_argument("--camera", type=int, default=0)
    rec.add_argument("--backend", choices=["dshow", "msmf", "any"], default="dshow")
    rec.add_argument("--seconds", type=float, default=6)
    rec.add_argument("--warmup", type=int, default=20)
    rec.add_argument("--width", type=int, default=640)
    rec.add_argument("--height", type=int, default=480)
    rec.add_argument("--fps", type=float, default=30)
    rec.add_argument("--auto-exposure", type=float)
    rec.add_argument("--exposure", type=float)
    rec.add_argument("--auto-wb", type=float)
    ext = commands.add_parser("extract")
    ext.add_argument("--capture", type=Path, required=True)
    ext.add_argument("--calibration", type=Path, required=True)
    ext.add_argument("--output", type=Path, required=True)
    loc = commands.add_parser("locate")
    loc.add_argument("--capture", type=Path, required=True)
    loc.add_argument("--output", type=Path, required=True)
    loc.add_argument("--threshold", type=int, default=160)
    loc.add_argument("--min-area", type=int, default=2)
    loc.add_argument("--max-area", type=int, default=150)
    cmp = commands.add_parser("compare")
    cmp.add_argument("--samples", type=Path, required=True)
    cmp.add_argument("--reference", type=Path, required=True)
    cmp.add_argument("--offset-ms", type=float, required=True,
                     help="camera timeline timestamp of first effect submission, established independently")
    cmp.add_argument("--horizon-ms", type=float, default=6000)
    cmp.add_argument("--minimum-range", type=float, default=5)
    cmp.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "record":
        if not 0 < args.seconds <= 120 or not 0 <= args.warmup <= 300:
            parser.error("seconds must be >0 and <=120; warmup must be 0..300")
        record(args)
    elif args.command == "extract":
        extract(args)
    elif args.command == "locate":
        locate(args)
    else:
        compare(args)


if __name__ == "__main__":
    main()
