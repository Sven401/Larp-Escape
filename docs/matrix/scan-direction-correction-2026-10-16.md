# Tastenmatrix: ursprüngliche Scanrichtung wiederhergestellt

Stand: **16. Oktober 2026, Europe/Berlin**. Status: **gebaut und hostseitig
geprüft; nicht installiert, Geräteabnahme offen**. Basis:
`50effd812bb8f2ac30031aa836936a8efd7086ba`, Branch `codex/matrix-test-preparation`,
[Draft-PR #49](https://github.com/Sven401/Larp-Escape/pull/49).
Die [maschinenlesbaren Belege](evidence/scan-direction-correction-2026-10-16.json)
binden die gebauten Quellen und Binärdateien über SHA-256.
Quellstand ist der Commit dieser Korrektur; spätere Änderungen benötigen neue Prüfungen.

## Anlass und Umsetzung

Der [bisherige Gerätetest](device-test-2026-10-16.md) erkannte zwei bewusst
betätigte Reedkontakte nicht. **Belegt durch Quellvergleich** mit `61ef6c6`
und `001eb7b`: Der Testscanner hatte die historische Stromrichtung umgekehrt.
Der Nutzer beauftragte am 16.10.2026 in Sitzung
`01a141f2-ef84-7a63-b492-6b1fa2e9d26e` die Korrektur nach ursprünglicher
Scanrichtung und Pinbelegung. Der genau seinerzeit live gelaufene Binärstand
ist weiterhin nicht bestimmt.

`MatrixInputs` schaltet A–I an `0x20 B1/A0–A6/B5` einzeln LOW und liest
Zeilen 1–8 an `0x21 B7–B0`, Zeile 9 an `0x20 B0` mit Pull-up. Inaktive
Spalten bleiben hochohmig: bewusster Unterschied zum abschließenden HIGH des
Originals, bei gleicher Dioden-Stromrichtung. Wake- und fremde Registerbits
bleiben erhalten. Einschwingzeit, Busfehler, Resetprüfung und Altersgrenze gelten
weiter. Erst neun vollständig gelesene Spalten ergeben eine neue Probe.
Funkreihenfolge `row * 9 + column` und App-Protokoll bleiben unverändert.

**Nutzerbestätigt am 16.10.2026:** Einige Kontakte fehlen absichtlich wegen
Platzkollisionen mit anderen Baugruppen. Das Raster umfasst 81 logische
Kandidaten, keine behaupteten 81 realen Kontakte. Die tatsächlichen Lücken
sind noch nicht zugeordnet. Keine komprimierte Nummerierung und keine automatische
Einstufung einer leeren Messung als „nicht bestückt“ oder „defekt“.

## Historischer IO7-Prüfmodus

Die alte Belegung liest auch `0x21 B7`. Das ist ausdrücklich **keine Lösung des
MCP23017-Datenblattkonflikts**: [Microchip DS20001952D, Seite 18, Abschnitt 3.5.1](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf)
fordert für IO7 die Ausgangskonfiguration. Chipidentität und Eignung an der
realen Box bleiben offen, ebenso die getrennte Wake-B7-Frage.

`larp-matrix-test` setzt zusätzlich `LARP_MATRIX_LEGACY_IO7_INPUT=1`.
Ohne ausdrückliches Opt-in wird ein Matrix-Testbuild abgewiesen.
`larp-led-product` startet weiterhin keinen Matrixscanner. Eine gültige Probe
bezeichnet einen vollständigen frischen Registerscan im historischen Prüfmodus,
keine Hardware- oder Datenblattabnahme. B7 wird nicht ausgeblendet; es gibt
keinen als Vollmatrix ausgegebenen Teilscan.

## Gemessene Softwareprüfungen

| Prüfung | Ergebnis |
|---|---|
| LED-/SDK-/Wake-/Matrix-Hostsuite | 16 LED-, 4 SDK-Fälle sowie Wake/I²C und Matrix bestanden |
| Gerichtetes Diodenmodell | Alle 81 logischen Einzelkoordinaten, unbestückte Beispielpositionen, Verpolung, Lösen und Mehrfachkontakte bestanden |
| Gezielter Alt-/Neuvergleich | Gleicher geschlossener A1-Diodenzweig: Scanner aus `50effd8` verfehlt ihn, Korrektur erkennt ihn |
| IO7-Buildgrenze | Fehlendes oder auf 0 gesetztes Opt-in abgewiesen; Opt-in 1 übersetzt |
| Funkvertrag | 4572 Assertions, echte Host-HMAC und unabhängige Referenzbytes bestanden |
| Architektur | 7 Selbsttests; 145 Dateien / 302 direkte Includes bestanden |
| ESP32 | `larp-led-product` und `larp-matrix-test` erfolgreich gebaut |
| Unabhängige Implementierungsdurchsicht | Keine konkreten Fehler gefunden |

Das Diodenmodell bildet gerichtete Verbindungen ab und erkennt den bisherigen
Richtungsfehler. Die Beispiel-Lücken behaupten keine konkrete Boxbestückung.
Transaktionsfehler, ignorierte Schreibzugriffe, fehlende Expander, Freigabe
beider Spaltenports trotz Einzelfehler, Wiederanlauf, Uhrüberlauf, Einschwingzeit
und ausschließlich vollständige Veröffentlichung sind geprüft.

Aufrufe: `python firmware/led/tests/run_output_tests.py`,
`python test/test_led_ble_wire/run.py`,
`python scripts/check_architecture.py --self-test`,
`python scripts/check_architecture.py` und
`python -m platformio run -d firmware/led -e larp-led-product -e larp-matrix-test -j 2`.
PlatformIO lief in seiner vorhandenen eigenen Pythonumgebung; der allgemeine
Python-Interpreter enthält das Modul nicht. Die Builds melden die vorhandene
Frameworkwarnung in `esp32-hal-uart.c` (Rückgabe ohne Wert).
Androidquellen und Protokoll sind unverändert; kein neuer Androidlauf.

## Neues Artefakt und Gerätegrenze

Matrix-Firmware SHA-256:
`a15c2c698c527c1b919ffde7cc31b213c91570856100a16d9d5d7741d27beb8d`,
1.139.856 Byte; RAM 42.472 Byte, Programm 1.133.557 Byte.
Partitionstabelle unverändert:
`22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91`.

Der Nutzer bestätigte die Box vollständig stromlos bei angeschlossenen
Matrixleitungen. Kein Gerätezugriff, Upload oder Rollback in dieser Korrektur.
Zuletzt verifiziert installiert bleibt `c613ace`. Die vorhandene Matrix-App
ist zum unveränderten Funkformat kompatibel.

Nächster gemeinsamer Schritt: Gerät und Port frisch zuordnen, aktuellen
Gerätestand sowie chipgebundenen Rückweg prüfen; dann korrigierte Anwendung
installieren/verifizieren und denselben Reedkontakt gezielt halten, lösen und
erneut halten. Physische Leitungen bei verbleibenden Unklarheiten gezielt
prüfen. Details und private Belegorte stehen in der
[aktuellen Sitzungsübergabe](../../handoff/matrix-test-2026-10-16.md).
