# Hardware-Quelleninventar vom 10. Oktober 2026
**Zusätzliche Designressource · 1. November 2026:** Das vom Nutzer bereitgestellte
[GPIO-Expander-Fertigungspaket von 2024](references/gpio-expander-2024-11-12/README.md)
ist als Referenz für zukünftiges Hardwaredesign gesichert. Herkunft, SHA-256,
Inhalt und offene Prüfungen stehen dort; keine Zuordnung zur Originalbox oder
Fertigungsfreigabe. Das historische Inventar unten bleibt unverändert.

**Fortschreibung vom 18. Oktober 2026:** Dieser Bericht bewahrt den Quellen-
und Prüfstand vom 10. Oktober. Spätere funktionale LED-/Wake-/Matrix-/Audio-/NFC-
Ergebnisse und die bestätigten As-built-Pins stehen in der
[Hardwareplanung](HARDWAREPLANUNG.md) und im
[fortgeschriebenen Hardwarevertrag](HARDWAREVERTRAG-V0.1-2026-10-11.md).
Frühere Fehlversuche und offene Funktionsangaben unten sind historische Befunde;
sie setzen die späteren Teilnachweise nicht zurück. CAD-Eingangsdateien und
Hashes bleiben unverändert; elektrische und Fertigungsfreigabe bleiben offen.

Status: unverändertes Quellen- und Exportpaket, keine freigegebene Hardware-Revision.
Basis: Produktcommit `29da937c4309beddfd45c5dd0fbf84fdf853d8e4` (`main`).
Paketbranch: `plan/hardware-revision-quelleninventar`.

## Herkunft und Umfang

Die 15 Dateien stammen aus dem bis dahin unversionierten Verzeichnis `Hardware/`
des vorhandenen Produkt-Hauptcheckouts. Übernommen wurde dessen Dateistand am
10. Oktober 2026, bytegleich und ohne Änderungen oder Löschungen an den Originalen.
Die Tabelle bindet die Übernahme an konkrete Bytes. Ursprünglicher CAD-Ersteller,
Exportversion und Zuordnung zur bestückten Originalbox sind aus diesem Eingang
nicht unabhängig bestätigt. Datumsangaben in Dateinamen sind keine verifizierten
Revisionskennungen. Das Paket bewahrt den vorhandenen Eingang zur weiteren Prüfung;
es definiert noch keinen verbindlichen Pin-, Bus- oder Versorgungsvertrag.

Enthalten sind JSON- und SCHDOC-Quelldateien sowie SVG-, PNG-, PDF- und BOM-Exporte.
Die CAD-Importierbarkeit der SCHDOC-Dateien wurde nicht geprüft. Die CSV-Dateien
enthalten UTF-16-kodierte, tabulatorgetrennte BOM-Daten. Es wurde keine PCB-Layoutdatei oder gesonderte
Netzlistendatei im Eingang gefunden. Eine ausdrücklich als V2 benannte editierbare
Quelle fehlt; eine mögliche Zuordnung zu den generischen Sheet-Dateien ist offen.

## Duplikate und Varianten

- `SCH_Gpio-Expander_2026-10-10.json` und `SCH_Gpio-Expander_2026-10-10 (1).json` sind bytegleich.
- `Schematic_Esp-Breakout-V2_2026-10-10.png` und `Schematic_Esp-Breakout-V2_2026-10-10 (1).png` sind bytegleich.
- Die beiden `Sheet_1`-SCHDOC-Dateien unterscheiden sich, ebenso die beiden `Sheet_1`-SVG-Dateien. Die Endung `(1)` bedeutet hier keine bestätigte Kopie. Alle Varianten bleiben erhalten.
- Breakout, Breakout V2 und GPIO-Expander werden anhand ihrer Dateinamen unterschieden; die vollständige Zuordnung von Quelle, Export und realer Baugruppe ist offen.

## Offene Hardware- und Integrationsfragen

Beide Breakout-BOMs und beide benannten Breakout-SVGs enthalten `LED-G4` und
`WS2881`. Der BOM-Eintrag selbst bezeichnet eine dreipolige Schraubklemme und
bestätigt keinen tatsächlich verbauten LED-Chip. `LED-G4` allein belegt auch
keine elektrische Verbindung zu GPIO4.

Die fortgeschrittene Firmwarebasis `7f2a3e4` und die serielle Diagnose bei `db52d7a`
konfigurieren dagegen WS2811/GRB an GPIO15 (`src/main.cpp`, beziehungsweise
`src/HardwareAcceptance/main.cpp`). Diese extern zum Paket verglichenen Stände
werden hier nicht übernommen. LED-Typ, tatsächlicher Datenpfad, Versorgung unter
Last, Pegel und Revisionszuordnung bleiben ungeklärt. Daraus folgt keine Freigabe
für eine Pin- oder Treiberänderung. Der gemeldete fehlgeschlagene LED-Sichttest
bleibt offen; Ausgabeaufrufe belegen keine physische Funktion.

CAD-Netzliste, elektrischer Regelcheck, PCB-Regelcheck, vollständiger Pin-/Bus-/
Versorgungsplan und Abgleich der bestückten Box fehlen. Kein Fertigungsfreigabestand.
Die nutzergeführte Diagnose über Firmware und Projekt-App ist ein getrenntes,
noch nicht durch diese Dateien erfülltes Produktpaket. Kein Hardwarelauf wurde
für diese Übernahme durchgeführt.

## Prüfung dieser Übernahme

Dateiliste und Anzahl (15), Dateigrößen und SHA-256 wurden vor und nach der Kopie
verglichen. Alle drei JSON-Dateien und alle fünf SVG-Dateien sind syntaktisch
parsebar. Das bestätigt weder CAD-Semantik noch elektrische Richtigkeit.
Die Tabelle bezieht sich ausschließlich auf die unveränderten Eingangsdateien;
dieses Manifest und die lokale `.gitattributes` ergänzen das Paket auf 17 Dateien.

Die Dateiformatregel verhindert eine Änderung der Eingangsbytes durch Git-
Zeilenendennormalisierung und kennzeichnet PDF als Binärformat. Die Leerzeichenprüfung erkennt die
originalen CRLF-Zeilenenden als Zeilenenden; andere Leerzeichenprüfungen bleiben aktiv. Vor der Regel
hatte Git JSON-Zeilenenden normalisiert und PDF-Inhalte als Text mit nachgestellten
Leerzeichen beanstandet. Die anschließend neu vorgemerkten 15 Eingangsdateien
werden auch gegen die Git-Blobs auf Bytegleichheit geprüft; `git diff --cached
--check` wird auf dem vollständigen Paket ausgeführt. Dies ist keine CAD-Prüfung.

| Datei | Bytes | SHA-256 |
| --- | ---: | --- |
| `BOM_Esp-Breakout-V2_2026-10-10.csv` | 3024 | `2d6292179cfc36f5482caa9c7e74384bcc6a65ba47f3dba3256e9c775e101411` |
| `BOM_Esp32-Breakout_2026-10-10.csv` | 3040 | `f7caf1b2b9a502106a003348cd62f9664110a23f0b532788ea23609310dfdd42` |
| `SCH_Esp32-Breakout_2026-10-10.json` | 75337 | `6438234c76b455c6910777e1bae82de1f5031ca5967a659261cf0349354e06cb` |
| `SCH_Gpio-Expander_2026-10-10 (1).json` | 76006 | `946e9df479d5286c5cf358e86e515687d6df86c2dffb0a13f6e2389269b0802e` |
| `SCH_Gpio-Expander_2026-10-10.json` | 76006 | `946e9df479d5286c5cf358e86e515687d6df86c2dffb0a13f6e2389269b0802e` |
| `Schematic_Esp-Breakout-V2_2026-10-10 (1).png` | 110948 | `c2e789f34d03539b3a22f06ead7df53c40565d8d8ec541922f82566aefae2876` |
| `Schematic_Esp-Breakout-V2_2026-10-10.pdf` | 59240 | `829d00304d1be2dcaf060ff94bad37820ba1b4d87a0e824e46d15e8744c5f288` |
| `Schematic_Esp-Breakout-V2_2026-10-10.png` | 110948 | `c2e789f34d03539b3a22f06ead7df53c40565d8d8ec541922f82566aefae2876` |
| `Schematic_Esp-Breakout-V2_2026-10-10.svg` | 139936 | `ca93cd9a221d4cb464df7e0beee7801a00a361a40db6a7a466332df0d67360c6` |
| `Schematic_Esp32-Breakout_2026-10-10.svg` | 228669 | `0cd2811f46fee0f75fb2d6fed32c9e0d55e4a8df69d0373720f72d1691475627` |
| `Schematic_Gpio-Expander_2026-10-10.svg` | 209658 | `1352fe0270f3324d9bf9a1636ddd05d2012edea924031a606897a89539c5ea46` |
| `Sheet_1_2026-10-10 (1).schdoc` | 116928 | `384597fd814c08e5b551c12e783300b7d9018217ab8d397510d7950d9b9a24aa` |
| `Sheet_1_2026-10-10 (1).svg` | 131879 | `b0c4abc2608e71731c2b9e1a5bdbc204a38a2c855eea81fca543f2d3d1f36b46` |
| `Sheet_1_2026-10-10.schdoc` | 151441 | `249924d55aa85eca3948a9ab6fc47b2c6cc623ef6646695cb54f3740d78a7cee` |
| `Sheet_1_2026-10-10.svg` | 144857 | `920ba978fc13bf7f05816caf4056f01826a5e0c9d8f2c16b5fd67f1cb4785836` |
