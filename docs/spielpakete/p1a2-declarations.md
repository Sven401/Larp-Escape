# P1a-2: begrenzte Schema- und Typprüfung der Deklarationen

Stand: **31.10.2026**. Nutzerauftrag: Entwickler und unabhängigen Reviewer mit
Schema-/Typvalidierung bis zum fertigen PR beauftragen. Basis:
`ec1c52084e286892ada6540159e4c8036a907d04` (#114). Führender Entwickler:
`p1a2_developer`; technische Gegenfragen: `p1a2_maintainer`; unabhängiger
Gegenprüfer: `p1a2_reviewer`; PM: übergeordnete Sitzung.

## Umfang und Ergebnisvertrag

`PackageDeclarationReader` verwendet den kanonischen Byteprüfer aus #114 über
einen optionalen synchronen Tokenbeobachter. Die bestehende Bytegrammatik,
Quellfehlerbehandlung und Limits bleiben dort allein definiert. Der Beobachter
bekommt einzelne dekodierte Stringbytes bzw. Integer-/Boolzeichen, keine
Quellzeiger. Tokens können bei ungültigem Input unvollständig sein. Kein
Callback publiziert übernommene Paketdaten. Quelle, Reader und Beobachter haben
je Aufruf einen exklusiven Besitzer; Reentranz und Quellmutation sind verboten.

Das Ergebnis ist ausschließlich `Rejected` oder **`DeclarationsOnly`**, ohne
booleschen Erfolgsoperator und ohne übernommenes Paketmodell. Es bestätigt
exakte Feldmengen, Typen und die hier benannten Werte-/Listenregeln für:

- Inhaltswurzel einschließlich Format, packageId, Revision und Label;
- requirements und gepinnte Modulreferenzen;
- Subjects, Ressourcen und Stationsköpfe;
- Stationswerte, Timer und Ports.

Initialwerte werden streng als bool oder int32 geprüft; kein impliziter Cast.
uint32-Werte, uint16-Modulsemver, Digestsyntax, Label-/ID-Bytegrenzen und die
P0-Listenbudgets werden geprüft. ID-Definitionslisten und requirements sind
byteweise strikt sortiert und eindeutig. Modulreferenzen behalten ihre
Arrayreihenfolge: mehrere Versionen derselben ID sind möglich. Gleiche
(ID,Version,Digest)-Referenzen dürfen wiederholt sein und verbrauchen keine
zusätzliche Modulidentität; gleiche ID/Version mit anderem Digest ist Konflikt.
Die Kapazität von 16 zählt verschiedene (ID,Version)-Paare. Vollständige
Modularray-Duplikat-/Reihenfolgesemantik bleibt eine spätere Vertragsprüfung.

Bezeichner sind gemäß technischem Abgleich genau 1..48 dekodierte ASCII-Bytes
im kanonischen Profil (0x20..0x7F); keine zusätzliche Regex, Normalisierung oder
Locale-Sortierung. Labels enthalten 1..96 UTF-8-Bytes. Capability ist derzeit
**opaque kanonischer JSON-String**; ohne erfundene ASCII-, Längen-, Nichtleer-
oder Allowlistregel. Eine Capability wird dadurch nicht als unterstützt bestätigt.
Positive requirements-Majorversionen haben im P0 keine angegebene Bitbreite und
werden daher ohne überlaufende Integerkonvertierung lexikalisch auf >0 geprüft.

`rules` und `instances` müssen Arrays sein, ihre Inhalte werden lediglich
bytekanonisch und mit denselben Quell-/Tiefenlimits überlesen. Selbst falsche
Regeln oder Instanzen können deshalb `DeclarationsOnly` ergeben. Modulauflösung,
Referenzbindung, Regeln/Ausdrücke, Konfluenz, Planer, Digestberechnung,
Signaturen/Rechte, Capabilityauflösung, Deployment und Aktivierung sind **offen**.
Dieses Paket schließt P1a/#93 und das Gesamtspiel nicht ab.

## Fehler, Lebensdauer und Betriebsprofil

Ein Aufruf setzt den Zustand zurück. Kein Byte-/Quellzeiger bleibt nach Rückkehr
erhalten. Bei Fehler existiert kein veröffentlichtes Teilmodell. Das Ergebnis
enthält den ersten Deklarationsfehler, Byteoffset und JSON-Pointer-Objektpfad;
fehlende Felder nennen das erste fehlende Feld, kanonische/Quellfehler haben
einen leeren Pfad und ihren originalen Offset. Das Readerprofil benötigt höchstens
fünf relevante Objekt-/Arrayebenen (Root bis Stationsdeklaration); überlesene
Inhalte bleiben vom kanonischen Stack begrenzt.

Nach einem Deklarationsfehler läuft der Byteprüfer ohne weitere Schemaarbeit
bis EOF oder seinem ersten Fehler weiter. Kanonische und Quellfehler haben
Vorrang, insbesondere Quellenfehler direkt nach einem vollständigen Root.
`DeclarationResult.canonical` hält auch bei Schemafehler das Byteprüfergebnis.
Die Source wird nicht zurückgesetzt oder ein zweites Mal gelesen.

Kein Heap, keine Ganzdateikopie und kein DOM. Fester Frame-Stack (32), ein
97-Byte-Tokenpuffer, begrenzter Fehlerpfad und höchstens 16 gespeicherte
Modulidentitäten. Lange opaque Strings und überlesene Tokens werden gestreamt,
kein Tokenpufferüberlauf wird als zusätzlicher Formatfehler interpretiert.
Der Besitzer muss das Readerobjekt in seinem eigenen Speicherbudget führen;
keine geprüfte ESP32-Taskstackgröße oder Gerätekapazität daraus ableiten.

Das explizite konservative **Metadaten-Betriebsprofil** zählt die kanonischen
Quellbytes des gesamten Inhalts abzüglich der vollständigen Arraywerte von
Stations-`rules` und `instances`, einschließlich deren Klammern. Alle übrigen
Klammern, Keys, Escapes und Texte zählen. Exakt 32768 Bytes sind zulässig,
mehr ergibt `MetadataLimit`, getrennt von Schemafehlern. Das behauptet keine
vollständige P0-Metadatenbudgetprüfung für semantisch überlesene Inhalte. Das
256-KiB-Gesamtdokumentlimit und Tiefe32 gelten weiterhin uneingeschränkt.

## Prüfstand

Frische lokale Windowsprüfung mit MinGW GCC 5.1.0/32 Bit und C++11,
`-Wall -Wextra -Wpedantic -Werror` im CMake-Hostpfad:

- Vollständiger CMake-/Ninja-Build und **5/5 CTest-Ziele** einschließlich neuer
  sieben Deklarationstestgruppen, unveränderter Byteprüfersuite und Runtime-/Wirefälle.
- PlatformIO 6.2.0 `host-regression`: **14 Suiten / 141 Fälle** erfolgreich.
  Gemeinsame explizite CMake-Quellliste und PlatformIO-Quellfilter enthalten den
  neuen Reader; die frühere getrennte Linklücke aus #114 wird nicht wiederholt.
- **18/18 P0-Modelltests** einschließlich der vorhandenen Referenztraces erfolgreich.
- Architektur **149 Dateien / 309 Includes**, **88 unabhängige C++11-Header**,
  Formatbaseline **339 Dateien** mit clang-format 19.1.7 erfolgreich;
  neuer Test zusätzlich direkt mit demselben Formatter geprüft.

Testfälle enthalten falsche/fehlende/zusätzliche Felder und Typen, Zahlenränder,
ID-/Labelbytes, Semver/Digests, Listenordnung/-kapazität, Modulidentitätsbudget,
opaque Capability, semantisch falsche Skipbereiche und Metadatenlimit exakt/+1.
Quellenfehler an jeder Byteposition einschließlich Rootende, alle kürzeren
Präfixe und Wiederverwendung nach Fehler werden geprüft. JUnit und CMake-Logs
liegen regenerierbar in `private/p1a2-pio.xml` und `private/p1a2-host/`.

Ein zu früh vor Buildende gestarteter erster CTest-Aufruf konnte die noch nicht
gebauten Programme nicht finden; der vollständige Lauf nach Buildabschluss ist
der oben benannte Nachweis. Der Python-Wrapper des vorhandenen Formatters war
zunächst ohne sein Modul nicht lauffähig; geprüft wurde anschließend die
vorhandene native 19.1.7-Binärdatei. Kein Produktfehler daraus abgeleitet.

Unabhängige Review- und CI-Ergebnisse werden am konkreten veröffentlichten
Commit separat zugeordnet. Native SDK-/PSA-, Firmware- und Androidnachweise
folgen den unveränderten PR-Workflows. Keine Geräteprüfung oder Upload.
