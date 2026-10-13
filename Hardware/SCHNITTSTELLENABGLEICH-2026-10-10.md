# Hardware-Schnittstellenabgleich vom 10. Oktober 2026

**Status:** geprüfter Quellenbefund; Revisionszuordnung und physische Abnahme offen.
**Paket:** `plan/hardware-revision-schnittstellenabgleich`, Basis
`e4af436167c253cb674db1c0038bdef99031535b` (`plan/hardware-revision-quelleninventar`).
**Umfang:** ausschließlich dieser Bericht, reproduzierbare Quellenauswertung und deren
JSON-Ergebnis. Die 15 Eingangsdateien bleiben bytegleich. Keine Pin-, CAD- oder
Firmwareänderung, kein Upload und kein physischer Test. Keine Fertigungsfreigabe.

Der Eingang enthält zwei Hardwareentwürfe, deren LED-/Audiozuordnungen von der
aktuellen Firmware abweichen. Das ist ein Konflikt zwischen Quellenständen. Eine
Ursache für die fehlgeschlagenen LED-/Audiobeobachtungen der Originalbox lässt
sich daraus noch nicht feststellen. Deren tatsächliche Revision und Kabelbelegung
sind nicht bestätigt. Firmware und App müssen diese Unsicherheit im Diagnosebericht
erhalten; ein vermeintlich korrigierter Pin darf nicht automatisch gewählt werden.

## Quellen und Revisionszuordnung

Die Dateihashes stehen in [QUELLENINVENTAR.md](QUELLENINVENTAR.md) und im neuen
[maschinellen Befund](pruefung/quellnetze-2026-10-10.json). Die Bezeichnungen
„alt“ und „V2“ unterscheiden hier Dateigruppen, keine abgenommenen Boxrevisionen.

| Eingang im Verzeichnis `Hardware/` | Frisch geprüfte Zuordnung | Grenze |
| --- | --- | --- |
| `SCH_Esp32-Breakout_2026-10-10.json` | EasyEDA 6.5.54, 34 Bauteile mit Pins, 148 Pins, 87 Wire-Objekte. | Geometrie und benannte Netze auswertbar; kein nativer CAD-Import/ERC. |
| `Sheet_1_2026-10-10.schdoc` | Protel Schematic Capture ASCII v5. Alle 34 Bauteil-UUIDs stimmen mit dem alten Breakout-JSON überein; ebenfalls 148 Pin- und 87 Wire-Records. | Starke Zuordnung zum alten Breakout; keine bestätigte CAD-Importierbarkeit. |
| `Schematic_Esp32-Breakout_2026-10-10.svg` | Alle 34 Bauteil-IDs des alten JSON finden sich im SVG. | Exportquelle zugeordnet; kein PCB-Nachweis. |
| `SCH_Gpio-Expander_2026-10-10.json` und `(1).json` | Bytegleiche EasyEDA-Quellen: 22 Bauteile mit Pins, 95 Pins, 93 Wire-Objekte. | Enthalten Anschlussleisten für das Expandermodul, keinen überprüfbaren MCP23017-Schaltungsaufbau. |
| `Sheet_1_2026-10-10 (1).schdoc` | Alle 22 Bauteil-UUIDs stimmen mit beiden Expander-JSONs überein; ebenfalls 95 Pins und 93 Wire-Records. | Dies ist anhand der Objekte dem Expander zugeordnet, keine identifizierte V2-Quelle. |
| `Schematic_Gpio-Expander_2026-10-10.svg` | Alle 22 Bauteil-IDs des Expander-JSON finden sich im SVG. | Exportquelle zugeordnet. |
| `Schematic_Esp-Breakout-V2_2026-10-10.svg`, `.png`, `(1).png`, `.pdf`, zugehörige BOM | Eigenständiger V2-Export. PNG visuell geprüft; Titelblock nennt Rev. 1.0 und Datum 2021-08-05. | Dateiname 2026-10-10 ist keine Revisionsfreigabe. Keine zugehörige editierbare V2-Quelle im Eingang identifiziert. |
| `Sheet_1_2026-10-10.svg` und `(1).svg` | XML parsebar; nicht über dieselben Bauteil-IDs zuordenbar. | Separate, noch nicht abschließend zugeordnete Exporte; keine V2-Quelle daraus ableiten. |
| Beide Breakout-BOMs | UTF-16, TSV. `LED-G4`/`WS2881` besitzt Footprint `TERMINAL SCREW 5.08 3P` und Manufacturer Part `Screw terminal 3p`. | Der Eintrag beschreibt einen Steckverbinder und bestätigt weder LED-Chip noch Farbfolge. Expander-BOM fehlt. |

Eine PCB-Quelldatei, CAD-generierte elektrische Netzliste, Gerber, Bestückungsdatei
oder reale Kabelbaumdokumentation ist im inventarisierten Eingang nicht vorhanden.
Die neue JSON-Auswertung ist eine nachvollziehbare geometrische Quellenauswertung,
keine vom CAD-System freigegebene Netzliste.

## Geprüfte Firmwarebasis

Verglichen wurden F0 `e973818edde56be4f4907a024f9c4f347802a3cc` und die getrennte
serielle Diagnosebasis `db52d7aa318a96e24e97e6b62194df644b9758e2`. Zwischen beiden
Ständen ist der Diff der folgenden vier Dateien leer: `src/main.cpp`,
`lib/Config/Config.h`, `lib/DFMinniHandler/DFMinniHandler.cpp` und
`src/GameConfiguration/StandardProfile.cpp`. F1 und ein künftiger Produktdiagnosepfad
werden damit nicht abgenommen. Die Firmwarequellen sind nicht Bestandteil dieses
Hardwarebranches; sie werden anhand dieser Commitanker referenziert.

Weitere ausgewertete Pfade bei `db52d7a`: `lib/MCPConfig/MCPConfig.h` und `.cpp`,
`lib/Keymatrixwrapper/Keymatrix.cpp`, `lib/NFCReader/NFCReader.cpp` und
`src/HardwareAcceptance/main.cpp`. `src/Backup` ist kein aktueller Pinvertrag.

| Funktion | Quellkonfiguration der Firmware | Bedeutung für Diagnose |
| --- | --- | --- |
| Wake 1–4 | MCP-Instanz 0, Adresse `0x20`; B7/B6/B4/B3 = numerische Pins 15/14/12/11. `INPUT_PULLUP`; HIGH wird aktiv interpretiert. | MCP-Pinnummern nicht mit ESP32-GPIO15/14/12/11 verwechseln. Physische Nummerierung und Polarität unbestätigt. |
| Matrix | 9×9, Zeilen 1–8 auf MCP1 (`0x21`) B7…B0; Zeile 9 auf MCP0 B0. Spalten A auf MCP0 B1, B…H auf A0…A6, I auf B5. LOW ist gedrückt. | Kabelbaum und reale Koordinaten fehlen; Hostzuordnung bestätigt kein erreichbares Feld. |
| Expanderbus | Standardprofil SDA21/SCL22, Adressen `0x20`/`0x21`; Laufzeit startet mit `Wire.begin()`. | Zwei erfolgreich erkannte Adressen bestätigen keine Wake-/Matrixverdrahtung. |
| LED | WS2811, GRB, GPIO15, 23 Indizes 0…22; FastLED-Budget 5 V/1000 mA. | Treiberannahme und Softwarebudget, keine Messung von LED-Typ, Farbe, Versorgung oder Strom. |
| DFPlayer | UART1 9600/8N1; ESP-RX25 und ESP-TX26. BUSY14, LOW=busy; WAKE13 mit HIGH/LOW-Folge. | BUSY und Wake benötigen gesonderte reale Leitungen; Sendeaufruf und BUSY allein bestätigen keinen hörbaren Track. |
| PN532 | Software-SPI SCK16, MISO17, MOSI23, SS4 über Vier-Pin-Konstruktor. Makros IRQ27 und RESET34 in `Config.h`. | Der Spielpfad weist aus diesen Makros allein keinen Anschluss oder aktiv gesteuerten Reset nach. Die serielle Probe liest IRQ27. |

## Pins, Busse und Versorgung: Abgleich der Entwürfe

Die alten JSON-Verbindungen wurden aus tatsächlichen Wire-Geometrien und
Pinanschlüssen ermittelt. Beispiel: `LED-G4` DATA, Pin 2 (`gge38352`, Position
635/-600) liegt im selben gezeichneten Netz wie `U3` Pin 26, Name `4`
(`gge31641`, Position 210/-640). Die Aussage beruht damit nicht allein auf dem
Designator. Pin 26 ist hier die Nummer am Entwicklungsboard-Symbol; der Pinname
`4` benennt den GPIO. Die V2-Spalte beruht auf dem sichtbar geprüften PNG-Export.

| Funktion | Alter Breakout: gezeichnete Verbindung | V2: sichtbarer Export | Konflikt/offene Frage |
| --- | --- | --- | --- |
| LED-DATA | `LED-G4:2 DATA` direkt zu `U3:26`, GPIO4. | DATA trägt Netlabel `G4`; derselbe Label liegt an ESP GPIO4. | Firmware GPIO15. Erst reale Platine, Datenleitung und LED-DIN zuordnen; kein Pinwechsel freigegeben. |
| LED-Versorgung | `LED-G4:1` +5V, gemeinsames Netz mit ESP `V5`, DFPlayer `VCC` und `5VIN:1`; Pin3 GND. | Pin1 +5V, Pin3 GND. | Reale Einspeisung, Rückleiter, Lastspannung, Strom und LED-Modul unbekannt. |
| DFPlayer RX | U1 Pin2 an ESP GPIO17; zusätzlich R3 zwischen diesem Netz und GND, Aufdruck `4.3`. | `G17` an RX, R3 nach GND. | Firmware ESP-TX26. R3 ist gezeichnet als Ableitung nach GND, kein nachgewiesener Serienwiderstand/Pegelwandler; Wert und tatsächliche Bestückung prüfen. |
| DFPlayer TX | U1 Pin3 an ESP GPIO16. | `G16` an TX. | Firmware ESP-RX25. GPIO16/17 werden in der Firmware für PN532 verwendet. |
| DFPlayer BUSY | U1 Pin16 ohne weitere angeschlossene Pins oder Netlabel; No-connect-Markierung im Export. | Pin16 mit No-connect. | Firmware erwartet GPIO14; gezeichnete Leitung fehlt. Realen Draht und unabhängigen Signalverlauf klären. |
| DFPlayer WAKE | IO1/IO2 und ADKEY1/2 ohne gezeichnete ESP-Verbindung. | IO1/IO2 und ADKEY1/2 mit No-connect. | Firmware toggelt GPIO13; Funktion, tatsächlicher Moduleingang und Verdrahtung bleiben unbestätigt. |
| Audioausgabe | U1 DAC_R/L an `DAC_R_L` Pins2/1, SPK1/2 an `SPK_1_2` Pins2/1; GND an beiden U1-GND-Pins. | Entsprechende DAC-/SPK-Klemmen dargestellt. | Lautsprecher-/Verstärkeranschluss und hörbare Tracks nicht freigegeben. |
| PN532 | Keine als PN532/RFID benannte Baugruppe; lediglich allgemeine GPIOklemmen. | RFID 2×4: 1 SCK, 2 MISO, 3 MOSI, 4 SS, 5 VCC, 6 GND, 7 IRQ, 8 RSTD. ESP-Quelle zeigt G4/G16/G17 für LED/Audio, keine Zuordnung dieser RFID-Netznamen zu ESP-Pins. | Kein belegter PN532-Stecker-/Pinvertrag. Firmware SS4/SCK16/MISO17 kollidiert mit den LED-/Audioentwurfsnetzen, falls diese tatsächlich umgesetzt sind. |
| Expanderbus | Alter Breakout besitzt allgemeine GPIOklemmen; kein benannter Expanderstecker. | `GPIOEXPANDER`: 1 GND, 2 VCC, 3 SDA, 4 SCL; ESP-GPIOzuordnung im Export fehlt. | Keine reale SDA21/SCL22-Zuordnung oder Versorgungshöhe bestätigt. |

Der Expander-JSON zeichnet CN1/CN2 mit Pin1 GND, Pin2 VCC, Pin3 `SDA/SI`,
Pin4 `SCL/SCK`. Alle 16 A-/B-Klemmen führen Pin1 nach GND, Pin2 zum jeweiligen
A-/B-Signal und Pin3 nach VCC. Beispielsweise B7/B6/B4/B3 Pin2 führen zu U2
Pin20/18/14/12. Das dokumentiert die gezeichneten Modulanschlüsse, keine
bestätigte Zuordnung zu realen MCP23017-Beinchen oder Wake-Tastern.

Ein weiterer Anschlusskonflikt ist die nummerierte Interrupt-/Resetleiste:

| Kontakt | Expanderquelle `CN3` | V2-Breakout `CN2` | Konsequenz |
| --- | --- | --- | --- |
| 1 | ITB | ITA | Ein gerades Kabel wäre anhand dieser Entwürfe A/B-vertauscht. Reales Kabel unbekannt. |
| 2 | ITA | ITB | IRQ-Bedeutung und Firmwareeinsatz getrennt verifizieren. |
| 3 | RESET | IORESET | Labelnamen bestätigen keine gemeinsame Verbindung zwischen getrennten Zeichnungen. |

`SW1` der Expanderquelle verbindet seine drei Eingangsseiten mit VCC und die
anderen Seiten mit `Adr2/Adr1/Adr0`. Eine vollständig prüfbare interne Modulschaltung
mit Widerständen und Adressdekodierung fehlt. Stellung und tatsächliche Adressen
0x20/0x21 sind am bestückten Aufbau zu belegen. Keine Klemme erhält allein durch
das Label VCC eine bestätigte Spannung oder Pegelverträglichkeit.

## Annahmen für den Diagnosevertrag

**Sicher als Quellkonfiguration:** vier logische Wake-Kanäle mit obiger MCP-Zuordnung,
23 LED-Indizes und begrenzter RGB-Anforderung; Ausgabeparameter WS2811/GRB/GPIO15;
BUSY LOW-Interpretation und UARTparameter; PN532-Transportparameter. Diese Angaben
müssen als gemeldete Konfiguration mit Firmwarestand dokumentiert werden.

**Unbestätigt als Hardwareeigenschaft:** installierter LED-Typ/Farbfolge, echte
LED-Positionen, physische Wake-Polarität und Nummerierung, Kabel-/Steckerorientierung,
reale Spannungen/Strombudget, verfügbare Versorgungstelemetrie, Audio-/NFC-Funktion.
Die untersuchten Quellen zeigen keinen angeschlossenen Spannungs-/Strommesspfad.
Fehlende Telemetrie muss eine fehlende Fähigkeit bleiben, kein fiktiver 5-V-Messwert.

Der Wake-Bericht soll Kanal, rohen Pegel, konfigurierte Aktivinterpretation und
Zeit/Flanke erhalten. Erst Nutzerbeobachtung ordnet eine Betätigung dem Kanal zu.
Ein mit Pull-up offener Eingang kann als HIGH gelesen werden; das darf keine
erfolgreiche Betätigung oder richtige physische Polarität vortäuschen.

Beim LED-Schritt stehen korrelierte Firmwareausführung, physische Nutzerbeobachtung
und optionaler Messwert nebeneinander. `show()`-Dauer, gerenderte Frames und
Software-Leistungslimit belegen keine sichtbare Farbe. Fehlende Versorgungstelemetrie
und fehlende Beobachtung bleiben explizit offen. Für Audio müssen Trackauftrag,
BUSY-Verlauf und hörbare Beobachtung getrennt bleiben; ein fehlender BUSY-Anschluss
darf nicht als „Track fehlt“ diagnostiziert werden. NFC startet nur mit Lesen;
Typ/Kapazität und Schreibbereichschutz bleiben vor Schreibfähigkeit erforderlich.

## Offene Mess- und Revisionsfragen

Diese Fragen sind Anforderungen an einen späteren eigenständig bedienbaren
App-/Firmware-Ablauf und dessen exportierten Endbericht. Diese Sitzung führt keine
Nutzerhandlungen oder Geräteprüfung an. Der unterbrochene Originalboxlauf bleibt
unterbrochen.

| Frage | Benötigter Beleg | Derzeitiger Status |
| --- | --- | --- |
| Welche Hardwarequelle beschreibt die Box? | Fotos/Bestückungsidentität und nachvollziehbare Leitungs-/Steckerzuordnung mit Revisionskennung. | Offen; Dateiname/Titelblock genügt nicht. |
| Was erreicht LED-DIN? | Zugeordnete Leitung ab ESP-Pin, tatsächliche Wellenform am ESP und ersten LED-Eingang, LED-Typ und Farbfolge; konkrete Testmittel im Export. | Kein Messwert. Vorheriger Sichttest fehlgeschlagen, Ursache offen. |
| Welche Versorgung liegt tatsächlich an? | Spannung am ESP, LED-Eingang und DFPlayer im Leerlauf und während begrenztem Lastschritt; Einspeise-/GND-Pfad, Versorgungseinheiten und Messzeit. | Keine Spannungs-/Strommessung. Softwaresoll 5V/1A ersetzt diese nicht. |
| Welche Wake-Taste ist welcher Kanal? | Je Kanal drei eindeutige Betätigungen mit Drücken/Loslassen, Rohpegel und Nebenauslösungen; Kombinationen. | Einzeltest nicht durchgeführt. |
| Stimmen DF-UART, BUSY und WAKE? | Reale GPIO-/Modulanschlüsse, SD-/Trackidentität, separate Versorgungsbeobachtung, BUSY-Flanken und hörbares Ergebnis. | Track100 mit Versorgung ohne Ton/BUSY-Verlauf; Ursache offen. |
| Stimmen PN532-Bus/Mode und Reset/IRQ? | Reale Steckerkontakte/Modus, zeitgebundene Leserantwort, UID/Typ/Kapazität und später geschützter Schreib-/Rücklesepfad. | Wechselnde Leserbereitschaft, kein belegter Tag-Schreibauftrag. |
| Welcher CAD-Stand ist freigabefähig? | V2-Quelle oder bestätigte aktuelle Revision, PCB, CAD-Netzliste, BOM/Moduldetails; native ERC-/DRC-Ergebnisse mit Werkzeugversion. | Quellen/Prüfungen fehlen; keine Fertigungsfreigabe. |

Der historische Zwischenbefund stammt aus der kanonischen KI-Werkbank
`docs/hardware/reichstagsbox-sitzungspruefung-2026-10-10.md`; Produktbeleg
`docs/hardware-acceptance-2026-10-10.md` bei `db52d7a`. Er ist eine geprüfte
Auswertung älterer Rohbelege, kein neuer Hardwaretest dieses Berichts.

## Prüfung und Reproduktion

Aus dem Paket-Worktree:

```powershell
python Hardware/pruefung/extract_source_nets.py | Set-Content -Encoding utf8 Hardware/pruefung/quellnetze-2026-10-10.json
git diff --check
```

Der Auswerter verwendet nur Python-Standardbibliotheken. Er parst drei JSONs,
zwei ASCII-SCHDOCs, fünf SVGs und zwei UTF-16-BOMs; SHA-256 und Bytezahlen werden
erneut ermittelt. Er verbindet Wire-Segmente an Endpunkten, expliziten Junctions,
Pinanschlüssen und Flagpositionen, sowie gleich benannte Flags innerhalb einer
Zeichnung. Kreuzungen ohne solchen Kontakt werden nicht verbunden. Alle
Pin-/Bauteil-IDs und Koordinaten bleiben im Ergebnis überprüfbar. Die Native-CAD-
Semantik von versteckten Pins, Hierarchie und elektrischen Pinarten wird nicht
validiert. SCHDOC wird strukturell geprüft; seine Netzsemantik wird nicht dekodiert.

Frisch durchgeführt: deterministische Wiederholung der Auswertung; Hashabgleich
aller zwölf maschinell gelesenen Quellen gegen das Quellenmanifest und Git-Basis;
34/34 bzw. 22/22 UUID-Zuordnungen; gezielte Prüfung der gezeichneten LED-, UART-,
BUSY-, Versorgungs- und Expandersteckernetze; visueller V2-PNG-Abgleich und leerer
Diff der vier benannten Firmwaredateien zwischen F0 und serieller Diagnose.
Der gestagte Paketdiff wird mit `git diff --cached --check` vor dem Commit geprüft.
Die Befehlsauflösung fand `kicad-cli`, `altium` und `dxp` nicht im PATH; daraus
folgt keine Aussage über alle installierten GUI-Anwendungen. Native CAD-Werkzeugprüfung,
ERC, PCB-DRC, Firmwarebuild und physischer Test wurden nicht durchgeführt.

**Nächster Schritt:** Revisionsidentität und aktuelle editierbare V2-/PCB-Quellen
klären; Diagnosevertrag mit den ausdrücklich unbestätigten Hardwareeigenschaften
weiterführen. Ein endgültiger Hardwarevertrag benötigt die bestätigte reale
Revision und später die vom Nutzer abgeschlossene App-Abnahme.
