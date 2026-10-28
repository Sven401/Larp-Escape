# Legacy-MCP: echter Bibliothekspfad mit injiziertem Transport

Die Suite kompiliert **Adafruit MCP23017 2.3.2**, **Adafruit BusIO 1.17.4**
und **Unity 2.6.1** aus den gepinnten PlatformIO-Abhängigkeiten. Die eigentlichen
Adafruit-Methoden, `MCPHandler`, `KeyMatrix` und `GameState` werden unverändert
aus ihren jeweiligen Quelldateien übersetzt. Nur die Arduino-/Wire-/SPI-
Plattformgrenze wird im Host ersetzt. SPI wird hier nicht ausgeführt.

## Normaler Prüfweg

Aus dem Repository-Stamm:

```powershell
python -m platformio test -e host-mcp -v
```

Elf Unity-Fälle prüfen die originale Fehlerwertkette, gültige HIGH/LOW-Signale,
NACK, Null-/Kurzantwort, einen einzeln ausgefallenen Expander, Buswiederkehr,
geprüfte Schreibfehler, echte modellierte Matrixkontakte, eine spät ungültige
Teilbatch sowie fehlgeschlagenes Select/Release und gesperrtes Weiterscannen
bis zur vollständigen Freigabe. Der Startup-Fall weist auch nach, dass `begin`
überhaupt keine GPIO-/OLAT-Schreibversuche erzeugt. Zwei weitere Fälle in
`test_game_application` prüfen absichtlich plausible Nutzwerte mit `valid=false`
bis zum Spielverbraucher. Dessen bestehende Suite enthält damit 20 Fälle.

Der Registertest zeigt weiterhin: Die originale skalare Adafruit-Methode kann
bei I²C-Fehler HIGH zurückgeben. Der korrigierte Projektpfad nutzt ausdrücklich
die fehlerfähigen Bool-Overloads und gibt ungültige Samples bis zum Verbraucher
weiter. Ein Erfolg besagt, dass die I²C-Transaktionen bestätigt wurden; er ist
keine Messung des physischen Ausgangspegels.

## Offlineprüfung und reproduzierbarer Rotnachweis

Nach dem normalen Lauf sind die drei Abhängigkeiten in `.pio/libdeps/host-mcp`
vorhanden. Der zusätzliche Runner lädt nichts herunter. Er prüft die angegebenen
Bibliotheksversionen, protokolliert Buildaufruf und SHA-256 der übersetzten
Quelldateien und gibt den echten Build-/Testexitcode zurück:

```powershell
$mcpCompiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
python test/test_mcp_adapter/run.py --compiler $mcpCompiler --output .pio/mcp-green
```

Für die Ausgangsimplementierung vor dem Fix nur die betroffenen alten Dateien
aus dem Dokumentationscommit `dc0e3f12b571c534ef258300a47eab8a917387df` extrahieren.
Der aktuelle Arbeitsbaum bleibt dabei unverändert:

```powershell
$mcpOldFiles = @(
  'lib/MCPHandler/MCPHandler.h', 'lib/MCPHandler/MCPHandler.cpp',
  'lib/Keymatrixwrapper/Keymatrix.h', 'lib/Keymatrixwrapper/Keymatrix.cpp',
  'include/GamePorts/IWakeInput.h', 'include/GamePorts/ICrystalInput.h',
  'lib/GameState/GameState.cpp', 'test/test_game_application/GameFakes.h'
)
git archive --format=zip --output=.pio/mcp-before.zip dc0e3f12b571c534ef258300a47eab8a917387df -- $mcpOldFiles
if ($LASTEXITCODE -ne 0) { throw 'Snapshot extraction failed' }
Expand-Archive -LiteralPath .pio/mcp-before.zip -DestinationPath .pio/mcp-before
python test/test_mcp_adapter/run.py --compiler $mcpCompiler --legacy-snapshot .pio/mcp-before --output .pio/mcp-red
```

Ein frisches Zielverzeichnis verwenden. Diese Variante kompiliert die alten
Projektadapter, Ports und den alten Controller gegen dieselben gepinnten
Bibliotheken. `legacy_probe.cpp` ist nur mit dem ausdrücklich gesetzten Makro
`MCP_LEGACY_PROBE` aktiv; die normale CI führt es nicht aus.

**Erwarteter Rotnachweis: drei fehlgeschlagene Sicherheitsassertionen und
Exitcode ungleich null.** Nach gesundem Start verlassen NACK und Nullantwort
den tatsächlichen Standby; `writeGPIO` meldet bei NACK trotzdem Erfolg. Ein
Compilerfehler wäre kein Rotnachweis. `build.log`, `test.log`,
`build-command.json` und `sources.sha256` bleiben im jeweiligen Ausgabeordner.
Die acht archivierten Dateien sind eine gezielte historische Vergleichsbasis;
andere unveränderte Projektdateien kommen aus dem aktuellen Checkout.

## Aussagegrenzen

- Das Wire-Modell bildet MCP23017/BANK=0-Register, Richtungen, Latches und
  geschlossene Matrixkontakte ab. Fehler werden bei ACK und gemeldeter
  Empfangsmenge injiziert. Es ersetzt weder elektrische Messung noch die
  Bestätigung der Chiprevision, B7-Eignung, Pullups oder tatsächlichen Pinbelegung.
- Ein fehlgeschlagener Schreibaufruf kann im Modell bereits physisch wirksam
  gewesen sein. Weitere LOW-Auswahlen bleiben nach einem Fehler gesperrt,
  solange nicht alle Spaltenfreigaben bestätigt sind. Bei weiterhin defektem
  Bus wird keine physisch erfolgreiche Freigabe behauptet.
- BusIO 1.17.4 prüft die `requestFrom`-Menge, aber nicht jeden einzelnen
  negativen `Wire.read()`-Rückgabewert. Dieser Prüfweg setzt einen exklusiven,
  synchronen Wire-Besitzer und den vollständigen angekündigten RX-Puffer voraus.
  Ein konkurrierender fremder RX-Verbrauch ist nicht abgedeckt.
- Die Samples gelten für den gerade ausgeführten synchronen Read; kein
  Zeitstempel-, SMP-, Timeout-/Frist- oder Langzeitnachweis. Die getrennten
  Baugruppenadapter werden durch diese Legacy-Suite nicht abgenommen.

Die festgelegten Startpegel bleiben erhalten; All-HIGH-Recovery startet erst
nach einem fehlgeschlagenen Laufzeitscan. Keine Firmwareinstallation oder
Paketmigration ist Teil dieses Prüfwegs.
