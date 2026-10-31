# NG-R1: Gehäuse und Montage

Stand: 31.10.2026. **Digitaler Entwicklungsstand, physische Passprobe offen.**
Mechanik: `/root/enclosure_developer`; unabhängiger Review:
`/root/hardware_review`. Quellenbasis des Auftrags: `63477d0`; aktuelle
Dateiprüfungen stehen in `geometry-report.json`, Slicer-Nachweise separat.

## Umfang und Aufbau

Außenmaß **168 × 148 × 90 mm**, zuzüglich 3 mm Schraubenköpfen auf dem Deckel.
Alle Einzelteile passen in 180 × 180 × 180 mm Bauraum des vorgesehenen
Bambu Lab A1 mini. Druckmaterial: PETG für die fünf mechanischen Teile,
transparentes/naturfarbenes PETG für den Diffusor. Die Diffusion und sichtbare
Lichtwirkung müssen am ersten Druck geprüft werden; das Rendering simuliert
keine Optik.

Der Akku liegt unten. Vier Schrauben halten den herausnehmbaren Elektronikträger
darüber. Die Platine wird mit **PA-/Nylonschrauben und PA-/Nylonmuttern** befestigt;
Metall ist hier wegen der Antennennähe kein zulässiger Ersatz. Der Deckel trägt
einen separaten Ringhalter und einen austauschbaren Diffusor. Alle Schrauben
sind von oben zugänglich. Für Service und Laden wird der Deckel geöffnet;
es gibt keine außenliegenden USB-Buchsen.

**Betriebsgrenze:** vor Regen geschützt, keine IP-Einstufung. Lüftungsschlitze
und Gurtschlitze sind offen. Akku zum Laden entnehmen. Keine Ladesicherheit,
Temperaturfreigabe oder Laufzeit aus der digitalen Geometrie ableiten.

## Lieferdateien und Drucklage

| Datei | Stück je Station | Drucklage in STL |
| --- | ---: | --- |
| `base.stl` | 1 | Boden außen auf dem Druckbett, offene Seite oben |
| `tray.stl` | 1 | Flache Unterseite unten; Platinenstützen oben |
| `lid.stl` | 1 | Sicht-/Außenseite unten; Zentrierrand oben |
| `diffuser.stl` | 1 | Geschlossene optische Außenfläche unten; konischer Rand oben |
| `ring_holder.stl` | 1 | Flache Ringauflage unten, Schraubpfosten oben |
| `coupon.stl` | 1 Passprobe vor Serie | Flach wie exportiert |

Die STL-Dateien sind einzeln auf Z=0 und positive XY-Koordinaten gelegt. STEP
und `NG-R1.FCStd` verwenden gemeinsame Montagekoordinaten. Deshalb einzelne
STLs nicht anhand ihrer automatisch verschobenen Ursprünge zur Baugruppe
zusammensetzen. `assembly.step` enthält die Baugruppe samt ausdrücklich
vereinfachten Referenzkörpern; diese Referenzen werden nicht gedruckt.

Ausgangsprofil: 0,4-mm-Düse, 0,20-mm-Schichten, vier Wandlinien, 20 % Gyroid,
fünf Boden-/Deckschichten. Der separate Slicerbericht entscheidet über die
tatsächlich geprüften Einstellungen. Die Konstruktion vermeidet große freie
Brücken; Lüftungsschlitze und Mutterzuführungen haben kleine Brücken von etwa
5–6 mm. Der Diffusorkonus wächst in Druckrichtung um weniger als 45° nach außen.
Keinen Stützstoff ungeprüft einschalten: dieser könnte gefangene Mutternuten
füllen. Den Coupon zuerst drucken und M3-Mutter-/Schraubenpassung prüfen.

## Ergänzende mechanische Bestellliste

Alle Mengen für **zwei Stationen**, Reserven ausdrücklich zusätzlich:

| Artikel / Spezifikation | Einbau | Bestellmenge mit Reserve |
| --- | ---: | ---: |
| M3 × 12 mm Zylinderschraube, ISO 4762, Edelstahl A2 | 8 Deckel | 10 |
| M3 × 10 mm Zylinderschraube, ISO 4762, Edelstahl A2 | 8 Träger | 10 |
| M3 × 16 mm Zylinderschraube, ISO 4762, Edelstahl A2 | 6 Ringhalter | 10 |
| M3 Sechskantmutter, ISO 4032/DIN 934, A2, SW 5,5 mm, Höhe 2,4 mm | 22 | 30 |
| **M3 × 10 mm PA-/Nylonschraube**, Kopf Ø höchstens 5,5 mm | 8 Platine | 10 |
| **M3 PA-/Nylon-Sechskantmutter**, SW 5,5 mm, Höhe höchstens 2,4 mm | 8 Platine | 10 |
| Wiederverwendbares Klettband, 20 mm breit, ca. 300 mm lang | 2 Akku | 4 |
| Kabelbinder, 2,5 mm breit, ca. 100 mm lang | 6 Ring + Kabel | 20 |
| PETG und transparentes/naturfarbenes PETG, 1,75 mm | gemäß Slicer | übergreifende Bestellliste |

Keine Unterlegscheiben in die gefangenen Mutternuten legen. Eine andere
Kopfform ist nur zulässig, wenn der reale Kopf vollständig aufliegt und im
geprüften Kopfraum bleibt. Schrauben handfest anziehen; PETG und Nylon nicht
mit hohem Drehmoment klemmen. Das Gewinde in den Muttern entsteht nicht im Druck.

## Montagefolge

1. Coupon prüfen: M3-Mutter muss in die 5,8-mm-Aufnahme passen, M3-Schraube
   durch die 3,4-mm-Bohrung gleiten. Druckfehler/Elefantenfuß entfernen;
   bei systematischem Maßfehler Druckprofil korrigieren, nicht alle Teile erzwingen.
2. Vier Stahlmuttern seitlich in die hohen Deckelpfosten einschieben. Vier
   Stahlmuttern oben in die niedrigen Trägerpfosten einlegen. Unterseitige
   Klettbandschlitze einfädeln. Akku mit USB-A-Anschlüssen Richtung +X
   (rechte Gehäuseseite in der Draufsicht) einlegen und mit Klett sichern.
3. Vier Nylonmuttern von unten in den Träger einlegen. Platine auf die vier
   5-mm-Stützen legen und mit M3 × 10 PA-Schrauben befestigen. Der ESP zeigt
   mit seiner Antenne zur Rückseite (+Y), von der Powerbank weg.
4. Träger mit vier M3 × 10 Stahlschrauben montieren. Beide USB-Kabel separat
   einstecken: Powerbank → ESP-Micro-USB und Powerbank → Carrier-USB-C.
   Kabel seitlich und vorne führen; im markierten Antennenbereich keine
   Kabelschlaufen, Metallteile oder andere Gegenstände unterbringen.
   Ausgewählte Kabel: Delock 84900 (USB-A → Micro-B, 0,5 m) und Delock 83326
   (USB-A → USB-C, 0,5 m). Maximale für diese Revision reservierte Steckerhüllen:
   am ESP 20 × 25 × 10 mm, am Carrier 16 × 25 × 17 mm; an den beiden
   Akkuanschlüssen jeweils 20 mm Breite, 32 mm Länge und 20 mm Höhe.
   Die Hersteller nennen keine vollständigen Steckergehäusezeichnungen;
   diese Hüllen sind deshalb Beschaffungs-/Passgrenzen, keine gemessenen Kabelmaße.
5. LED-Ring mit LEDs nach oben in den Ringhalter legen. Ring drehen, bis die
   drei schmalen Kabelbinder **zwischen** jeweils zwei LEDs verlaufen; keine
   LED oder Lötstelle unter einen Kabelbinder klemmen. Nur leicht festziehen.
   Lötanschlüsse und die drei Adern bleiben frei und führen durch die Mitte.
6. Drei Stahlmuttern von unten in den Ringhalter legen. Diffusor von innen
   in den Deckel einsetzen. Ringhalter mit drei M3 × 16 Stahlschrauben von
   außen montieren. Drei verbleibende Pfostenlippen tragen den Diffusorrand
   bei Z=83,6 mm; der Diffusor wird zwischen diesen Auflagen und dem Deckel
   gehalten und nicht verklebt.
7. LED-Kabel polrichtig gemäß elektrischer Montageanleitung anschließen.
   Vor dem Schließen auf Quetschstellen prüfen; LED-Kabel beim Öffnen nicht
   als Deckelscharnier belasten. Vier M3 × 12 Deckelschrauben einsetzen.

Zum Akkuwechsel Deckel und Träger lösen, Kabel abziehen und Klett öffnen.
Die Verkabelung muss das vollständige Herausheben des Trägers erlauben.

## Maße, Herkunft und Prüfumfang

- V50: [Hersteller](https://voltaicsystems.com/v50/) nennt 118 × 82 × 24 mm.
  Das am 31.10.2026 heruntergeladene [Hersteller-STEP](https://voltaicsystems.com/content/V50-AL_3D.stp)
  wurde mit FreeCAD gelesen: gültig, 19 Körper, 117,7 × 82 × 23,95 mm.
  SHA-256: `f3fa84fd27211b0f486d47434f4be1dd6e486575bef7f244f02871bd18a24303`.
  Ältere Händlermaße nennen 26 mm Höhe. Der Referenzkörper nimmt deshalb
  118 × 82 × **26** mm an, und das Fach besitzt mindestens 30 mm lichte Höhe.
  Die zwei USB-A-Anschlüsse liegen an einem kurzen Ende. Das Modell reserviert
  32 mm Steckerraum hinter dieser Stirnseite; die konkrete Kabelgehäuseform
  ist gesondert zu prüfen.
- Adafruit 1586: [Hersteller](https://www.adafruit.com/product/1586), außen
  65,5 mm, innen 52,3 mm, 3,2 mm hoch. Ringmulde außen 66,3 mm; Montage ist
  ohne Bohrung durch die LED-Platine vorgesehen. Farbe/LED-Typ können nach
  Herstellerlieferung variieren; dies ändert nicht die mechanische Teilenummer.
- ESP32-S3-DevKitC-1 v1.1: [Herstellerzeichnung](https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-DevKitC-1_V1.1_20220429.pdf),
  62,74 × 25,40 mm Basisplatine, ohne Antennenüberstand. Die vereinfachte
  Antennenhülle deckt den Überstand ab; die reale Passprobe bleibt erforderlich.
  N16R8 ersetzt N8R8 gemäß `../pcb/FERTIGUNG.md` ohne Änderung der Carrierpins.
  Die PCB-Schnittstelle stammt vom zuständigen PCB-Entwickler:
  Carrier 85 × 80 mm; Bohrungen (5,5), (80,5), (5,75), (80,75), Ø3,2 mm.
  Transformation Carrier → Gehäuse: **X=45+u, Y=137−v, Z-Unterseite=46 mm**.
- Der konservative äußere Metallfreiraum um den Antennenreferenzkörper beträgt
  15 mm. Akku und Metallbefestiger werden dagegen geprüft; die PCB-Befestiger
  müssen Nylon sein. Dies ist keine Funkabnahme und prüft nicht das eigentliche
  Leiterplatten-Kupferlayout, das dem Elektronikreview unterliegt.

`geometry-report.json` prüft gültige Volumenkörper, genau einen zusammenhängenden
Körper je Druckteil, STEP-Rücklesen, geschlossene STL-Netze, Selbstüberschneidungen,
nichtmannigfaltige Kanten, Orientierung, Bauraum, Baugruppen-/Referenzkollisionen
und externe Metallteile im Antennenfreiraum. Die aktuelle Prüfung liest den echten PCB-STEP ein und bindet dessen SHA-256. Fehlende Sicherungs- und optionale Sockelgeometrie sowie die maximale Kondensatorhöhe werden durch zusätzliche Hüllen geprüft. Andere Referenzkörper enthalten
maßhaltige Hüllen statt vollständiger Bauteilmodelle. Ein realer Stecker-/Kabel-
und Schraubenabgleich sowie der erste Testdruck bleiben erforderlich.

## Reproduzieren

```powershell
& 'C:/Program Files/FreeCAD 1.0/bin/python.exe' Hardware/naehe-glut/tools/build_enclosure.py
& 'C:/Program Files/Blender Foundation/Blender 4.0/blender.exe' -b --python Hardware/naehe-glut/tools/render_enclosure.py
```

Die editierbare Konstruktionsquelle ist `tools/build_enclosure.py`; maßliche
Änderungen dort regenerieren die nativen und exportierten Dateien gemeinsam.
Die Maßtabelle im gespeicherten FCStd dokumentiert den erzeugten Stand; sie ist
kein interaktives Spreadsheet mit automatisch abhängigen Features. STEP/FCStd
enthalten weiterhin bearbeitbare CAD-Körper. Blender-Beschriftungen und Farben
sind nur Vorschau, keine eingeprägte Beschriftung oder optische Simulation.
Automatische FreeCAD-Sicherungen (`*.FCStd.*`, `*.FCStd1`) und `.tmp` gehören nicht ins
Fertigungspaket.
