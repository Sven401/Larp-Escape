# NG-R1: unabhängiger Hardware- und Fertigungsreview

## Aktueller Beschaffungsnachtrag · 31.10.2026

Prüfer: `/root/supply_review`, unabhängig von Beschaffungsentscheidung und
Material-/Slicerumsetzung. Geprüfter Ausgangscommit:
`e9e2afe7b847f4ce8b9e44c8a2007274854c5df1`; der Nachtrag gilt für die unten
über SHA-256 gebundenen Arbeitsdateien.

**Ergebnis: N16R8 als dokumentierte Beschaffungsvariante und die neu geslicten
PETG-Basic-Druckdateien digital gegengeprüft; keine offenen Blocker dieser
Änderung.** Der folgende historische R1-Review bleibt Beleg seiner damaligen
Dateien. Seine PETG-HF-Projekte und abweichenden Paket-Hashes sind durch
diesen Nachtrag ersetzt. Physische Abnahme und Firmwareportierung bleiben offen.

Selbst ausgeführte Gegenprüfungen:

- Das originale N16R8 wird in der [Espressif-Produktübersicht](https://www.espressif.com/en/products/devkits/esp32-s3-series/esp32-s3-devkitc-1)
  ausdrücklich angeboten. Tabelle 1-1 im [WROOM-1-Datenblatt](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
  bestätigt 16 MB Quad-Flash und 8 MB Octal-PSRAM sowie dieselbe
  18,0×25,5×3,1-mm-Modulhülle wie N8R8. Die genutzten GPIO15/16 und GND,
  Micro-USB und PCB-Antenne wurden mit den offiziellen DevKitC-1-Anleitungen
  für [v1.1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)
  und [v1.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html)
  abgeglichen. Der Wechsel setzt keine neue Carrierverdrahtung voraus.
- Die [v1.1-Maßzeichnung](https://dl.espressif.com/dl/schematics/esp_idf/DXF_ESP32-S3-DevKitC-1_V1.1_20220429.pdf)
  als Bild gesichtet: 25,40 mm Breite, 1,27 mm Randabstand je Headerseite,
  daraus 22,86 mm Reihenabstand und 2,54 mm Raster. 62,74 mm bezeichnet
  die Basisplatine ohne Antennenüberstand. Die vorhandene Antennenhülle bis
  Y=143 deckt den Überstand zur Pin-1-Lage Y=135 ab; sie ist weiterhin ein
  vereinfachter Referenzkörper. Wareneingangs- und Passprüfung bleiben nötig.
  Der Händler-MPN wird keine unbestätigte v1.1-Lieferzusage zugeschrieben.
- Der noch erforderliche Firmwareauftrag nennt nun 16 MB Quad-Flash,
  8 MB Octal-PSRAM, tatsächliche Speichererkennung und Partitionierung.
  Es wurde kein N16R8-Build, Upload oder Gerätelauf durchgeführt; eine
  Speicher-/Pinprüfung ist kein Nachweis fertiger R1-Firmware.
- Materialauswahl, Profilauflösung und die drei geänderten Werkzeuge gelesen.
  Alle sechs STL-/3MF-Hashes gegen `slice-report.json` selbst berechnet;
  alle sechs 3MF geöffnet, CRC und enthaltenen G-Code-Hash geprüft.
  Tatsächlicher G-Code: fünf schwarze Teile mit Bambu PETG Basic für A1 mini,
  245 °C Düse, 70 °C Textured PEI, Dichte 1,25 und 13 mm³/s; Diffusor
  weiter PETG Translucent mit 250/245 °C und 6 mm³/s. Vier Wände und Gyroid
  bestätigt. Die warnungsfreien Sliceläufe stammen vom Materialentwickler.
- Aus den Ergebniswerten selbst addiert: 11,72 Stunden je Station,
  für beide 758,02 g Basic und 16,57 g Translucent einschließlich Passproben.
  Das [PETG-Basic-Datenblatt V3.0](https://store.bblcdn.com/s1/default/cb94589bf7994fdcbfa833badefae9cd/Bambu_PETG_Basic_Technical_Data_Sheet.pdf)
  bestätigt Dichte, 65 °C/8 h Trocknung und die Spule Ø200×67 mm.
  Der ergänzte S2 nimmt laut verlinktem Händler Ø210×85 mm auf und erreicht
  70 °C; die manuelle Einstellung auf 65 °C/8 h ist dokumentiert.
- Druck-ZIP selbst geöffnet: alle 26 Nutzdateien plus Manifest, CRC,
  SHA-256 und bytegenauer Vergleich zum Arbeitsbaum bestanden. PCB-Manifest
  mit 51 Dateien selbst nachgerechnet; alle elf PCB-ZIP-Einträge ebenfalls
  CRC-/bytegenau geprüft. Native PCB-, CAM-, STEP- und STL-Geometrie sind
  gegenüber dem Ausgangscommit unverändert. Deshalb keine erneute
  geometrische Berechnung oder ERC-/DRC-Ausführung behauptet.
- Bestellliste, Montage-/Druckanleitung und Terminansatz gegengelesen.
  Die Teilsummen 66,07 EUR bzw. 110,12 EUR einschließlich der bezeichneten
  Versandwege sind rechnerisch richtig. Bürklin-Livebestand stammt von
  `/root`; Welectron-Produktdaten wurden zusätzlich direkt abgerufen.
  Lieferanzeigen sind keine Reservierung. Bestellansatz 02.11., interner
  Eingangskontrollpunkt 13.11.2026 und sofortige Umplanung bei späterer
  Bestätigung schützen das Dezemberziel; der PCB-Rückfallweg bleibt erhalten.

Keine Bestellung oder Druckübertragung. Die physischen Prüfungen aus dem
historischen Review gelten weiter. Das Beschaffungsfenster ist plausibel,
aber weder Lieferzusage noch Nachweis zweier funktionierender Exemplare.

Aktuelle Revisionsbindung, Pfade relativ zu `Hardware/naehe-glut/`:

| Datei | SHA-256 |
| --- | --- |
| `BESTELLUNG.md` | `76e1dadab5cc20434d2d4c2f087d5f2542d02e18cbe6526051453016b6f4f19c` |
| `README.md` | `a4dfbb5f6448cf1eb4a260c1b50346231512efdb8fee6b54f4e8765e3663c458` |
| `pcb/FERTIGUNG.md` | `ab87bb793417aee2f3ee55cf998aa748d30df255d96dd47080e93e32a6994556` |
| `pcb/SHA256SUMS.txt` | `5293733ece92a6bf330c27fde513dca02fb9bdebad9e6fd05b7e21aeb12a47b2` |
| `pcb/exports/naehe-glut-r1-fertigung.zip` | `8c6bfef63651d85d21443767e3e80453d091eaf4883d107b8c68a9c78e2fa74b` |
| `fertigung/NG-R1-Druckpaket.zip` | `2d78a8af9b4b4f19a6be4ce1a8067fa800c50f26ae48b561d8b2351c2c3d0cb9` |
| `mechanik/DRUCK.md` | `ef247706b6c71c9c218f8482f917b849e1ebe340a3460daaf47f618be89df29f` |
| `mechanik/MONTAGE.md` | `db60ba73c2a653d4245edf8e53924b3888249d904ff191e71381f1fa318cd694` |
| `mechanik/slice-report.json` | `c55974c4b04a9c2af10d78876e3ca97466d3778d64dc1d3daf1b4aabbe6e647f` |
| `mechanik/slicer/petg_basic.json` | `42ec7d001f76c5c339803eb702d5e9cd7b7466bb2798771072f76149ab09b37c` |
| `mechanik/slicer/profile-sources.json` | `ac666e8afe78db0c94d5e30d5300fd11b5149110b9507f4362e80f93e0e4d364` |
| `tools/prepare_slicer_profiles.py` | `99e146d1fa436fc804d4b7bf2709d0daffd1fc9aee593d0cb61c37241a561efe` |
| `tools/slice_enclosure.py` | `e27b850b5fe9470bbd2ab2380ae15539c1c58c5e82ed44bdd9ec037fe1b8f82a` |
| `tools/package_prints.py` | `f638050f8a9fcdf10bd1999b2e4f62d5a95268eb75929ee3dd8b6f6e455828e6` |

## Historischer R1-Review vor N16R8-/PETG-Basic-Wechsel

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
