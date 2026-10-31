"""Probe the installed FreeCAD kernel/export path, not a product enclosure.

Windows example, from repository root:
  & 'C:/Program Files/FreeCAD 1.0/bin/python.exe' Hardware/naehe-glut/tools/probe_cad.py
"""

import json
import math
from pathlib import Path

import FreeCAD
import Mesh
import Part


def main():
    root = Path(__file__).resolve().parents[3]
    destination = root / ".tmp" / "naehe-glut-cad-probe"
    destination.mkdir(parents=True, exist_ok=True)
    document = FreeCAD.newDocument("ToolProbe")
    probe = document.addObject("Part::Feature", "Probe")
    probe.Shape = Part.makeBox(10, 20, 3)
    document.recompute()
    document.saveAs(str(destination / "probe.FCStd"))
    Part.export([probe], str(destination / "probe.step"))
    Mesh.export([probe], str(destination / "probe.stl"))
    shape = Part.Shape()
    shape.read(str(destination / "probe.step"))
    mesh = Mesh.Mesh(str(destination / "probe.stl"))
    FreeCAD.closeDocument(document.Name)
    reopened = FreeCAD.openDocument(str(destination / "probe.FCStd"))
    native = reopened.getObject("Probe").Shape
    result = {
        "purpose": "CAD tool probe only; not a product artifact",
        "freecad_version": ".".join(FreeCAD.Version()[:3]),
        "dimensions_mm": [10, 20, 3],
        "fcstd_valid": native.isValid(),
        "fcstd_volume_mm3": native.Volume,
        "step_valid": shape.isValid(),
        "step_solids": len(shape.Solids),
        "step_volume_mm3": shape.Volume,
        "stl_closed": mesh.isSolid(),
        "stl_facets": mesh.CountFacets,
        "stl_volume_mm3": mesh.Volume,
    }
    valid = (
        result["fcstd_valid"]
        and result["step_valid"]
        and result["step_solids"] == 1
        and result["stl_closed"]
        and result["stl_facets"] == 12
        and all(math.isclose(result[key], 600.0, abs_tol=0.001)
                for key in ("fcstd_volume_mm3", "step_volume_mm3", "stl_volume_mm3"))
    )
    result["passed"] = bool(valid)
    (destination / "result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2))
    FreeCAD.closeDocument(reopened.Name)
    if not valid:
        raise SystemExit("CAD export/reimport probe failed")


if __name__ == "__main__":
    main()
