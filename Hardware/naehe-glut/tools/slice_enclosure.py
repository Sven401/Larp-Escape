"""Slice all NG-R1 print parts with the pinned official Bambu Studio engine.

Requires the official portable 2.8.2.61 distribution in .tmp/cad-tools/bambu-studio.
Keep the local Python executable beside its resources: copy python.exe and
python312.dll there as python-cli.exe/python312.dll, and set PYTHONHOME to the
original Python installation. See run_bambu.py for why the native entry is used.
No network connection or printer dispatch is performed.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / "Hardware/naehe-glut"
SLICER = ROOT / ".tmp/cad-tools/bambu-studio"
PROFILES = PACKAGE / "mechanik/slicer"
RUN_ID = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")


def slice_part(name):
    stl = PACKAGE / "mechanik" / (name + ".stl")
    digest = hashlib.sha256(stl.read_bytes()).hexdigest()
    destination = ROOT / ".tmp/naehe-glut-sliced" / RUN_ID / name
    destination.mkdir(parents=True, exist_ok=True)
    filament = "petg_translucent" if name == "diffuser" else "petg_hf"
    command = [str(SLICER / "python-cli.exe"), str(PACKAGE / "tools/run_bambu.py"),
               str(SLICER), "--datadir", str(ROOT / ".tmp/bambu-config"),
               "--load-settings", f"{PROFILES / 'machine.json'};{PROFILES / 'process.json'}",
               "--load-filaments", str(PROFILES / (filament + ".json")),
               "--slice", "0", "--outputdir", str(destination),
               "--export-3mf", name + ".3mf", str(stl)]
    environment = os.environ.copy()
    environment["PYTHONHOME"] = sys.base_prefix
    result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True)
    (destination / "console.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{name}: slicer returned {result.returncode}; see {destination}")
    report = json.loads((destination / "result.json").read_text())
    assert report["return_code"] == 0 and len(report["sliced_plates"]) == 1
    plate = report["sliced_plates"][0]
    gcode = (destination / "plate_1.gcode").read_bytes()
    expected_density = float(json.loads((PROFILES / (filament + ".json")).read_text())["filament_density"][0])
    actual_density = float(re.search(rb"; filament_density: ([0-9.]+)", gcode).group(1))
    assert len(gcode) > 1000 and abs(actual_density - expected_density) < 0.001
    assert hashlib.sha256(stl.read_bytes()).hexdigest() == digest, f"{name} changed during slicing"
    published = PACKAGE / "mechanik/print"
    published.mkdir(exist_ok=True)
    shutil.copyfile(destination / (name + ".3mf"), published / (name + ".3mf"))
    return {
        "part": name, "stl_sha256": digest, "filament": filament,
        "slicer_return_code": report["return_code"], "warning": plate["warning_message"],
        "estimated_seconds": plate["total_predication"],
        "estimated_grams": sum(f["total_used_g"] for f in plate["filaments"]),
        "bbox": plate["objects"][0]["bbox"],
        "gcode_sha256": hashlib.sha256(gcode).hexdigest(),
        "project_3mf_sha256": hashlib.sha256((published / (name + ".3mf")).read_bytes()).hexdigest(),
        "wall_loops": report["wall_loops"], "infill_percent": report["sparse_infill_density"],
        "layer_mm": report["layer_height"],
    }


if __name__ == "__main__":
    names = sys.argv[1:] or ["base", "tray", "lid", "diffuser", "ring_holder", "coupon"]
    # Native rendering/export is run serially; concurrent Windows export was unstable.
    with ThreadPoolExecutor(max_workers=1) as pool:
        rows = list(pool.map(slice_part, names))
    report = {"tool": "Bambu Studio 2.8.2.61", "printer": "Bambu Lab A1 mini, 0.4 mm",
              "bed": "Textured PEI Plate", "date": "2026-10-31",
              "note": "Digital slicing only; estimates are not real print or fit measurements.",
              "parts": rows}
    (PACKAGE / "mechanik/slice-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
