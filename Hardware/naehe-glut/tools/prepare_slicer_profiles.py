"""Resolve shipped Bambu Studio profile inheritance for its CLI loader.

Source profiles: BambuStudio v02.08.02.61, resources/profiles/BBL.
The CLI's --load-settings does not resolve system-profile inheritance itself.
"""
import hashlib
import json
from pathlib import Path
import sys


def main():
    source = Path(sys.argv[1]).resolve() / "resources/profiles/BBL"
    output = Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    sources = {}

    def resolve(kind, name, stack=()):
        if name in stack:
            raise ValueError(f"Profile cycle: {stack}, {name}")
        path = source / kind / (name + ".json")
        raw = path.read_bytes()
        sources[str(path.relative_to(source))] = hashlib.sha256(raw).hexdigest()
        current = json.loads(raw)
        result = {}
        parent = current.get("inherits")
        if parent:
            result.update(resolve(kind, parent, (*stack, name)))
        for include in current.get("include", []):
            result.update(resolve(kind, include, (*stack, name)))
        result.update({k: v for k, v in current.items() if k not in ("inherits", "include")})
        return result

    selections = {
        "machine": ("machine", "Bambu Lab A1 mini 0.4 nozzle"),
        "process": ("process", "0.20mm Standard @BBL A1M"),
        "petg_basic": ("filament", "Bambu PETG Basic @BBL A1M 0.4 nozzle"),
        "petg_translucent": ("filament", "Bambu PETG Translucent @BBL A1M"),
    }
    for target, (kind, name) in selections.items():
        data = resolve(kind, name)
        if target == "process":
            data.update(wall_loops="4", sparse_infill_density="20%",
                        sparse_infill_pattern="gyroid", bottom_shell_layers="5",
                        top_shell_layers="5", enable_support="0", brim_type="no_brim",
                        curr_bed_type="Textured PEI Plate")
        if target == "machine":
            assert data["printer_model"] == "Bambu Lab A1 mini"
        if kind == "filament":
            assert float(data["filament_density"][0]) > 1
            assert float(data["nozzle_temperature"][0]) >= 220
        (output / (target + ".json")).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    (output / "profile-sources.json").write_text(json.dumps({
        "source": "https://github.com/bambulab/BambuStudio/tree/v02.08.02.61/resources/profiles/BBL",
        "source_license": "BambuStudio AGPL-3.0; retain upstream attribution",
        "sha256": sources,
        "overrides": "4 walls, 20% gyroid, 5 top/bottom layers, no support, no brim",
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
