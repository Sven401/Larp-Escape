# NG-R1: unabhängiger Hardware- und Fertigungsreview

Prüfer: `/root/hardware_review`, getrennt von Elektronik-, Mechanik- und
Slicerimplementierung. Datum: 31.10.2026. Basiscommit des Auftrags:
`63477d08e160d1b1f81b37bbd16593c1a2087624`; geprüft werden die unten über
SHA-256 bezeichneten Arbeitsdateien, nicht eine behauptete Geräteversion.

**Ergebnis: digitale Prototyp-Fertigungsfreigabe für die unten gebundenen
PCB- und Druckpakete. Keine offenen harten digitalen Reviewbefunde.**
Die Freigabe gilt für unbestückte PCB-Fertigung sowie die erste Druck-/Montage-
Erprobung nach den beigefügten Anleitungen. Sie ist keine Betriebsfreigabe,
keine Zusage der Bauteillieferbarkeit und kein Nachweis zweier funktionierender
Stationen. Physische Prüfungen und S3-/R1-Firmware bleiben offen.

## Selbst ausgeführte Gegenprüfungen

- `circuit.json`, Schaltplangenerator, PCB-Generator, native Quellen und
  exportierte Netzliste gelesen. Mit einem eigenen Python-Abgleich wurden
  alle **54 elektrisch verbundenen Ref./Pin-Paare** aus der JSON-Quelle,
  der Schaltplan-XML und den direkt mit `pcbnew` gelesenen PCB-Pads verglichen:
  identisch. Das führende `/` der KiCad-Blattnetznamen wurde normalisiert;
  explizite NC-Netze wurden nur bei diesem 54-Pin-Vergleich ausgeklammert.
- Carrier-ESP-Verbindungen sind GPIO15, GPIO16 und GND; keine Verbindung
  zu ESP-5V/3V3. J1/J3 haben 22 Kontakte, Raster 2,54 mm und Reihenabstand
  22,86 mm. Vier Montagebohrungen entsprechen der mechanischen Schnittstelle.
- AHCT-Pinbelegung einschließlich unbenutzter Gates, 1-kΩ-Eingangswiderstand,
  Daten-Pulldowns, 330-Ω-Ausgangswiderstand, NPN-Freigabe mit Basiswiderstand
  und Basis-Pulldown sowie 5-V-OE-Pullup geprüft. GPIO16 erhält dabei keine
  direkte 5-V-Verbindung. Zwei getrennte 5,1-kΩ-Rd liegen an CC1/CC2.
- GCT-Zeichnung USB4135, Rev. A1 vom 22.11.2022, als Bild selbst gesichtet:
  Padfolge B12/B9/A5/B5/A9/A12, Abstandsgruppen 5,50/3,04/1,00 mm,
  Kontaktpadbreiten 0,80/0,76/0,70 mm bei 1,20 mm Länge sowie vier
  Schirmanker stimmen mit dem gebündelten Footprint überein. VBUS/GND/CC
  sind richtig zugeordnet. KEMET C320 ist mit 2,54-mm-Raster umgesetzt;
  F1 hat 5,08 mm, Q1 den dokumentierten aufzubiegenden E/B/C-Footprint.
- Fertigungs-/BOM-/Montagevertrag einschließlich genauer Teilenummern,
  Polung, Stromquelle, Leitungen und materialgebundener PCB-Befestigung
  gelesen. Bohrdaten enthalten 99 metallisierte Bohrungen (88 Bauteilkontakte,
  11 Vias) und vier getrennte nicht metallisierte Ø3,2-mm-Montagebohrungen.
- Finale ERC-/DRC-Berichte gelesen: 0 Fehler/Warnungen, 0 offene Verbindungen,
  einschließlich Schaltplanparität. Diese Werkzeugläufe stammen vom Entwickler;
  der oben beschriebene Pin-/Netzvergleich wurde zusätzlich selbst ausgeführt.
  Den echten Gerber-Rückimport `cam-top.png`/`cam-bottom.png` selbst gesichtet:
  Außenkontur mit Antennenausschnitt, Reihen, USB-Fanout, Lötstopp und
  Polungsmarkierungen plausibel. Die vier NPTH erscheinen im verwendeten
  Renderer nur oben; ihre tatsächlichen Excellon-Koordinaten wurden separat
  gelesen. Kein Ersatz der CAM-Prüfung durch eine Editoransicht.
- Mechanikgenerator und Kollisionsprüfungen gelesen. Ein eigener
  FreeCAD-Aufruf von `build()` ohne Export bestätigte mit dem aktuellen echten
  PCB-STEP keine Kollision von PCB oder ergänzenden F1/C1/U1-Hüllen mit
  Druckteilen. Transformation: X=45+u, Y=137−v, PCB-Unterseite Z=46 mm.
  Die Diffusorauflage wurde selbst mit **20,997946 mm²** nachgerechnet.
- Schraubenlängen anhand der Montagekoordinaten nachgerechnet: Deckel
  M3×12, Träger M3×10, Ringhalter M3×16, PCB M3×10 aus PA/Nylon passen
  zu den vorgesehenen Mutterhöhen und Bohrungstiefen. Darstellung von
  zusammengebautem und explodiertem Gehäuse gesichtet. Akku liegt unter
  dem herausnehmbaren Träger; Klett und offene Lüftung sind vorgesehen.
- Slicerprofilauflösung, DLL-Wrapper und Slicerskript gelesen. Die Profile
  lösen Vererbung auf und setzen vier Wände, 20 % Gyroid, 0,20-mm-Schichten,
  fünf Boden-/Decklagen, keine Stützen und keinen Brim. Keine Druckübertragung.
- Alle sechs STL- und 3MF-Hashes gegen `slice-report.json` unabhängig
  berechnet; jedes 3MF geöffnet und Hash des enthaltenen G-Codes geprüft:
  identisch. Im tatsächlichen G-Code A1 mini, vier Wände, Gyroid, PETG HF
  mit Dichte 1,28/240 °C bzw. PETG Translucent mit 1,25/245 °C und
  70 °C Textured-PEI-Bett bestätigt. Base, Diffusor und Ringhalter zusätzlich
  zwischen STL und eingebettetem 3MF-Netz über Volumen verglichen: keine
  Skalierung. Die größeren Slicer-Bounding-Boxes bei runden Teilen entstehen
  durch XY-Drehung der Begrenzungsbox, nicht durch vergrößerte Bauteile.
- Beschaffungs- und Druckanleitungen gegengelesen. Mengen, zwei getrennte
  USB-Kabel, Ladegeräte, Werkzeugbedarf, Filamentabschätzung und das offene
  N8R8-Beschaffungsrisiko sind sichtbar; Dezemberziel und PCB-Rückfallweg
  bleiben erhalten.
- Finales PCB-Manifest mit **51 Dateien** unabhängig nachgerechnet und alle
  Manifestpfade auf Nichtausschluss durch Git-Ignore geprüft; das
  Fertigungs-ZIP geöffnet, CRC geprüft und alle elf enthaltenen Dateien
  bytegenau gegen die sieben Gerberlagen, Gerber-Job, zwei Bohrdateien und
  `FERTIGUNG.md` im Arbeitsbaum verglichen. Druck-ZIP ebenfalls geöffnet:
  alle **26 Nutzdateien plus SHA256.json** stimmen bytegenau mit Manifest
  und Arbeitsbaum überein. Die aktuelle Geometrieprüfung bindet den unten
  genannten STEP-Hash und meldet keine Baugruppen-/Modulkollisionen.

## Im Review geschlossene Punkte

- Veraltete PCB-STEP-Bindung im Geometriebericht erkannt. Nach letztem
  PCB-Export wurde die formale Geometrieprüfung erneut ausgeführt; aktueller
  Bericht, FCStd und Assembly-STEP stimmen zum freigegebenen Exportstand.
- Der Druckpacker prüfte anfangs nur Slicerhashes. Er prüft jetzt zusätzlich
  `geometry.passed`, den aktuellen PCB-STEP-Hash und erfolgreiche warnungsfreie
  Slices. Die Ergänzung wurde gelesen; der Koordinator belegte den ablehnenden
  Lauf mit veraltetem STEP. Das daraus anschließend erzeugte Paket wurde
  durch den Reviewer unabhängig geöffnet und abgeglichen.
- Ein erster Verdacht auf Diffusor-Fallspiel wurde nach Gegenklärung und
  eigener Flächenrechnung zurückgezogen: Die drei Restlippen tragen den Rand.
- Die fehlende C2-Referenz im Bestückungs-SVG ist durch eindeutige Lage- und
  Padkoordinaten in `FERTIGUNG.md` dokumentiert. Bei manueller Bestückung
  diese Zuordnung mitbenutzen; ein nicht dokumentierter Sockel-/Bauteilersatz
  gehört nicht zu dieser Freigabe.

## Fachliche Grenzen und notwendige physische Abnahme

TI SCLS264R spezifiziert den SN74AHCT125-Eingangsleckstrom auch bei VCC=0;
dies stützt die getrennten Einschaltfolgen am Eingang. Es ist **keine**
allgemeine Powered-off-Toleranz seines Ausgangs. Ring und Buffer müssen am
selben LED5V-Zweig bleiben. Die NPN-Freigabe verhindert keinen gespeicherten
LED-Zustand: Ein stromloser ESP kann den zuletzt gesetzten Ring weiter leuchten
lassen. Schwarzer Startframe für 24 Pixel und Betriebs-Stop gehören zur noch
nötigen Firmwareanpassung; für Service beide Versorgungspfade ausschalten.

Die DC-Strompfadrechnung berücksichtigt 35-µm-Kupfer, kurze 0,4-mm-USB-
Anbindungen, 1,2-mm-Versorgungszüge und Vias mit mindestens 20 µm Hülsenkupfer.
Sie ersetzt keine Temperaturmessung. Die zusätzliche träge 2-A-Sicherung
ist keine aktive Strombegrenzung und muss hinter einer strombegrenzten
Powerbank nicht sicher auslösen. Die freigegebene Versorgungsart bleibt
V50 USB-A: höchstens 2 A je Port/3 A zusammen; keine beliebige stärkere
Quelle und kein PC-Port als LED-Versorgung.

Die Konstruktion ist ein Prototyp für trockenen, vor Regen geschützten Einsatz.
Ausstehend bleiben Coupon-/Passprobe, reale Kabel-/Steckermaße und Verlegung,
Löt-/Durchgangsprüfung, Ein-/Ausschaltfolgen, Spannung und Temperatur bei
maximaler Last, Akku-Dauerlauf, Optik und Funk mit beiden geschlossenen
Stationen. Die drei 2,5-mm-Ringbinder müssen zwischen LEDs geführt werden;
keine LED oder Lötstelle darf unter ihnen liegen. PCB-Schrauben/-Muttern
müssen wegen des Antennenbereichs PA/Nylon sein. Das digitale Modell prüft
Freiräume, keine Funkreichweite. Laden nur mit entnommenem Akku.

## Quellen der elektrischen Gegenprüfung

- [TI SN74AHCT125, SCLS264R](https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf):
  Versorgung, Eingangsschwellen, VCC=0-Eingangsleckstrom und OE-Verhalten.
- [GCT USB4135](https://gct.co/connector/usb4135),
  [GCT-Zeichnung, Händlerkopie](https://www.avelmak.sk/media/dsc_items/91625/usb4135.pdf):
  Pinfolge, Landpattern und 3-A-Gesamtnennstrom der VBUS-Kontakte.
- [Voltaic V50](https://voltaicsystems.com/v50/): 118×82×24 mm,
  USB-A-Always-On und Stromgrenzen; keine Übertragung auf USB-C-PD.
- [Adafruit 1586](https://www.adafruit.com/product/1586) und
  [NeoPixel Best Practices](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices):
  24er-Ring, Maße und Daten-/Versorgungsbeschaltung.
- Weitere Teile-/Maßquellen sind in `pcb/BOM.md` und `mechanik/MONTAGE.md`
  je konkreter Bestellnummer verlinkt.

## Revisionsbindung

SHA-256, Pfade relativ zu `Hardware/naehe-glut/`. Das PCB-Manifest bindet
zusätzlich die einzelnen nativen CAD-Dateien, Bibliotheken und Prüfberichte;
`slice-report.json` bindet alle sechs STL/3MF/G-Code-Sätze. Änderungen an
gebundenen Quellen oder Lieferdateien erfordern eine passende erneute Prüfung.

| Datei | SHA-256 |
| --- | --- |
| `pcb/exports/naehe-glut-r1-fertigung.zip` | `0265f6784e9ce061d8e2eecfd65aed418c8fa6ab3b054096a68a49767d1ae173` |
| `fertigung/NG-R1-Druckpaket.zip` | `37d0aa60937481615a3de703bd0868441432eb2472f5a9483ff93f37b5cacd02` |
| `pcb/SHA256SUMS.txt` | `a48174053657f5f8849cf80bc2c02f1920e79ff3c98b9f081225f8d42e643dc7` |
| `pcb/exports/naehe-glut-r1.step` | `142e65eeb9bf46b686526d12e6fadaa2a7eabe060a8cecdd4da1308241c7ec7c` |
| `mechanik/geometry-report.json` | `239050d13db0ce00a56d7a18939709375b8c01333275b64f614a1ca49b27cd30` |
| `mechanik/slice-report.json` | `8602e6392f48347e10f611d01e663b7a3d50c1f7beaf436b9408a0811a3a8667` |
| `mechanik/NG-R1.FCStd` | `d6fcc83fa364b181427dfb446687a147de4f8078d2574ec1c312849f50bb48f6` |
| `mechanik/assembly.step` | `44bbe4e8e41327c876fb4171d29bdbfddf573245f9cf350f5865688b0f730e95` |
| `mechanik/MONTAGE.md` | `79476549159e67511b8ff9136affba0add4c65be953ac107d705e9cc4e798ae5` |
| `mechanik/DRUCK.md` | `2a40ea6a47a0210945a37b80ee6a79f63bc6c0856e099ef796e5ceea5c44c65c` |
| `BESTELLUNG.md` | `be28062bca316c09fc8880fb13aa09d996170b0674d6df4080aac9c7714a7ec3` |
| `README.md` | `46a8d5f0577f927714d3dcc5191b5f2bb7f00a3bb7c9f617e3ed9d97c2b0baf4` |
| `tools/build_enclosure.py` | `6be6502617cf05983f42a0b49e7d4bd04d404114be7515850fe459c4f2a40509` |
| `tools/build_pcb.py` | `29afafa5db08f90141b151d638a4ccdf6afc0211bd5df3dba45130e75ef9a900` |
| `tools/build_schematic.py` | `53a31bdbe0a0a55f98f74898c7fd9fc866d5ec192c64c48c67a781c186ce691e` |
| `tools/prepare_slicer_profiles.py` | `c65b0383a1cff3462031e696292e693d0de3803511f8a624791b7b6ad9e0e282` |
| `tools/run_bambu.py` | `7fc3e3139f9091e5d46ddc10f0968e06a86e629161a5818dfd275ab5150934de` |
| `tools/slice_enclosure.py` | `f6a3a05623f14efe50916eecca5033a1cb85f2e4c4997e42b14104db20d82ae1` |
| `tools/package_prints.py` | `316b20c9e77e5a0de19fb96604934c6318a54114f080879ecd3686f57df5b068` |
| `tools/check_pcb_contract.py` | `8881e4a7e67d635c2ccf242093ed6081b5b661ec85b07ab64c0f57cbe0689450` |
| `tools/check_cam.py` | `e46d0841058867f8fc71f7c012a4626f00d088c7454377843cf134dae7007206` |
| `tools/package_pcb.py` | `9a05bca3a7c3b75e55095186f0bcb989df7ca0b51622ebf808fe016c85ec5ee9` |
