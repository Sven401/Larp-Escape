# ESP32–MCP23017-I²C-POC, Entwurf POC-R0

**Stand 2026-10-10:** elektrischer Vorentwurf, nicht gefertigt und nicht fertigungsbereit. `poc-r0.kicad_pcb` ist eine editierbare KiCad-5.1-PCB-Datei (Formatversion `20171130`) mit 19 Footprints, 116 Pads, 30 benannten Netzen und geschlossenem 110 × 80-mm-Umriss. **Sie hat null Leiterbahnen und Platzhalter-Footprints für SW1–SW3.** `netzliste.csv` ist eine von Hand erstellte Soll-Netzliste, kein CAD-Export. Die SVGs sind editierbare Vektoransichten, keine CAD-Schaltpläne. Es gibt noch keinen nativen EasyEDA-Pro-Schaltplan und kein ERC-Ergebnis. Der EasyEDA-DRC des importierten PCBs meldete 88 offene Verbindungen.

## Quellenbasis und Abgrenzung

- Produkt-Hauptcheckout `main` / `29da937c4309beddfd45c5dd0fbf84fdf853d8e4`, am 2026-10-10 geprüft. Die 15 älteren Dateien in `Hardware/` sind unveränderte Eingangsquellen; insbesondere ihre Beschriftungen belegen keine reale Originalbox-Verdrahtung.
- Vergleich: `.hardware-interface-worktree/Hardware/SCHNITTSTELLENABGLEICH-2026-10-10.md` auf `plan/hardware-revision-schnittstellenabgleich` / `853dd06423d52eec30d93f394b21280903c128d1`; Firmware-Pins in `.hardware-diagnostic-worktree/src/GameConfiguration/StandardProfile.cpp` und `lib/MCPConfig/MCPConfig.cpp` auf `db52d7aa318a96e24e97e6b62194df644b9758e2`. Der POC bildet **einen** Expander ab, nicht die zwei Expander der Originalbox.
- [Espressif DevKitC V4, Steckerbelegung und Versorgung](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html), [ESP32-WROOM-32E](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.html), [ESP32-Hardware-Designrichtlinien](https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32/index.html), [Microchip MCP23017 DS20001952D](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf).
- EasyEDA Pro V3.2.149 ist als lokale GUI verfügbar. Am 2026-10-10 wurde `poc-r0.kicad_pcb` über **Import > KiCad** in ein EasyEDA-Pro-Projekt `poc-r0` importiert. Umriss, Footprints und Ratlines waren in der PCB-Ansicht sichtbar. Der Import ist keine vollständige elektrische oder mechanische Validierung. Die Bridge-Erweiterung v0.1.1 wurde ebenfalls importiert, ihre externe Interaktionsberechtigung ist jedoch nicht freigeschaltet; `easyeda_doctor` meldete deshalb `disconnected` am lokalen Port 18765. Eine CLI wurde nicht gefunden. Das [V3-Dateiformat](https://prodocs.easyeda.com/en/format/index/) ist ein projektgebundenes Logformat; die kanonische PoC-Quelle bleibt bis zu einem geprüften nativen Export die KiCad-Datei.

## Architekturentscheidung

X1 ist **ESP32-DevKitC V4 mit ESP32-WROOM-32E**, als steckbare Baugruppe auf zwei 19-poligen Buchsenleisten. Der DevKit übernimmt USB, Spannungsregler, USB-UART und seine vorhandenen EN-/BOOT-Funktionen. Diese Entscheidung vermeidet im POC ein ungeprüftes eigenes HF-Layout für das WROOM-Modul. Ein beliebiges als „ESP32 DevKit“ verkauftes Board ist kein freigegebener Ersatz: Steckerabstand, Bestückungsseite und USB-Überstand sind vor dem PCB-Footprint gegen genau das beschaffte DevKit und Espressifs Maßzeichnung zu prüfen.

X1 wird ausschließlich über seinen USB-Anschluss versorgt. Der 5-V-Stift J2/19 des DevKit bleibt unbeschaltet. U1 (MCP23017-E/SP, 28-poliges SPDIP) erhält 3,3 V von X1 J2/1. Keine externe 3,3-V-Einspeisung, kein 5-V-I/O und keine Leistungslast an den GPIO-Klemmen. Der verfügbare 3,3-V-Strom des konkret beschafften DevKit und die Last des späteren Aufbaus sind noch nicht gemessen oder budgetiert; die POC-Klemmen sind nur für hochohmige Logik-/Messanschlüsse vorgesehen.

SDA = ESP GPIO21, SCL = GPIO22, beide mit je 4,7 kΩ nach 3,3 V. Das entspricht der geprüften Firmware-Konfiguration. U1 A2/A1 liegen auf GND; A0 hat 10 kΩ Pulldown. Ohne Shunt JP1: 0x20; mit JP1 nach 3,3 V: 0x21. JP1 darf nur spannungslos umgesteckt werden. Der Firmwarestand der Originalbox erwartet gleichzeitig 0x20 und 0x21; ein Lauf dieser Firmware auf diesem POC würde daher keine vollständige Box simulieren.

SW1 zieht X1 EN nach GND (Reset), SW2 zieht GPIO0 nach GND (Boot), SW3 zieht U1 RESET nach GND. R3/R5/R6 sind je 10 kΩ Pull-ups; R5/R6 ergänzen die DevKit-Beschaltung und müssen gegen das genaue Board-Schaltbild geprüft werden. C1 100 nF direkt an U1 VDD/VSS, C2 10 µF an der lokalen 3,3-V-Schiene. Keine Interruptleitung ist zum ESP verdrahtet; INTA/INTB sind nur auf J5 herausgeführt. Sie brauchen vor Nutzung eine definierte elektrische Konfiguration und bei Open-Drain-Betrieb einen Pull-up.

## Pin-/Bus-/Versorgungsvertrag POC-HW/0.1-draft

| Signal | Quelle → Ziel | Pegel / Richtung | Status |
| --- | --- | --- | --- |
| +3V3 | X1 DevKit J2/1 → U1/9, Pull-ups, C1/C2 | 3,3 V, USB-abgeleitet | Entwurf; Stromreserve offen |
| GND | X1 J2/14 und J3/1,7 → U1/10, Taster, C1/C2 | gemeinsamer Rückleiter | Entwurf |
| SDA | X1 J3/6 GPIO21 → U1/13 | I²C, Open Drain, 4,7 kΩ nach 3,3 V | Quellkonfiguration abgeglichen |
| SCL | X1 J3/3 GPIO22 → U1/12 | I²C, 4,7 kΩ nach 3,3 V | Quellkonfiguration abgeglichen |
| EN / BOOT | X1 J2/2 / J3/14 → SW1 / SW2 nach GND | aktive LOW-Taster; GPIO0 ist Strap-Pin | nur POC, nicht Boxbeleg |
| MCP_RESET | U1/18 → SW3 nach GND, R3 nach 3,3 V | aktiv LOW | Entwurf |
| A0/A1/A2 | U1/15 /16 /17 | 0x20 oder 0x21 per JP1; A1=A2=LOW | Entwurf |
| GPA0…GPA7 | U1/21…28 → J3/1…8 | 3,3-V-GPIO | GPA7 laut Microchip nur als Ausgang einplanen |
| GPB0…GPB7 | U1/1…8 → J4/1…8 | 3,3-V-GPIO | GPB7 laut Microchip nur als Ausgang einplanen |
| INTA/INTB | U1/20 /19 → J5/1 /2 | noch kein Pull-up/ESP-Eingang | Fähigkeit offen |
| ESP-GPIO23/19/18/17 | X1 J3/2/8/9/11 → J6/1…4 | 3,3-V-GPIO | POC-Breakout; GPIO17 nur bei WROOM-Variante |

J5/3 = 3,3 V, J5/4 = GND; J6/5 = 3,3 V, J6/6 = GND. J3/J4 meinen die neuen POC-Ausgänge GPA/GPB; „X1 J3“ meint die DevKit-Buchsenleiste. Die Namen sind bewusst verschieden qualifiziert. GPIO6–11 des ESP sind Flash-Pins und werden nicht herausgeführt. GPA7/GPB7 sind nach Microchip-DS20001952D für MCP23017 als **Output only** gekennzeichnet: Für die Originalbox-Wake-Zuordnung auf GPB7 ist das ein offener Bauteil-/Firmwarekonflikt, kein hier stillschweigend gelöster Fehler.

## BOM, Netzliste und PCB-Ableitung

- `netzliste.csv`: vollständige Soll-Verbindungen der bestückten Teile; Semikolontrenner, UTF-8. `NC`-Zeilen bezeichnen absichtlich unbeschaltete Pins. Sie wurde gegen die obige Tabelle manuell verglichen, nicht aus einem CAD-System exportiert.
- `stueckliste.csv`: Soll-BOM mit offenen Beschaffungs-/Footprintfragen. X1 zählt als bestückte DevKit-Baugruppe; seine aufgelöteten Teile werden nicht doppelt bilanziert.
- `poc-r0.kicad_pcb`: editierbarer KiCad-5.1-PCB-Entwurf mit Pads, Netzen und Umriss; 0 Segmente/Vias/Zonen. Der DevKit-Buchsenabstand ist im Entwurf 25,4 mm, entsprechend [Espressifs Maßblatt](https://dl.espressif.com/dl/schematics/esp32_devkitc_v4_dimensions.pdf); die Orientierung und reale Passung des konkret beschafften Boards bleiben ungeprüft. Die Gehäuseumrisse der Buchsen und Taster sind vereinfachte Platzhalter, keine freigegebenen Bibliotheksfootprints.
- `schaltbild.svg` und `pcb-platzierung.svg`: editierbare Vektoransichten für Schaltungsabsicht und Anordnung. Die PCB-Skizze enthält keine fertigungstauglichen Daten; die KiCad-Datei ist die maßgebliche PCB-Quelle.
- `pruefe_quellen.py`: syntaktischer Abgleich von KiCad-Pads, Soll-Netzliste, CSV und SVG; **kein CAD-Parser und keine DRC**. Vor einem Layoutabschluss muss ein PCB-Designer die DevKit-Mechanik, U1-Footprint und Steckerausrichtung im CAD prüfen, einen nativen Schaltplan anlegen, den PCB routen und ERC/DRC ausführen.

## Prüfstand und Freigabetor

Durchgeführt: Vergleich der offiziellen Pinbelegungen mit Firmware-Konfiguration und Quellenbericht; Abgleich der aktuellen Android-Diagnosequelle `.diagnose-android-fake-worktree/companion/android/DIAGNOSTIC-V1.md` (synthetischer Diagnosepfad, keine POC-Fähigkeit); `python Hardware/POC-EasyEDA-ESP32-MCP23017/pruefe_quellen.py` erfolgreich: 19 Footprints, 116 Pads, 88 zugeordnete Pads identisch zur Soll-Netzliste, 31 Netzeinträge, 12 BOM-Zeilen, zwei parsebare SVGs. Der Test prüft nur Struktur und Zuordnung. EasyEDA-Pro-KiCad-Import gelang; der dortige DRC zeigte **88 Connection Errors** (offene Verbindungen) und keine darüber hinaus angezeigte Fehlerklasse. Nicht durchgeführt: ERC, Routing, mechanische Passprobe, USB-/3,3-V-Messung, I²C-Kommunikation, GPIO-Last- oder physischer Funktionsnachweis. **Fertigungsfreigabe: nein.** Die unterbrochene Originalbox-Prüfung bleibt unberührt. Ein späterer Nutzerlauf muss über App/Firmware geführt und erst nach Abschluss anhand des exportierten Berichts ausgewertet werden.

## Versions- und Änderungsnachweis

| Version | Datum | Änderung | Beleg-/Freigabestand |
| --- | --- | --- | --- |
| POC-HW/0.1-draft | 2026-10-10 | Eigenständiger 3,3-V-DevKitC-V4/Ein-MCP-POC mit 0x20/0x21-Jumper, EN/BOOT/MCP-Reset und GPIO-Ausgängen; KiCad-5.1-PCB mit Pads/Netzen/Umriss | EasyEDA-KiCad-Import und Strukturabgleich bestanden; 88 offene DRC-Verbindungen; ungeroutet, kein ERC oder Gerätetest; keine Firmware-/App-Implementierung |

Bei einer späteren Pin-, Pegel-, Adress-, Strom- oder Fähigkeitsänderung diese Tabelle und die Netzliste zusammen versionieren und die dann aktuellen Firmware- und App-Quellen erneut abgleichen. Der Vertrag beschreibt nur den POC und darf nicht als Änderung der Originalbox-Konfiguration übernommen werden.
