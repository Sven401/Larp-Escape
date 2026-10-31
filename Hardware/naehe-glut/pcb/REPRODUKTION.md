# PCB-Quellen und Export

Werkzeuge: KiCad 10.0.8 mit pcbnew-Python, FreeCAD 1.0.2 für die zwei
Bauteilhüllen, gerbonara 1.6.3 für unabhängigen CAM-Rückimport, Inkscape für
Rasteransichten. Die editierbaren KiCad-Dateien und Projektbibliotheken liegen
direkt hier; sie sind ohne Generator bearbeitbar.

Für die lokal entpackte KiCad-Runtime müssen `PYTHONHOME` auf deren `bin`,
`KICAD_CONFIG_HOME` auf einen beschreibbaren Konfigurationsordner und
`KICAD10_3DMODEL_DIR` auf `share/kicad/3dmodels` zeigen. Bei normal installiertem
KiCad genügt dessen eigene Umgebung. Die unten genannten Programme können
durch absolute Installationspfade ersetzt werden.

Vom Repository-Stamm aus erst `tools/build_schematic.py`, danach den
XML-Netzexport ausführen. `tools/build_component_envelopes.py` mit FreeCAD-
Python und anschließend `tools/build_pcb.py` mit KiCad-Python ausführen.
Alle Toolpfade beginnen mit `Hardware/naehe-glut/`.
Der Router erzeugt ein vollständig neues Layout; manuelle Änderungen an den
nativen Dateien werden dabei überschrieben. Deshalb erst in einer Arbeitskopie
neu bauen und sämtliche Prüfungen wiederholen.

Folgende CLI-Aufrufe erfolgen **aus diesem PCB-Verzeichnis**:

```powershell
kicad-cli sch export netlist --format kicadxml -o exports/netlist.xml naehe-glut-r1.kicad_sch
kicad-cli sch erc --exit-code-violations -o pruefung/erc.rpt naehe-glut-r1.kicad_sch
kicad-cli pcb drc --schematic-parity --exit-code-violations -o pruefung/drc.rpt naehe-glut-r1.kicad_pcb
kicad-cli sch export svg -o exports/ naehe-glut-r1.kicad_sch
kicad-cli pcb export gerbers -l F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts -o exports/gerber/ naehe-glut-r1.kicad_pcb
kicad-cli pcb export drill --excellon-separate-th --generate-report --report-path pruefung/drill-report.rpt -o exports/gerber/ naehe-glut-r1.kicad_pcb
kicad-cli pcb export step --force -o exports/naehe-glut-r1.step naehe-glut-r1.kicad_pcb
kicad-cli pcb export svg --mode-single --fit-page-to-board --exclude-drawing-sheet -l F.Cu,F.Silkscreen,Edge.Cuts -o exports/top.svg naehe-glut-r1.kicad_pcb
kicad-cli pcb export svg --mode-single --fit-page-to-board --exclude-drawing-sheet --mirror -l B.Cu,B.Silkscreen,Edge.Cuts -o exports/bottom.svg naehe-glut-r1.kicad_pcb
kicad-cli pcb export svg --mode-single --fit-page-to-board --exclude-drawing-sheet --sketch-pads-on-fab-layers -l F.Fab,Edge.Cuts -o exports/assembly.svg naehe-glut-r1.kicad_pcb
```

Danach mit KiCad-Python `tools/check_pcb_contract.py` und mit normalem Python
plus gerbonara `tools/check_cam.py` ausführen. Die zwei resultierenden CAM-SVGs
mit Inkscape `--export-type=png --export-width=1600` rasterisieren und sichten.
Abschließend `tools/package_pcb.py` ausführen; dessen ZIP enthält genau sieben
Gerberlagen, den Gerber-Job, PTH/NPTH-Excellon und die Fertigungsspezifikation.
Das SHA256-Manifest bindet die nativen Quellen, Bibliotheken, Exporte und
Prüfbelege. Es darf nach Änderungen erst nach erneuter Prüfung aktualisiert
werden. STEP-Neuexport erfordert außerdem die erneute Gehäuse-Kollisionsprüfung.
