# Konfigurationsprofil v1 – erster Umsetzungsschnitt

Stand: 5. Oktober 2026. Profilmodell, Standardprofil, Validierung und JSON-Ausgabe sind implementiert. JSON-Einlesen, Ausführung des Graphen, Speicherung, Netzwerkschnittstelle und Android-Editoren folgen. Die Produktionsfirmware verwendet noch den bisherigen Controller. Ein vorhandenes JSON-Dokument bedeutet keine erreichbare API oder übernommene Gerätekonfiguration.

## Quellen und reproduzierbarer Export

- `include/GameConfiguration/Profile.h`: hardwarefreie, feste Werttypen, Limits und IDs.
- `src/GameConfiguration/ProfileValidation.cpp`: semantische Prüfung mit Fehlerart, Abschnitt, Objekt-ID und Detail.
- `src/GameConfiguration/StandardProfile.cpp`: bisheriges Standardspiel als Daten, aus Rollen-/Kristalldefinitionen und bestehender Hardware/Controllerbelegung.
- `ProfileWriter`: synchroner JSON-Stream ohne Heap und Transport; validiert vor erster Ausgabe. Sinkfehler kann einen Teil eines Dokuments hinterlassen, der verworfen werden muss. Während der Ausgabe bleibt das Profil unverändert.
- `scripts/generate_profile_schema.py`: strukturelles JSON-Schema; liest Kapazitäten aus dem Header. IDs/Bezüge, GPIO-Eigentum, Ausdruckstiefe und Geräteverträge werden zusätzlich durch die Firmware geprüft.
- `standard-profile.json`: lesbar formatierter Export; Quellprofil ist die C++-Factory. Noch kein JSON-Roundtrip oder Import implementiert.

```powershell
pio run -e profile-tools
.pio\build\profile-tools\program.exe --summary
.pio\build\profile-tools\program.exe --summary --example
.pio\build\profile-tools\program.exe --example > hint-profile.json
.pio\build\profile-tools\program.exe > standard-profile.json
python scripts/generate_profile_schema.py
```

Das Werkzeug läuft am Rechner und initialisiert keine ESP32-Hardware. Es alloziert sein Profil einmal auf dem Heap; das Modell und die Validierung selbst alloziieren keinen Speicher. Unter älteren PowerShell-Versionen bei Umleitung explizit UTF-8 verwenden; die normale Ausgabe sind UTF-8-Bytes. Das exportierte Dokument ist ein Konfigurationsentwurf, keine laufende Anwendung.

## Modell und Grenzen

| Bereich | Kapazität / Vertrag |
| --- | --- |
| Komponenten | 16; zunächst NFC-SPI, MCP23017, Matrix, DFPlayer-UART, WS2811/GRB und ESP-NOW |
| Zustände / Übergänge | 32 / 64; jeweils eigene IDs; kleinere Priorität zuerst, pro Quellzustand eindeutig |
| Bedingungen | 96 Knoten, Pfadtiefe höchstens 8; Predicate, All, Any, Not; Referenzen dürfen teilen, Zyklen nicht |
| Aktionen / Effekte | 96 / 32; je Zustand bis 8 Eintritts-/Tick-/Austrittsaktionen, je Übergang 4 Aktionen |
| Rollen / Kristalle | 16 Rollen mit je bis 8 Kristallen; eindeutige sieben Byte lange UID und eindeutige Positionen pro Rolle |
| Matrix / Wake | Je 9 Zeilen/Spalten, bis 4 Wake-Eingänge; Listenposition ist logische Koordinate, Zeile 1..9 / Spalte 0..8 |
| LEDs | Bis 256 im Modell; physischer Aufbau bislang 23. Keine Hardwarefreigabe für die Maximalzahl |
| Namen | 32 UTF-8-Bytes ohne Terminator, gültiges UTF-8, keine C0-Steuerzeichen; Schema-Codepunktgrenze allein reicht nicht |
| IDs | 1..65535 innerhalb ihrer typisierten Tabelle; 0 ist fehlender/ungenutzter optionaler Bezug, keine Zustands-ID |
| Speicher | **28.636 Byte je Profil** auf MinGW32 und ESP32-Compiler; zwei Profile mindestens 57.272 Byte ohne Parser/Laufzeitkontext |

Kapazitäten bieten begrenzte Erweiterung gegenüber 7 Geräten, 9 Zuständen, 19 Übergängen, 6 Rollen und 18 Kristallen des Standardspiels. Sie sind ein erster Ressourcenentwurf, keine gemessene Produktionsreserve. Größen später mit Parser, aktivem Profil, Entwurf, Laufzeitstatus und WLAN zusammen budgetieren. Profil nie auf kleinen Taskstack legen. Die ESP32-Compileranalyse meldet für `makeStandardProfile` allein 2.912 Byte statischen Frame bei `-Os`; das ist keine gemessene Gesamttiefe einschließlich aufgerufener Funktionen.

## Wire- und Bedeutungsverträge

- Konfigurationsschema und Statusschema sind unabhängig. `schemaVersion=1` im Profil bezeichnet diesen Konfigurationsvertrag; bestehende Status-v1-Capability bleibt ausschließlich `status.read`.
- Zustände und Rollen verwenden unabhängige IDs; das Standardprofil vergibt 1..9 beziehungsweise 1..6. Neue Profile müssen diese Nummerierung nicht beibehalten. Sichtbare Namen sind keine Identität.
- Komponententypen, Felder, Bedingungen, Aktionen und Effektarten werden über explizite ASCII-Namen serialisiert. Startmodi für MCP-Pins: 0 nicht konfiguriert, 1 InputPullup, 2 Output, 3 Input. GPIO 255 bedeutet ungenutzt.
- Komponenteneinstellungen sind einheitlich im Dokument enthalten. Aktive Felder hängen vom Typ ab; nicht verwendete Parameter dienen als gespeicherte Voreinstellung. Unbekannte Typen/Schemata werden abgewiesen, deaktivierte Geräte dürfen nicht von aktiven Regeln/Aktionen referenziert sein.
- SPI-Pinfolge SCK/MISO/MOSI/SS, I²C SDA/SCL, Audio RX/TX/BUSY/WAKE, LED DATA. GPIO 6..11 und USB-UART 1/3 stehen hier nicht zur Verfügung; GPIO 34..39 sind keine Ausgänge. Bootstrapping-Pins brauchen weiterhin elektrische Abnahme.
- Geteiltes I²C nur bei gleichem Bus und gleicher SDA-/SCL-Zuordnung; Adresse eindeutig. Matrixpositionen und Wake-Eingänge dürfen keine Expanderpins doppelt belegen. UART0 bleibt Diagnose; DFPlayer-Vertrag derzeit 9.600 Baud, Lautstärke 0..30.
- Bedingungen lesen vorab erfasste Werte/Ereignisse. Ein Predicate führt kein Geräte-I/O aus. Gerätebezogene Felder besitzen eine Komponenten-ID. All/Any haben zwei Kinder, Not eines. Boolwerte erlauben eq/ne mit 0/1; Zeitvergleiche sind auf halben uint32-Bereich begrenzt.
- `transition_elapsed_ms` ist eine pro Übergang ausgewertete Uhr. Vor der Ausführung ist noch festzulegen, wann ihre Beobachtung beginnt und wie sie beim Verlassen zurückgesetzt wird; das Standardspiel startet seinen 500-ms-Guard bei erster Guardprüfung. Die neue Engine muss dieses Verhalten bewahren.
- Aktionen haben explizite Komponenten-/Effektbezüge und Parameter. Unterschied zwischen Anfrage und ausgeführter Hardwarewirkung bleibt Bestandteil der Adapterverträge. Fehlerresultate und Eintritt/Austritt bei manuellen Wechseln folgen in E2/E4.
- Paletten sind 16 RGB24-Werte; ein Kristall ändert sie nur bei `changesPalette=true`. Standardfarben wurden aus `lib/LedFx/Pallets.h` übernommen; bei späterem Entfernen des alten Controllers verbleibt eine kanonische Zuordnung im Profil.

## Standardspiel: abgebildet, Ausführungsvergleich noch offen

Export enthält sieben Komponenten, sechs Rollen, neun Zustände, 19 Übergänge, 18 Bedingungsknoten, 37 Aktionen und 19 Effekte. UID-/Rollendaten kommen aus `GameDomain::defaultRoleDefinitions`; Hardwarepins und MCP-Startmodi entsprechen dem bestehenden Code. Elektrische Startmodus-/Matrixfragen bleiben offen.

Entry-/Tick-/Transition-Aktionen bilden unter anderem Kristall-/Rollen-Audio, Helligkeitsrampen mit 150-ms-Intervall, Twinkle-Dichte, Burst, Wake, Reset, Rundenzählung und ACTIVATE ab. Wiederholte oder abgewiesene Effektanforderungen behalten ihre Reihenfolge; die Ausführungsengine ist noch nicht implementiert. Noch kein Beweis identischen Spielflusses. Das zweite Beispielprofil ergänzt Zustand 42 (Hinweis), einen Übergang aus Idle nach 30 Sekunden, Rückkehr nach 5 Sekunden und einen cyanfarbenen Burst; es wird im Profilwerkzeug erfolgreich validiert und exportiert, noch nicht als Spiel ausgeführt.

`emptyReadsBeforeClear=5`, `consecutiveEmptyReads=false` und die bestehenden späten Standby-Lücken bewahren die dokumentierte Ausgangspolitik. Timer-/Event-Latches, Palette auf neuem Kristall und eindeutige Verbrauchsaktionen müssen beim Anschluss an den Controller konkret umgesetzt werden. Vorhandene Controllerfixtures dienen dafür als spätere Verhaltensreferenz.

## Gemessener Erstgebrauch

Profilwerkzeug mit C++11 und Warnungen als Fehler gebaut; Factory/semantische Validierung im Werkzeug erfolgreich. Kompakter JSON-Export **19.202 UTF-8-Bytes**, durch Python-JSON-Leser eingelesen; lesbar formatiertes Artefakt 43.164 Byte. Keine neue Host-Testsuite ergänzt oder ausgeführt. Ausführungs- und Fehlerpfadabdeckung des neuen Modells bleibt offen. Das Schema wurde erzeugt; keine unabhängige JSON-Schema-Validierung behaupten.

Architekturprüfung zunächst 45 Dateien/110 Includes, 32 isolierte Header und 80 Formatdateien bestanden. Spiel- und USB-Profil sowie das Profilwerkzeug erfolgreich gebaut. Spielprofil: RAM 55.156 Byte, Flash 1.224.921/1.310.720 Byte (93,5 %); USB-Profil: RAM 21.480 Byte, Flash 291.673 Byte. Profilcode ist noch nicht an den Spielcontroller angebunden und teilweise durch den Linker entfernt; dies ist kein RAM-/Flashbudgetnachweis für dessen spätere Aktivierung. Cppcheck 2.1 für drei neue Implementierungen ohne Befund. Keine Remote-CI ausgeführt. Der USB-ESP32 wurde in diesem Schritt nicht neu geflasht.

## Nächster konkreter Schritt

Begrenzten JSON-Importer für dieses Schema ergänzen: Entwurf statt aktives Profil ändern, Mengen/Text/Typen früh prüfen, Überlauf und doppelte Schlüssel abweisen, danach semantische Validierung. Anschließend E2-Graph-/Bedingungsausführung und Standardspielvergleich; erst dann Konfigurationsaktivierung, Speicherung und Appanbindung.
