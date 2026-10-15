# Vier Wake-Inputs: Vorbereitung des gemeinsamen Tests

Stand: 15. Oktober 2026. Auftrag: vier aktuelle Eingangszustände in der echten
Companion-App erkennen und ihre Kacheln durch Tauschen in einem 2×2-Raster an die
Positionen der realen Box anpassen. Grundlage ist der gemeinsam geprüfte
LED-Produktstand `20518f52de82845635e8590f1440dbfbb268dcb2` aus
[PR #45](https://github.com/Sven401/Larp-Escape/pull/45).
Der neue PR enthält diese noch nicht integrierte Grundlage und richtet sich
weiterhin gegen `plan/projektuebergabe-kuratierter-stand`; kein Merge beauftragt.

## Testumfang und Bedienung

Auf der ESP-Seite steht **Wake-Inputs** vor der LED-Bedienung. Die vier Kacheln
behalten ihre Identität Wake 1–4. Farbe und Text unterscheiden Aktiv, Inaktiv und
Unbekannt; HIGH/LOW bleibt als Rohpegel erkennbar. Mehrere Eingänge können
zugleich aktiv sein. Die Anzeige zeigt empfangene Momentaufnahmen und zählt
keine Impulse.

Eine Kachel länger berühren, aufheben und an die erwartete Position ziehen.
Sie folgt dem Finger einschließlich des ursprünglichen Griffversatzes. Sobald
sie über einem anderen Rasterplatz ist, erscheint dessen Kachel am verlassenen
Startplatz als **Vorschau**. Erst Loslassen auf dem Ziel tauscht beide Plätze.
Ein anderes Ziel aktualisiert die Vorschau, ohne Zwischenstände zu speichern.
Zurückziehen, außerhalb loslassen oder eine abgebrochene Geste verwirft den
Tausch. Barrierefrei stehen entsprechende Tauschaktionen bereit.

Die bestätigte Reihenfolge wird lokal je authentifizierter Box-ID gespeichert
und übersteht App-Neustart und Drehung. Die Anzeigeanordnung verändert weder
Pinzuordnung noch Polarität oder Firmwarekonfiguration. Die Live-Zustände folgen
immer der Input-ID, auch während einer Geste. Eine unterbrochene Geste wird nicht
als bestätigter Tausch wiederhergestellt.

## Elektrischer Prüfvorbehalt

**Ausgelesen, nicht am Gerät abgenommen:** Der bisherige Spielpfad benutzt
MCP-Adresse `0x20`, SDA 21 / SCL 22 und folgende Kandidaten:

| Identität | MCP-Pin | bisherige Interpretation |
|---|---|---|
| Wake 1 | B7 / 15 | HIGH aktiv, Pull-up |
| Wake 2 | B6 / 14 | HIGH aktiv, Pull-up |
| Wake 3 | B4 / 12 | HIGH aktiv, Pull-up |
| Wake 4 | B3 / 11 | HIGH aktiv, Pull-up |

Quellen am Basiscommit: `lib/Keymatrixwrapper/Keymatrix.cpp`,
`lib/MCPConfig/MCPConfig.cpp`, `src/GameConfiguration/StandardProfile.cpp` und
[Hardwarevertrag](../../Hardware/HARDWAREVERTRAG-V0.1-2026-10-11.md).
Position, Chipmarking, Ruhepegel und aktive Polarität der realen Box bleiben offen.
Ein offener Pull-up kann HIGH ergeben, ohne dass ein Sensor aktiv ist.

**Belegt am 15.10.2026:** Das
[Microchip-Datenblatt DS20001952D, Seite 1](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf)
nennt GPA7/GPB7 beim MCP23017 als reine Ausgänge. Der Testpfad bewahrt die bisherige
Zuordnung ausdrücklich als ungeprüfte Rohpegelquelle; er ersetzt B7 nicht still
und behauptet keinen funktionsfähigen vierten physischen Eingang. Vor einer
positiven Abnahme von Wake 1 sind tatsächlicher Baustein und Leitung zu klären.
Das Verschieben einer Kachel behebt diese elektrische Frage nicht.

Der neue Leser nutzt ausschließlich den ersten MCP und konfiguriert die vier
benötigten Bits als Eingänge mit Pull-up und ohne Polaritätsinvertierung. Andere
Bits bleiben erhalten; die alte Matrixinitialisierung wird nicht aufgerufen.
Ein GPIOB-Lesezugriff liefert alle vier Rohpegel gemeinsam. Fehlender Baustein,
Busfehler, unvollständige Antwort oder unpassende Registerbank ergeben unbekannt.

## Übertragung und Kompatibilität

Der bestehende authentifizierte BLE-Status trägt die Wake-Daten im bisher
reservierten Byte 37; Details im [Wire-Vertrag](../led/ble-wire-v1.md).
Die Box liest ungefähr alle 20 ms und sendet Status etwa alle 250 ms, zuzüglich
BLE-Übertragung. Änderungen zwischen zwei gesendeten Momentaufnahmen können
verpasst werden. Für den gemeinsamen Test jeden Zustand mindestens eine Sekunde
halten. Keine Entprellung oder Impulsspeicherung wird als Messung behauptet.

Nach 1000 ms ohne neueren vollständigen Wake-Status entwertet die App die Werte.
LED-Befehlsantworten, gleiche/ältere Sequenzen und einzelne Fragmente verlängern
keine Wake-Frist. Trennen, fehlende Anmeldung und Sitzungswechsel löschen die
Messwerte. Ein frischer Lesefehler ergibt ebenfalls unbekannt.

Die neue App versteht Firmware ohne Wake-Erweiterung und zeigt fehlende
Unterstützung. Die frühere App verlangt Byte 37 = 0 und lehnt die neue Firmware
ab. Deshalb **zuerst die neue App, danach die zugehörige Firmware installieren**.
Dienst-UUIDs, Schlüssel, Flashpartitionen und LED-Befehle bleiben gleich; ein
neuer Schlüsselimport ist bei intaktem vorhandenem Zugang nicht erforderlich.

## Gemeinsamer Ablauf nach Installation

1. Chip/Anschluss für Wake 1 und die offene Polarität besprechen. Box verbinden;
   neue, bestätigte Wake-Werte abwarten. Alle vier Ruhepegel notieren.
2. Jeden realen Eingang einzeln betätigen und mindestens eine Sekunde halten.
   Beobachten, welche feste Input-Nummer zwischen HIGH und LOW wechselt.
   Ruhe- und Aktivpegel getrennt bestätigen; fehlende Reaktion offen lassen.
3. Die reagierende Kachel zur erwarteten Position ziehen. Vor dem Loslassen
   bestätigen, dass die verdrängte Kachel als Vorschau am Ausgangsplatz steht.
   Loslassen, endgültigen Tausch prüfen und denselben Eingang erneut betätigen.
4. Ein Ziel überspringen, zurückziehen, außerhalb loslassen und eine Geste
   abbrechen: keine unbeabsichtigte Umordnung. Alle sechs Platzpaare ausprobieren.
5. Zwei Eingänge gleichzeitig halten: beide müssen unabhängig sichtbar bleiben,
   auch während einer Kachelbewegung. Zustände danach wieder lösen.
6. Handy drehen und App neu öffnen, erneut verbinden: bestätigte Anordnung
   bleibt, alte Messwerte erscheinen vor frischem Empfang nicht als aktuell.
7. Verbindung trennen beziehungsweise Box ausschalten: spätestens bei erkannter
   Trennung oder Fristablauf müssen die Eingänge unbekannt sein. Nach
   Wiederverbindung neue Zustände prüfen. Fehlende/gestörte MCP-Lesung gesondert
   prüfen, ohne vier inaktive oder aktive Eingänge daraus abzuleiten.
8. Bestehende LED-Bedienung kurz gegenprüfen: bewusster Start, Stop und erneuter
   Start; Umordnen einer Wake-Kachel sendet keinen LED-Befehl.

## Prüfstand und Artefakte

**Offen:** Installation dieses Wake-Paars, echte Touch-/TalkBack-Prüfung, Funk,
Ruhe-/Aktivpegel und elektrische Vierkanalabnahme. In dieser Vorbereitung wird
keine Hardware beschrieben. Softwareprüfungen und Builds werden im zugehörigen
PR am geprüften Commit dokumentiert; frühere grüne Ergebnisse gelten nicht als
Nachweis der Änderung.

Reproduzierbare Prüfwege:

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python -m platformio run -d firmware/led -e larp-led-product -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:lintRelease :app:assembleDebug :app:assembleRelease
```

Vor späterem Upload den chipgebundenen Flash-Rückweg aus dem
[Gerätebericht](../led/device-test-2026-10-15.md) am tatsächlich angeschlossenen
Chip erneut abgleichen. APK/Firmwarehash, Installation, empfangener Status und
Nutzerbeobachtung getrennt protokollieren. Private Flash-/Zugangsdateien nicht
ins Repository aufnehmen.
