"""Quellabgleich des POC; ersetzt weder KiCad/EasyEDA-Import noch ERC/DRC."""

import csv
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parent
pcb = (ROOT / "poc-r0.kicad_pcb").read_text(encoding="utf-8")


def balanced_sexpr(source):
    depth = 0
    quoted = False
    escaped = False
    for char in source:
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0 and not quoted


assert balanced_sexpr(pcb), "PCB-S-Ausdrücke unbalanciert"
assert pcb.startswith("(kicad_pcb (version 20171130)"), "keine KiCad-5.1-Kennung"
assert pcb.count("(gr_line ") == 4, "Umriss nicht als vier Linien vorhanden"
assert "(segment " not in pcb, "Layout ist als ungeroutet dokumentiert"

for svg in ROOT.glob("*.svg"):
    ET.parse(svg)

with (ROOT / "netzliste.csv").open(encoding="utf-8", newline="") as source:
    rows = list(csv.DictReader(source, delimiter=";"))
with (ROOT / "stueckliste.csv").open(encoding="utf-8", newline="") as source:
    bom = list(csv.DictReader(source, delimiter=";"))
assert all(row["Netz"] and row["Anschlüsse"] for row in rows)
assert all(row["Referenz"] and row["Anzahl"] for row in bom)

expected = {}
for row in rows:
    if row["Netz"] == "NC":
        continue
    for endpoint in row["Anschlüsse"].split():
        ref, pin = endpoint.rsplit("/", 1)
        ref = {"X1.J2": "S1", "X1.J3": "S2"}.get(ref, ref)
        key = (ref, pin)
        assert key not in expected, f"Netzlistenpin mehrfach: {key}"
        expected[key] = row["Netz"]

actual = {}
modules = re.findall(r"^  \(module .*?^  \)", pcb, flags=re.MULTILINE | re.DOTALL)
for module in modules:
    match = re.search(r'\(fp_text reference "([^"]+)"', module)
    assert match, "Footprint ohne Referenz"
    ref = match.group(1)
    for line in module.splitlines():
        match = re.search(r'^    \(pad "([^"]+)" .*?\(net \d+ "([^"]+)"\)', line)
        if match:
            key = (ref, match.group(1))
            assert key not in actual, f"PCB-Pad mehrfach: {key}"
            actual[key] = match.group(2)

assert expected == actual, (
    f"Soll/Ist-Abweichung: nur CSV={set(expected.items()) - set(actual.items())}; "
    f"nur PCB={set(actual.items()) - set(expected.items())}"
)
print(
    f"Quellprüfung OK: {len(modules)} Footprints, {pcb.count('(pad ')} Pads, "
    f"{len(actual)} zugeordnete Pads, {len(rows)} Netzeinträge, "
    f"{len(bom)} BOM-Zeilen, 2 SVGs; 0 Leiterbahnen. Kein CAD-Import/ERC/DRC."
)
