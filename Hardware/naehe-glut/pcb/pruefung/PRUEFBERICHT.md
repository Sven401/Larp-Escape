# Nähe Glut R1 – digitale PCB-Prüfung

Stand 31.10.2026, KiCad 10.0.8, Arbeitsbranch `codex/naehe-glut-hardware-r1`.
Prüfgegenstand sind die Dateien im SHA256-Manifest, noch kein gebautes Gerät.

- ERC: 0 Fehler, 0 Warnungen (`erc.rpt`).
- DRC einschließlich Schaltplanparität: 0 Verstöße, 0 offene Verbindungen,
  0 Footprintfehler (`drc.rpt`). Mindestabstand 0,25 mm.
- Nativer Netzexport gegen PCB: alle 95 Pad-/Netzpaare gleich; ESP-5V und
  ESP-3V3 bleiben isoliert (`netlist-and-power.json`).
- Unabhängiger CAM-Rückimport mit gerbonara 1.6.3: sieben Grafiklagen,
  99 PTH-Bohrungen einschließlich Vias und vier NPTH-Bohrungen erkannt.
  `cam-top.png` und `cam-bottom.png` entstehen aus den tatsächlichen Gerbern
  einschließlich Bohrdaten, nicht aus dem PCB-Editor.
  Konturmittelpunktmaße 85 × 80 mm; Bounds inklusive 0,05-mm-Konturstrich
  85,05 × 80,05 mm. Die Antennenaussparung gehört zur Außenkontur.

Der CAM-Leser meldet bei KiCads Excellon G90 nach Headerende und überzählige
RoundRect-Makroparameter; Dateien werden vollständig eingelesen. Fehlende
Paste-Lagen sind für dieses unbestückte Fertigungspaket beabsichtigt.
Für eine Schablone/automatische Bestückung ist ein gesonderter Auftrag nötig.
Die Standard-DRC-Ignoreliste ist keine projektspezifische Befreiung:
fehlender Courtyard, nicht zentrierte Via-Endpunkte, Tuningprofile und
Symbolfilter werden nicht als elektrische Freigabe verwendet. Montagevolumen
und Abstände sind zusätzlich im mechanischen Prüfbericht zu bewerten.

## USB-C – Herstellerzeichnung und Pinfolge

GCT USB4135-GF-A, Zeichnung Revision A1, 22.11.2022, Blatt 1.
Hersteller: <https://gct.co/connector/usb4135>; verwendete Zeichnungskopie:
<https://www.avelmak.sk/media/dsc_items/91625/usb4135.pdf>.
SHA256 der geprüften PDF:
`76de44a17437b7317b26e06a78bd7c8d0b77430181ec8105e6815411d43c4f07`.
Die lokale Prüfkopie liegt in `.tmp/USB4135-drawing.pdf` (nicht Teil der
Fertigungsunterlagen). Unabhängiger Reviewer hat die Zeichnung ebenfalls gelesen.

Verglichen mit dem gebündelten KiCad-Footprint: Kontaktfolge links nach rechts
B12/GND, B9/VBUS, A5/CC1, B5/CC2, A9/VBUS, A12/GND. Paarabstände
5,50 / 3,04 / 1,00 mm; Kontaktpadbreiten 0,80 / 0,76 / 0,70 mm,
jeweils Länge 1,20 mm. Shell-Ankerabstand quer 10,25 mm, längs 3,58 mm.
Gehäuse 8,94 × 6,90 mm, Höhe 3,22 mm. Zeichnung und Footprint stimmen überein.
Herstellerstromangabe 3 A für VBUS gesamt; zwei getrennte CC-Rd 5,1 kΩ.
Die zulässige konkrete Stromquelle und Servicefolge stehen in `FERTIGUNG.md`.

## Strompfad: DC-Rechnung ist kein thermischer Versuch

Die Rechnung in `check_pcb_contract.py` erfasst auch die kurzen **0,4-mm-USB-
Fanouts und 0,4-mm-Bohrungs-Vias**, nicht nur die 1,2-mm-Versorgungsbahnen.
Annahmen: Kupfer 35 µm, Via-Wand 20 µm, Platinendicke 1,6 mm, 20 °C.
Konservativ wird je eine durchgehende VBUS-/GND-Verbindung mit vollem Strom
gerechnet; Parallelkontakte verbessern die tatsächliche Verteilung.

USB bis Sicherung 0,01551 Ω, Sicherung bis LED 0,03132 Ω,
LED-Masse zurück USB 0,04363 Ω; zusammen **0,09046 Ω**.
Damit etwa 0,136 V Kupferabfall bei 1,5 A bzw. 0,181 V und 0,362 W bei 2 A.
Sicherung, Stecker, Kabel und Temperaturanstieg sind darin nicht enthalten.
Diese Rechnung ersetzt keine thermische Freigabe: am bestückten Prototyp
Spannung, Strom und Erwärmung bei Volllast und geschlossenem Gehäuse messen.
Die 2-A-Sicherung ist weder aktive Strombegrenzung noch garantierte Abschaltung
an einer strombegrenzenden Quelle. V50-Port 2 A, beide Ports zusammen 3 A.

## Mechanik und offene physische Abnahme

Finaler STEP SHA256:
`142e65eeb9bf46b686526d12e6fadaa2a7eabe060a8cecdd4da1308241c7ec7c`.
Er enthält die bestückte Platine einschließlich maßlicher C2-/F1-Hüllen;
das DevKit wird in der Gehäuseprüfung zusätzlich als Hülle eingesetzt.
Nominalbestellung 1,6 mm; STEP-FR4-Körper 1,51 mm plus Kupfer/Maskenlagen.
C1 16 mm, F1-Hülle 9 mm, C2-Hülle 7,36 mm; U1 direkt eingelötet, kein
zusätzlicher IC-Sockel. Vier M3-Montagen ausschließlich PA/Nylon.

Offen bleiben reale Löt-/Polungsprüfung, Versorgung unter Last, Start-/Stop-
und Abziehfolgen, Funk, Dauerlauf, mechanischer Probebau und zwei funktionierende
Geräte. Der unabhängige Review wird separat dokumentiert; digitale Prüfungen
sind keine behauptete Hardwareabnahme. Der Dezembertermin behält Vorrang.
