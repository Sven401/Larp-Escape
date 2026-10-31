# R1: Fertigungs- und Montagevertrag

Stand 31.10.2026; Prototyprevision. Native Quellen sind
`naehe-glut-r1.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, `NearGlow.kicad_sym`,
`NearGlow.pretty/` und die lokalen Bibliothekstabellen. Regenerierung erfolgt
aus `circuit.json` mit den Skripten unter `../tools/`; die CAD-Dateien bleiben
direkt in KiCad editierbar. Quellenänderungen nicht durch unbemerkte
Regenerierung überschreiben.

## PCB bestellen

- Menge: fünf unbestückte Platinen (zwei Geräte, drei Reserve/Erprobung).
- FR4, **zwei Kupferlagen**, nominal **1.6 mm**, außen **35 µm / 1 oz** Kupfer.
- 85 × 80 mm, geschlossener Umriss mit oberem Antennenausschnitt 20 × 15 mm.
- Lötstoppmaske beidseitig, Bestückungsdruck oben, bleifreies HASL oder ENIG.
- Mindestens 0.25 mm Abstand/Leiterbreite, 0.30 mm Mindestbohrung;
  tatsächlich Vias Ø0.8 mm mit Ø0.4 mm Bohrung. Fertiger muss mindestens
  20 µm durchgehende Hülsenkupferdicke für die Stromabschätzung gewährleisten.
- Mechanische Ø3.2-mm-Löcher **nicht metallisiert**, übrige Bauteil-/Via-Löcher
  metallisiert; separate PTH-/NPTH-Bohrdateien verwenden.
- Gerber enthalten F.Cu/B.Cu, F.Mask/B.Mask, F.Silkscreen/B.Silkscreen und
  Edge.Cuts. Bohrdateien und Umriss nicht durch Fotos oder SVGs ersetzen.
- Bestellvorschau des Fertigers gegen die exportierten CAM-Ansichten prüfen:
  85 × 80 mm, vier Montagebohrungen, Antennenausschnitt, zwei 22-polige Reihen.

Die genannten Gerber-/Bohrdateien gehören zusammen zu einer Revision. Änderungen
erfordern erneuten ERC, DRC mit Schaltplanparität und erneuten Export. Ein
bestandener Regelcheck ist kein physischer Last-/Funknachweis.

## Anschluss und Versorgung

Der ESP32-S3-DevKitC-1-N8R8 v1.1 sitzt mit Antenne zum Ausschnitt und Micro-USB
zum gegenüberliegenden Ende. J1 und J3 sind im Plan die gleichnamigen Reihen
des Espressif-Boards. Carrierkontakte zu J1: GPIO15/Pin8, GPIO16/Pin9,
GND/Pin22; J3: GND/Pin1,21,22. **Kein ESP-5V- oder 3V3-Pin ist verbunden.**

Das ESP-Board erhält ein eigenes USB-A→Micro-B-Kabel von der Powerbank.
Der zweite 5-V-USB-A-Port speist über ein USB-A→USB-C-Kabel J2. J2 ist nur
Stromeingang; CC1 und CC2 besitzen jeweils eigene 5.1-kΩ-Rd-Widerstände.
Der Carrier handelt kein USB-PD aus. Diese Revision ist für die benannte
5-V-Powerbank ausgelegt, nicht als universell stromverhandelndes USB-Gerät.
Benannte Quelle: Voltaic V50 USB-A-Port mit 5 V / 2 A, Kabel Delock 83326
(USB-A→USB-C, 0.5 m, Herstellerangabe 3 A / 24 AWG). Rd allein fordert
keine 2 A an. **Keinen PC-USB-Port als LED-Versorgung verwenden.** Beide
V50-Ausgänge zusammen dürfen deren 3-A-Grenze nicht überschreiten.
Für Service wird ausschließlich das USB-Kabel des DevKits zum PC umgesteckt.

J4: **1 = LED +5 V, 2 = DIN, 3 = GND**. DIN muss an den Eingang des
Adafruit-1586-Rings; DOUT bleibt frei. Mindestens AWG22 / etwa 0.34 mm² für
5 V und Masse, kurze Leitungen; Datenleitung mit benachbarter Masse führen.
Alle Leitungen am Carrier zugentlasten. Nicht unter Spannung umstecken.

F1 schützt ergänzend bei Fehlbedingungen, ist aber weder aktive 2-A-Begrenzung
noch garantierte Kurzschlussabschaltung einer strombegrenzten Powerbank.
Die Powerbank muss ihren eigenen Überstromschutz behalten. Niemals eine
leistungsstärkere Quelle allein anhand der Sicherungsaufschrift einsetzen.

## Firmware- und Einschaltvertrag

- GPIO15 liefert Daten für **24** Pixel; der alte 23-Pixel-Aufbau ist getrennt.
- GPIO16 bleibt beim Boot LOW/hochohmig. Q1 sperrt, R7 zieht /OE auf LED5V,
  R4 hält DIN bei deaktiviertem Buffer LOW.
- Nach Initialisierung des LED-Ausgangs GPIO16 aktivieren und einen vollständigen
  schwarzen 24-Pixel-Frame übertragen, bevor der Effekt freigegeben wird.
  Ein Frame bei gesperrtem Buffer erreicht den Ring nicht.
- Normales Effekt-Stoppen bleibt eine Firmware-Ausblendung. Das OE-Signal
  ersetzt weder Stop noch einen schwarzen Frame.
- **ESP-Stromverlust allein löscht bereits gelatchte Pixelfarben nicht.** Für
  Service beide USB-Zweige ausschalten/abziehen. Kein Hardware-Fail-Dark oder
  selbsttätiges Abschalten bei MCU-Ausfall wird für R1 behauptet.

SN74AHCT125N darf nach TI SCLS264R bei VCC=0 an seinen Eingängen das
ESP-Signal sehen (spezifizierter Eingangsleckstrom). Daraus folgt keine
allgemeine Toleranz fremd getriebener Ausgänge: Buffer und Ring liegen am
gleichen LED5V-Zweig; DIN darf keine externe Signalquelle treiben.

## Nach dem Bestücken

Bestückungshilfe für **C2 (100 nF)**: In `exports/assembly.svg` ist das kleine
unbeschriftete Rechteck unmittelbar oberhalb von U1 gemeint. Von oben gesehen,
Platinennullpunkt links oben, liegt C2-Pad 1 bei **(60,00; 26,00) mm** und Pad 2
bei **(62,54; 26,00) mm**. C2 ist ungepolt; Pad 1 führt LED5V, Pad 2 GND.

Vor Einsetzen des ESP Sicht-/Lötprüfung, Polung C1/U1/Q1/J4 und Durchgang
gegen den Schaltplan prüfen. Insbesondere keine Verbindung Carrier-LED5V zu
ESP-5V/3V3, keine Brücke zwischen den sechs J2-Kontakten. Danach
Stromversorgung, Ein-/Ausschaltfolgen, Volllast, Spannung am Ring, Wärme und
Funk im geschlossenen Gehäuse prüfen. Diese physischen Prüfungen sind offen.
