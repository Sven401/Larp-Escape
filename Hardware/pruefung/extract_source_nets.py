"""Reproducible geometric source audit, not a CAD ERC or physical netlist.

Run from any directory; no external dependencies. Output is JSON on stdout.
EasyEDA wire polylines connect at endpoints, explicit junctions and pin contacts.
Interior crossings without such a contact are not joined. Equal flag names join.
SCHDOC is inspected structurally only; its connectivity is not decoded here.
"""
import collections
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def audit_json(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    shapes = data["schematics"][0]["dataStr"]["shape"]
    pins, labels, wires, junctions, components = [], [], [], [], []
    for shape in shapes:
        parts = shape.split("~")
        if parts[0] == "W":
            coords = list(map(float, parts[1].split()))
            assert len(coords) >= 4 and len(coords) % 2 == 0
            points = list(zip(coords[::2], coords[1::2]))
            wires.append(points)
        elif parts[0] == "J":
            junctions.append((float(parts[1]), float(parts[2])))
        elif parts[0] == "F":
            fields = shape.split("^^")
            labels.append((tuple(map(float, fields[1].split("~"))), fields[2].split("~")[0]))
        elif parts[0] == "LIB":
            content = shape.split("#@$")
            texts = [x.split("~") for x in content[1:] if x.startswith("T~")]
            ref = next((x[12] for x in texts if x[1] == "P"), "?")
            name = next((x[12] for x in texts if x[1] == "N"), "?")
            uuid = content[0].split("~")[6]
            component_pins = []
            for raw in content[1:]:
                if not raw.startswith("P~"):
                    continue
                fields = raw.split("^^")
                pos = tuple(map(float, fields[1].split("~")))
                header = fields[0].split("~")
                pin = {"ref": ref, "number": header[3], "name": fields[3].split("~")[4], "position": pos, "id": header[7]}
                component_pins.append(pin)
                pins.append(pin)
            if component_pins:
                components.append({"ref": ref, "name": name, "id": uuid, "pins": len(component_pins)})
    parent = {}

    def find(p):
        parent.setdefault(p, p)
        if parent[p] != p:
            parent[p] = find(parent[p])
        return parent[p]

    def union(a, b):
        parent[find(a)] = find(b)

    contacts = set(junctions) | {p["position"] for p in pins} | {p for p, _ in labels}
    contacts |= {p for wire in wires for p in wire}
    for p in contacts:
        find(p)
    for wire in wires:
        for a, b in zip(wire, wire[1:]):
            union(a, b)
            for p in contacts:
                cross = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
                if (abs(cross) < 1e-9 and min(a[0], b[0]) <= p[0] <= max(a[0], b[0])
                        and min(a[1], b[1]) <= p[1] <= max(a[1], b[1])):
                    union(a, p)
    label_roots = {}
    for p, label in labels:
        if label in label_roots:
            union(p, label_roots[label])
        label_roots[label] = p
    nets = {}
    for pin in pins:
        net = nets.setdefault(find(pin["position"]), {"pins": [], "labels": []})
        net["pins"].append(pin)
    for p, label in labels:
        net = nets.setdefault(find(p), {"pins": [], "labels": []})
        if label not in net["labels"]:
            net["labels"].append(label)
    return {"title": data["title"], "editor_version": data["editorVersion"], "shape_counts": dict(collections.Counter(x.split("~", 1)[0] for x in shapes)), "components": components, "pin_count": len(pins), "nets": list(nets.values())}


def audit_schdoc(path):
    records = [dict(item.split("=", 1) for item in line.split("|") if "=" in item)
               for line in path.read_text(encoding="utf-8", errors="strict").splitlines()]
    return {"header": records[0]["HEADER"], "record_counts": dict(collections.Counter(r.get("RECORD", "header") for r in records)),
            "component_ids": [r["UNIQUEID"] for r in records if r.get("RECORD") == "1"],
            "designators": [r["TEXT"] for r in records if r.get("RECORD") == "34"]}


def main():
    result = {"schema": "larp-escape-geometric-source-audit/1", "source_commit": "e4af436167c253cb674db1c0038bdef99031535b", "limitations": ["Geometric source extraction; no native CAD import, ERC or DRC", "No PCB source, cable harness or physical measurements", "Equal labels joined only inside one JSON drawing; no cross-drawing net joining"], "files": {}}
    for path in sorted(ROOT.iterdir()):
        if path.suffix.lower() not in (".json", ".schdoc", ".svg", ".csv"):
            continue
        entry = {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        if path.suffix == ".json":
            entry.update(audit_json(path))
        elif path.suffix == ".schdoc":
            entry.update(audit_schdoc(path))
        elif path.suffix == ".svg":
            entry["xml_parse"] = ET.parse(path).getroot().tag
        else:
            import csv
            with path.open(encoding="utf-16", newline="") as source:
                entry["bom"] = list(csv.DictReader(source, delimiter="\t"))
        result["files"][path.name] = entry
    json.dump(result, sys.stdout, ensure_ascii=True, indent=2)
    print()


if __name__ == "__main__":
    main()
