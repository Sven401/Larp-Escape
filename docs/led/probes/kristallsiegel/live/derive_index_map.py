"""Derive full-camera positions from verified atlas positions, preserving provenance."""
from pathlib import Path
import hashlib
import json
import statistics

import cv2

root = Path(__file__).resolve().parent
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
atlas_path = root / "index-map.json"
roi_path = root / "mapping-rois.json"
capture_path = root / "mapping-cal" / "capture.json"
events_path = root / "mapping-cal" / "events.json"
atlas = json.loads(atlas_path.read_text())
roi = json.loads(roi_path.read_text())
capture = json.loads(capture_path.read_text())
events = json.loads(events_path.read_text())
assert atlas["mapping_status"] == "verified"
assert atlas["capture_sha256"] == sha(capture_path)
assert capture["roi_layout_sha256"] == sha(roi_path)
assert capture["events_sha256"] == sha(events_path)
assert capture["roi_layout"] == roi
assert [p["index"] for p in atlas["leds"]] == list(range(23))
size, columns = roi["tile_size"], roi["columns"]
leds = []
for point in atlas["leds"]:
    x, y = point["x"], point["y"]
    tile = (y // size) * columns + x // size
    source = roi["regions"][tile]
    camera_x = source["x"] - size // 2 + x % size
    camera_y = source["y"] - size // 2 + y % size
    assert 0 <= camera_x < roi["source_image_size"][0]
    assert 0 <= camera_y < roi["source_image_size"][1]
    leds.append({"index": point["index"], "sector": 1 if point["index"] < 8 else 2 if point["index"] < 15 else 3,
                 "x": camera_x, "y": camera_y, "radius": point["radius"],
                 "atlas_x": x, "atlas_y": y, "acquisition_tile": tile})
assert len({(p["x"], p["y"]) for p in leds}) == 23
anchor = statistics.median(e["host_ms"] - e["completed_ms"] for e in events)
delay = atlas["timing"]["delay_ms"]
candidates = []
for position, event in enumerate(events[:-1]):
    if not all(p[0] == p[1] == p[2] and 4 <= p[0] <= 16 for p in event["pixels"]):
        continue
    start = event["completed_ms"] + anchor + delay
    end = events[position + 1]["completed_ms"] + anchor + delay
    for context in capture["contexts"]:
        if start + 70 <= context["t_ms"] <= end - 70:
            candidates.append((event["pixels"][0][0], context, event["stage"]))
assert candidates, "No full-camera context found during a white reference"
_, context, stage = max(candidates, key=lambda item: item[0])
context_path = root / "mapping-cal" / context["file"]
assert sha(context_path) == context["sha256"]
image = cv2.imread(str(context_path))
assert list(image.shape[1::-1]) == roi["source_image_size"]
colors = {1: (80, 215, 255), 2: (255, 220, 70), 3: (170, 100, 255)}
for point in leds:
    x, y, index = point["x"], point["y"], point["index"]
    color = colors[point["sector"]]
    cv2.circle(image, (x, y), 11, color, 2, cv2.LINE_AA)
    text = str(index)
    text_x, text_y = x + 13, y - 10
    cv2.putText(image, text, (text_x, text_y), cv2.FONT_HERSHEY_SIMPLEX, .53, (0, 0, 0), 4, cv2.LINE_AA)
    cv2.putText(image, text, (text_x, text_y), cv2.FONT_HERSHEY_SIMPLEX, .53, color, 1, cv2.LINE_AA)
cv2.rectangle(image, (15, 14), (820, 78), (12, 12, 12), -1)
cv2.putText(image, "Kristallsiegel: verified physical LED indices 0-22", (27, 38), cv2.FONT_HERSHEY_SIMPLEX, .61, (240, 240, 240), 1, cv2.LINE_AA)
for sector, label, x in ((1, "Section 1: 0-7", 27), (2, "Section 2: 8-14", 280), (3, "Section 3: 15-22", 548)):
    cv2.putText(image, label, (x, 65), cv2.FONT_HERSHEY_SIMPLEX, .55, colors[sector], 1, cv2.LINE_AA)
output_image = root / "index-map-camera.png"
assert cv2.imwrite(str(output_image), image)
result = {"schema": 1, "mapping_status": "verified atlas positions, coordinate conversion only",
          "radiometry_status": "unassessed", "image_size": roi["source_image_size"],
          "coordinate_system": "full decoded camera frame: x right, y down, origin upper left",
          "transform": "tile=floor(atlas_y/tile_size)*columns+floor(atlas_x/tile_size); camera_xy=region_xy-tile_size/2+(atlas_xy mod tile_size)",
          "source_commit": "dc8c5d2358ffc4d5753b21c8806261046b4788d3",
          "firmware_sha256": atlas["firmware_sha256"], "settings": atlas["settings"],
          "provenance": {"atlas_map": {"file": atlas_path.name, "sha256": sha(atlas_path)},
                         "acquisition_rois": {"file": roi_path.name, "sha256": sha(roi_path)},
                         "capture": {"file": "mapping-cal/capture.json", "sha256": sha(capture_path)},
                         "events": {"file": "mapping-cal/events.json", "sha256": sha(events_path)},
                         "context": {"file": "mapping-cal/" + context["file"], "sha256": sha(context_path), "t_ms": context["t_ms"], "stage": stage},
                         "generator": {"file": Path(__file__).name, "sha256": sha(Path(__file__))}},
          "timing": atlas["timing"], "verification": atlas["verification"], "leds": leds,
          "annotated_context": {"file": output_image.name, "sha256": sha(output_image)}}
output_json = root / "index-map-camera.json"
with output_json.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({"map": str(output_json), "map_sha256": sha(output_json), "image": str(output_image), "leds": leds}))
