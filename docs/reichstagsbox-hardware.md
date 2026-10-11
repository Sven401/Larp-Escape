# Reichstagsbox: Hardwareblocker Matrix/Wake

## BLOCKER: Matrix-Reihe 1 und Wake 1 auf GPB7

Stand: 12. Oktober 2026. Erneut geprüft auf Produktbranch
`plan/nutzergefuehrte-hardwarediagnose-startintegration`, Produkt-HEAD
`988367ffa0daebe72faf66e52f5e741d019560b5`; die unten genannten
Quellzuordnungen sind gegenüber dem ursprünglichen Prüfstand `dc2237e8`
unverändert.

Die aktuelle Quellkonfiguration legt Matrix-Reihe 1 auf MCP-Instanz 1
(`0x21`), GPB7, und Wake 1 auf MCP-Instanz 0 (`0x20`), GPB7:

- `lib/Config/Config.h:33`: Matrix-Reihe 1 = Instanz 1 / `MCP_B7`;
- `lib/Keymatrixwrapper/Keymatrix.cpp:38,184`: Wake 1 = Instanz 0 /
  `MCP_B7`;
- `src/GameConfiguration/StandardProfile.cpp:196-206`: dieselbe Zuordnung
  als numerischer Pin 15.

Der Hardwarevertrag v0.1.1 aus Produktcommit `2204d54` und der
Schnittstellenabgleich aus `853dd06` halten fest: Bei tatsächlich bestückten
**MCP23017** sind GPA7 und GPB7 laut Microchip-Datenblatt DS20001952D reine
Ausgänge. Beide oben als Eingänge verwendeten GPB7-Zuordnungen sind deshalb
elektrisch blockiert. Die reale Expander-Variante ist jedoch nicht belegt;
eine Schaltplan- oder Firmwarebezeichnung weist die Bestückung nicht nach.
Der Diagnosepfad muss bis zur Hardwarequalifikation fail-closed bleiben.

### Fehlende As-built-Nachweise

Vor jeder Pinänderung fehlen weiterhin:

- lesbare Chip-/Modulmarkierungen, Platinenrevisionen und Vorder-/Rückseiten
  beider Expander;
- die physische Zuordnung der beiden Module zu `0x20` und `0x21`, einschließlich
  Adressjumpern sowie Steckerorientierung und Adernummern;
- bei spannungsfreier Box eine Durchgangszuordnung von Wake 1 und Matrix-Reihe 1
  über Taster/Matrix, Kabel und Klemme bis zum Expanderpin;
- Durchgangs- und Isolationsmessungen möglicher Ersatzkontakte gegen andere
  Verbraucher, GND und VCC sowie deren tatsächliche Erreichbarkeit am Stecker;
- VDD, Pull-ups/-downs, Schutz-/Pegelstufen, Ruhe-/Aktivpegel und die
  Dioden-/Ghosting-Topologie der Matrix.

`0x20/GPB2` für Wake 1 sowie `0x21/GPA0` bis `0x21/GPA6` für Matrix-Reihe 1
sind im aktuellen Quellprofil lediglich **softwarelogische Kandidaten**. Sie
sind weder als frei verdrahtet noch elektrisch konfliktfrei nachgewiesen und
damit **keine freigegebenen Ersatzprofile**. Eine Freigabe erfordert die
As-built-Nachweise, einen versionierten Hardwarevertrag und anschließend die
nutzergeführte physische Prüfung; Hosttests oder Busantworten reichen dafür
nicht aus.

## BLOCKER: keine aktuelle Upload- oder Restore-Freigabe

Der frühere Ablauf in Produktcommit `5d3779e` gehört zur Produktbasis
`7f2a3e4` auf `codex/reichstagsbox-hardware`. Seine damalige COM3-Zuordnung,
lokalen Python-/Backup-Pfade, Buildbefehle und Aussagen zur geladenen Firmware
sind nur Herkunftsnachweise. Sie identifizieren weder den heute verbundenen
Chip noch das aktuell geladene Image und dürfen nicht als heutiger Upload-,
Restore- oder Rückwegnachweis ausgeführt oder übernommen werden.

Maßgeblich ist
`C:/Users/seide/Projects/Larp-Escape-AI/plans/nutzergefuehrte-hardwarediagnose.md`,
Abschnitte „Kurzer Entwicklungs- und Gerätezyklus“ und „Flashlayout“. Vor einer
künftigen Geräteänderung ist in einem gesondert freigegebenen Geräteschritt:

1. die Originalbox neu zu identifizieren und Chipkennung, Flashgröße,
   tatsächlichen Port, geladenen Image-Hash und Partitionen in einem aktuellen
   Faktenblatt festzuhalten;
2. die außerhalb des Produkt-Repositories liegende Original-Vollsicherung samt
   Manifest über Dateigröße, SHA-256, Chipbindung und Flashgröße dieser Box
   zuzuordnen und ihre Integrität zu prüfen;
3. Zielcommit, Zielabbild-Hash, Partitionstabelle und sämtliche Schreibbereiche
   zu erfassen und gegen den chipgebundenen Rückweg abzugleichen; bei unbekanntem
   Inhalt, geänderten Partitionen/Startdaten oder unklarer Backup-Integrität ist
   der Sicherungsbedarf neu zu entscheiden;
4. Upload und Restore als getrennte Handlungen jeweils erst nach ausdrücklicher
   Nutzerfreigabe auszuführen; ein Restore erfolgt niemals automatisch. Nach
   einem Schreibvorgang sind Schreib-Hashes und der erwartete Startzustand zu
   belegen.

Diese Prüfung wurde in diesem Dokumentationsschritt nicht am Gerät ausgeführt.
Bis das aktuelle Faktenblatt und der geprüfte chipgebundene Rückweg vorliegen,
bleibt jede Firmwareänderung an der Originalbox fail-closed gesperrt.
