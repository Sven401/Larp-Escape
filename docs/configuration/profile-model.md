# Konfigurationsprofil v1 – erster Umsetzungsschnitt

Stand: 5. Oktober 2026. Profilmodell, Standardprofil, Validierung sowie JSON-Import und -Ausgabe sind implementiert. Ein hardwarefreier Graphkern mit Rechner-Vorschau ist implementiert; Profilabhängige Spiel-/Geräteaufträge sind mit Session verbunden; reale Komponentenadapter, Produktionsanschluss, Speicherung, Companion-Transport und Android-Editoren folgen. Die Produktionsfirmware verwendet noch den bisherigen Controller. Ein vorhandenes JSON-Dokument bedeutet keine erreichbare API oder übernommene Gerätekonfiguration.

## Quellen und reproduzierbarer Export

- `include/GameConfiguration/Profile.h`: hardwarefreie, feste Werttypen, Limits und IDs.
- `src/GameConfiguration/ProfileValidation.cpp`: semantische Prüfung mit Fehlerart, Abschnitt, Objekt-ID und Detail.
- `src/GameConfiguration/StandardProfile.cpp`: bisheriges Standardspiel als Daten, aus Rollen-/Kristalldefinitionen und bestehender Hardware/Controllerbelegung.
- `ProfileWriter`: synchroner JSON-Stream ohne Heap und Transport; validiert vor erster Ausgabe. Sinkfehler kann einen Teil eines Dokuments hinterlassen, der verworfen werden muss. Während der Ausgabe bleibt das Profil unverändert.
- `scripts/generate_profile_schema.py`: strukturelles JSON-Schema; liest Kapazitäten und Formatnamen aus den C++-Headern. IDs/Bezüge, GPIO-Eigentum, Ausdruckstiefe und Geräteverträge werden zusätzlich durch die Firmware geprüft.
- `ProfileReader`: begrenzter synchroner Import, erst in getrennten Arbeitsspeicher, dann semantische Validierung und Kopie ins Ziel. Fehlerresultat nennt Art, Byteoffset, Abschnitt, bereits gelesene Objekt-ID und Feld.
- `ProfileVocabulary.h`: gemeinsame Formatnamen für Leser, Schreiber und Schemagenerator.
- `standard-profile.json`: lesbar formatierter Export; Quellprofil ist die C++-Factory. Mit dem Rechnerwerkzeug wieder einlesbar.

```powershell
pio run -e profile-tools
.pio\build\profile-tools\program.exe --summary
.pio\build\profile-tools\program.exe --summary --example
.pio\build\profile-tools\program.exe --summary --import docs/configuration/standard-profile.json
.pio\build\profile-tools\program.exe --import edited-profile.json > canonical-profile.json
.pio\build\profile-tools\program.exe --example > hint-profile.json
.pio\build\profile-tools\program.exe > standard-profile.json
python scripts/generate_profile_schema.py
```

Das Werkzeug läuft am Rechner und initialisiert keine ESP32-Hardware. Es alloziert sein Profil und beim Import einen getrennten Entwurf auf dem Heap; Modell, Leser, Schreiber und Validierung selbst alloziieren keinen Speicher. Unter älteren PowerShell-Versionen bei Umleitung explizit UTF-8 verwenden; die normale Ausgabe sind UTF-8-Bytes. Das exportierte Dokument ist ein Konfigurationsentwurf, keine laufende Anwendung.

## Modell und Grenzen

| Bereich | Kapazität / Vertrag |
| --- | --- |
| Komponenten | 16; zunächst NFC-SPI, MCP23017, Matrix, DFPlayer-UART, WS2811/GRB und ESP-NOW |
| Zustände / Übergänge | 32 / 64; jeweils eigene IDs; kleinere Priorität zuerst, pro Quellzustand eindeutig |
| Bedingungen | 96 Knoten, Pfadtiefe höchstens 8; Predicate, All, Any, Not, Wait; Referenzen dürfen teilen, Zyklen nicht |
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
- `transition_elapsed_ms` zählt fortlaufend seit erster Abfrage im jeweiligen Übergang. Der bisherige wiederkehrende 500-ms-Guard wird im Standardprofil durch die neue Bedingungsart `wait` bewahrt. Genauer Zeit-/Aktionsvertrag und Grenzen stehen in [execution.md](execution.md).
- Aktionen haben explizite Komponenten-/Effektbezüge und Parameter. Unterschied zwischen Anfrage und ausgeführter Hardwarewirkung bleibt Bestandteil der Adapterverträge. Fehlerresultate und Eintritt/Austritt bei manuellen Wechseln folgen in E2/E4.
- Paletten sind 16 RGB24-Werte; ein Kristall ändert sie nur bei `changesPalette=true`. Standardfarben wurden aus `lib/LedFx/Pallets.h` übernommen; bei späterem Entfernen des alten Controllers verbleibt eine kanonische Zuordnung im Profil.

## Importvertrag

`readProfile(reader, scratch, destination)` beziehungsweise die Bytepuffer-Überladung sind transportunabhängig. Beide Profile gehören ausschließlich dem aufrufenden Eigentümer und müssen verschieden sein. Der Leser verändert nur `scratch`, verlangt ein vollständiges Dokument ohne nachfolgende Daten und ruft anschließend `validateProfile` auf. Nur bei Erfolg wird ins Ziel kopiert. Es gibt keine 28-KB-Profilkopie auf dem Parserstack. Bei Fehler kann der Entwurf teilweise beschrieben sein und darf nicht aktiviert werden; das Ziel bleibt unverändert. Kopieren ist **keine atomare Übernahme zwischen FreeRTOS-Tasks**. Eigentümer, Revisionsprüfung und kontrollierte Laufzeitaktivierung sind inzwischen über den [Profilcontroller mit zwei privaten Speichern](profile-slots.md) und die [Übernahme](activation.md) angeschlossen; Persistenz und Fehler-/Geräteabnahme bleiben offen.

`readDraftProfile(reader, draft)` ergänzt den direkten Import in genau einen inaktiven Bereich. Bei Fehler darf dieser teilweise beschrieben sein. Der Profilcontroller verhindert dessen Export/Aktivierung und schützt den aktiven Bereich; so braucht der angeschlossene Übernahmepfad keinen dritten vollständigen Profile-Scratchbereich. Alle folgenden Parsergrenzen gelten für beide Varianten.

- Höchstens **131.072 Eingabebytes (128 KiB)** einschließlich Leerraum/Escapes, Containerpfad höchstens 8 Ebenen. Diese zusätzlichen Dokumentgrenzen gelten neben Tabellenkapazitäten; beliebige Formatierung kann auch ein ansonsten kleines Profil über das Bytelimit bringen. Schemafelder `x-maxDocumentBytes` / `x-maxContainerDepth` sind Hinweise, keine automatisch durch JSON-Schema erzwungenen Regeln.
- Objektfelder dürfen beliebig angeordnet sein. Alle modellierten Felder sind erforderlich; unbekannte und doppelte Schlüssel werden abgewiesen. Schlüssel werden vorher entkodiert, sodass auch zwei unterschiedlich geschriebene Escapes denselben Schlüssel ergeben. Bei Bedingungen richtet sich die erlaubte Feldmenge nach `kind`, auch wenn `kind` zuletzt steht.
- Keine Standardwerte für fehlende Felder, Kommentare, BOM, nachfolgenden Dokumente oder überzähligen Kommata. Feste Arrays müssen genau die definierte Länge haben; Listen prüfen Kapazität vor dem nächsten Eintrag. Ein Byte über dem Streamlimit kann zur EOF-Unterscheidung gelesen werden, wird aber nicht verarbeitet.
- JSON-Texte werden in UTF-8 gelesen; `\u`-Escapes einschließlich gültiger Surrogatpaare werden entkodiert. Ungültiges UTF-8, einzelne Surrogate und C0-Zeichen einschließlich `\u0000` werden abgewiesen. Die 32-Byte-Namensgrenze gilt nach dem Entkodieren; auch Schlüssel und Enumtexte sind begrenzt.
- Numerische Felder verlangen exakte, nichtnegative Ganzzahlwerte innerhalb ihres Zieltyps. Dezimal-/Exponentialschreibweise wie `1.0` oder `1e3` ist bei ganzzahligem Wert zulässig. Keine Gleitkommarundung, stillen Überläufe oder Umdeutung negativer Werte. Erst danach folgen strengere semantische Wertebereiche.
- Eine Quelle meldet Byte, endgültiges Ende oder Lesefehler. Ein Lesefehler gilt nie als Dokumentende. Die Quelle muss begrenzte synchrone Zugriffe liefern; Netzwerk-Wartezeiten, Abbruch und Zugangsprüfung gehören zum späteren Transport. Import nicht aus einem Gerätecallback ausführen.

JSON-Grundlage für Syntax, Zahlen und Unicode: [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259.html). Doppelte Schlüssel strikt abzulehnen und die oben genannten Ressourcen-/Profilgrenzen sind Projektentscheidungen.

## Standardspiel: abgebildet, Ausführungsvergleich noch offen

Export enthält sieben Komponenten, sechs Rollen, neun Zustände, 19 Übergänge, 18 Bedingungsknoten, 37 Aktionen und 20 Effekte (aktuelle Standardrevision 3). UID-/Rollendaten kommen aus `GameDomain::defaultRoleDefinitions`; Hardwarepins und MCP-Startmodi entsprechen dem bestehenden Code. Elektrische Startmodus-/Matrixfragen bleiben offen.

Entry-/Tick-/Transition-Aktionen bilden unter anderem Kristall-/Rollen-Audio, Helligkeitsrampen mit 150-ms-Intervall, Twinkle-Dichte, Burst, Wake, Reset, Rundenzählung und ACTIVATE ab. Wiederholte oder abgewiesene Effektanforderungen behalten ihre Reihenfolge; die Graphvorschau verarbeitet die Anforderungsreihenfolge, typisierte Geräteaufträge sind inzwischen über Session angeschlossen; reale Adapter bleiben offen. Noch kein Beweis identischen Spielflusses. Das zweite Beispielprofil ergänzt Zustand 42 (Hinweis), einen Übergang aus Idle nach 30 Sekunden, Rückkehr nach 5 Sekunden und einen cyanfarbenen Burst; es wird im Profilwerkzeug erfolgreich validiert und exportiert, als Graphvorschau mit vorgegebenen Eingaben ausgeführt; Session führt inzwischen Rollen-/Geräteaufträge aus, siehe den aktuellen Schnitt unten.

`emptyReadsBeforeClear=5`, `consecutiveEmptyReads=false` und die bestehenden späten Standby-Lücken bewahren die dokumentierte Ausgangspolitik. Timer-/Event-Latches, Palette auf neuem Kristall und Verbrauchsaktionen sind in Session umgesetzt; ihre breite Vergleichs-/Fehlerpfadabdeckung bleibt offen. Vorhandene Controllerfixtures dienen dafür als spätere Verhaltensreferenz.

## Gemessener Erstgebrauch

Profilwerkzeug mit C++11 und Warnungen als Fehler gebaut; Factory/semantische Validierung im Werkzeug erfolgreich. Kompakter JSON-Export **19.202 UTF-8-Bytes**, durch Python-JSON-Leser eingelesen; lesbar formatiertes Artefakt 43.164 Byte. Keine neue Host-Testsuite ergänzt oder ausgeführt. Ausführungs- und Fehlerpfadabdeckung des neuen Modells bleibt offen. Das Schema wurde erzeugt; keine unabhängige JSON-Schema-Validierung behaupten.

Architekturprüfung zunächst 45 Dateien/110 Includes, 32 isolierte Header und 80 Formatdateien bestanden. Spiel- und USB-Profil sowie das Profilwerkzeug erfolgreich gebaut. Spielprofil: RAM 55.156 Byte, Flash 1.224.921/1.310.720 Byte (93,5 %); USB-Profil: RAM 21.480 Byte, Flash 291.673 Byte. Profilcode ist noch nicht an den Spielcontroller angebunden und teilweise durch den Linker entfernt; dies ist kein RAM-/Flashbudgetnachweis für dessen spätere Aktivierung. Cppcheck 2.1 für drei neue Implementierungen ohne Befund. Keine Remote-CI ausgeführt. Der USB-ESP32 wurde in diesem Schritt nicht neu geflasht.

## Import: beobachteter Gebrauch im Schnitt `2d7e9dd` am 5. Oktober 2026

Standardprofil über `--import docs/configuration/standard-profile.json` eingelesen und kanonisch wieder ausgegeben: **19.202 Bytes, bytegleich** zum Factoryexport. Hinweisprofil exportiert/eingelesen/ausgegeben: **20.026 Bytes, bytegleich**. Danach JSON-Datei bearbeitet: Name „Hinweis – Bühne“, Revision 3, Hinweisbedingung 45.000 ms und Burstfarbe RGB `0xffaa00`. Der Editor speicherte Unicode als Escapes und kehrte Objektfeldreihenfolgen um; Einlesen/Ausgeben erhielt das vollständige bearbeitete Dokument. Dies belegt den beobachteten Bearbeitungsablauf, keine vollständige Fehlerpfadabdeckung.

Frische Architektur-/Header-/Formatprüfung: 48 Dateien / 119 Includes, 34 isolierte Header und 83 Formatdateien bestanden. Cppcheck 2.1 für Leser, Schreiber und Profilwerkzeug ohne Befund. Keine neue Unity-Suite ergänzt oder ausgeführt; Fehlerfälle, Alias-/Zielerhalt, Grenzwerte und unabhängige Schema-Validierung brauchen noch gesonderte Ausführungsabdeckung. ESP32-Compiler `-Os -fstack-usage`: größter gemeldeter Importframe 1.824 Byte, nächster Frame 400 Byte. Das ist keine gemessene Gesamtstacktiefe oder freigegebene Taskstackgröße.

Builds des Import-Schnitts erfolgreich: Spiel RAM 55.156 Byte / Flash **1.225.549 von 1.310.720 Byte (93,5 %)**, USB RAM 21.480 / Flash 291.673 Byte, Rechnerwerkzeug mit Warnungen als Fehler. Der erste Spiel-Build scheiterte beim Bibliothekskompilieren an „No space left on device“; Wiederholung mit `pio run -e az-delivery-devkit-v4 -j 2 -e profile-tools` erfolgreich, ohne Quelldaten zu löschen. Kein Upload und keine Remote-CI. Profilimport noch nicht im Produktionslauf aktiv; die Buildgrößen belegen dessen späteres Ressourcenbudget nicht.

## Aktueller Session-Schnitt und nächster Schritt

Profilabhängiger Rollen-/Kristallfortschritt und typisierte Geräteaufträge sind über `GameExecution::Session` angeschlossen. Standardspiel mit sechs Rollen und Bonus sowie ein verändertes JSON mit siebter Rolle, vier Kristallen, Zustand 88, 750-ms-Guard und orangefarbener Palette wurden im selben Rechnerprogramm ausgeführt. Diese normalen Abläufe nutzen synthetische Geräte; Produktionscontroller, reale Adapter und breite Fehlerpfad-/Vergleichsabdeckung bleiben offen.

`ReadCrystals.effect` kann jetzt einen Palette-Effekt als LED-Zuordnung referenzieren; dessen Palette wird mit den Kristallfarben ersetzt. Ohne Zuordnung verlangen palettenändernde Kristalle genau einen aktiven LED-Streifen. Das Standardprofil enthält nun 20 Effekte, Revision 3 (19.395 kompakte Bytes); Hinweisrevision 4 (20.219 Bytes). Schema beschreibt den Bezug als Annotation, C++ validiert dessen Bedeutung. Bearbeitbare Zusatzartefakte/Provenienz und Portverträge: [session.md](session.md).

Als Nächstes Komponentenregistry, nicht blockierende Geräteadapter und kontrollierter Produktionsanschluss. Konfigurationsaktivierung, Speicherung, erweiterter Companion-Vertrag, direkter Appzugang ohne WLAN-Infrastruktur und Android bleiben offen. Vor deren Aktivierung RAM-/Flashreserve budgetieren.
