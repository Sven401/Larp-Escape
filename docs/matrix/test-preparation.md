# Tastenmatrix: Vorbereitung des gemeinsamen Tests

Stand: **16. Oktober 2026, Europe/Berlin**. Nutzerentscheidung: die Prüffirmware
zuerst aus der ursprünglichen Scanrichtung und Pinbelegung korrigieren.
**Softwarekorrektur vorbereitet, keine Geräteabnahme oder neue Installation.**
Änderung, Quellenvergleich und Prüfstand:
[Korrekturbericht](scan-direction-correction-2026-10-16.md).

Der erste [Gerätetest](device-test-2026-10-16.md) mit dem installierten Paar
`c613ace` erkannte weder den Reedkontakt unten links noch oben rechts.
Diese historischen Ergebnisse und die
[ursprünglichen Softwareprüfungen](preparation-results-2026-10-16.md)
bleiben getrennte Belege. Die Box ist nach Nutzerbestätigung vollständig
stromlos; die Matrixleitungen sind angeschlossen. Die neue Software wurde
nicht auf die Geräte übertragen. Aktueller Arbeitsstand und nächster Schritt:
[Sitzungsübergabe](../../handoff/matrix-test-2026-10-16.md).

## Ziel und Umfang

Die echte Companion-App zeigt die **81 logischen Positionen A1 bis I9** als
Live-Raster und aktive Koordinaten als Text. Das ist keine Behauptung, dass die
Box 81 bestückte Tasten besitzt. Der Nutzer bestätigt am 16.10.2026, dass einige
Kontakte wegen Platzkollisionen mit anderen Baugruppen bewusst nicht bestückt
sind; die genaue Liste ist noch unbekannt. Die Koordinaten werden deshalb
nicht zusammengezogen oder neu nummeriert. Ein leerer Messwert beweist weder
einen Defekt noch eine fehlende Bestückung.

Wir erfassen jeden tatsächlich vorhandenen Kontakt einzeln und ordnen seine
reale Position der beobachteten Koordinate zu. Die Anzeige verändert weder
Pinzuordnung noch Spielkonfiguration.

Die separate Firmwareumgebung `larp-matrix-test` ergänzt den vorhandenen
LED-/Wake-Zugang. Die normale Umgebung `larp-led-product` aktiviert keinen
Matrixscan. Beide benutzen dieselbe Partitionstabelle und den vorhandenen
Box-Zugang. Nur vollständige authentisierte Statusnachrichten liefern neue
Matrixwerte; nach 1000 ms ohne neueren Messstatus sowie bei Trennung erscheinen
die Werte unbekannt. LED-Befehlsantworten verlängern diese Frist nicht.

Das bewährte Muster bleibt: bewusste Aktion → Geräte-/Appanzeige auslesen →
reale Nutzerbeobachtung bestätigen → erst dann weiter. Ein verpasster kurzer
Kontakt wird bewusst wiederholt. Keine automatische Zuordnung aus einem
einzigen verspäteten Bildschirmbild.

## Quellenzuordnung und offene Hardwarefragen

**Quellabgleich am 16.10.2026:** `001eb7b50d79c67c5a738411390db2cb668cf050`
und `61ef6c65206b1fcd5656ac64825d22a518396f99`, jeweils
`lib/Config/Config.h`, `lib/Keymatrixwrapper/Keymatrix.cpp` und
`lib/MCPHandler/MCPHandler.cpp`. Beide zeigen die folgende Pinbelegung und
Abfragerichtung: Spalte LOW, Zeile mit Pull-up lesen. Der Nutzer bestätigt den
funktionierenden Originalbetrieb; die genaue Zuordnung des damaligen
installierten Binärstands ist offen. Quellvergleich und Nutzerbeobachtung
reichen als Grundlage dieser Softwarekorrektur, ersetzen aber keine neue
physische Kontaktzuordnung.

| Logische Leitung | MCP / Anschluss | Korrigierte Scanrolle |
|---|---|---|
| Zeilen 1–8 | 0x21 / B7, B6, B5, B4, B3, B2, B1, B0 | Eingänge mit Pull-up; B7 nur im historischen Prüfmodus |
| Zeile 9 | 0x20 / B0 | Eingang mit Pull-up |
| Spalte A | 0x20 / B1 | einzeln LOW, sonst hochohmig |
| Spalten B–H | 0x20 / A0–A6 | einzeln LOW, sonst hochohmig |
| Spalte I | 0x20 / B5 | einzeln LOW, sonst hochohmig |
| Wake 1–4 | 0x20 / B7, B6, B4, B3 | bestehende Wake-Konfiguration erhalten |

SDA21/SCL22 und 100 kHz folgen der vorhandenen Wake-Strecke. Der Matrixscanner
initialisiert keinen zweiten I²C-Bus und übernimmt nicht die widersprüchliche
historische `MCPConfig`-Initialisierung.

**Offener Herstellerkonflikt:** Das
[Microchip-Datenblatt DS20001952D, Seite 1 und Abschnitt 3.5.1, Seite 18](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf)
bezeichnet GPA7/GPB7 beim MCP23017 als reine Ausgänge und verlangt IO7=0.
Die frühere Umkehr des Scanners löste diesen Konflikt nicht, weil sie B7 beim
Freigeben ebenfalls auf Eingang setzte. Die Korrektur verwendet B7 nun
ausdrücklich wie der Originalscanner als Eingang; das ist **kein
datenblattkonformer Ausweg**. Die Umgebung `larp-matrix-test` setzt dafür
zusätzlich `LARP_MATRIX_LEGACY_IO7_INPUT=1`; ein Matrix-Testbuild ohne dieses
Opt-in wird abgewiesen. Chipidentität und Eignung sowie die separate
Wake-1/B7-Frage bleiben offen. Das Standardprodukt aktiviert keinen Matrixscan.

Nur eine Spalte wird auf LOW geschaltet; vor der nächsten Auswahl wird sie
wieder hochohmig. Anders als im Original erfolgt kein aktives HIGH nach dem
Lesen. Die Stromrichtung durch die Dioden bleibt dieselbe, während Kontakte
keine aktiv HIGH und LOW getriebenen Spalten verbinden können. Bei fehlender
Diodenentkopplung bleibt Ghosting möglich. Die reale Ring-/Kathodenseite wurde
noch nicht bis zu einer konkreten MCP-Klemme verfolgt; erforderliche physische
Abgleiche erfolgen gezielt nach Bedarf und sind keine Vorbedingung der vom
Nutzer beauftragten Softwarekorrektur.

Nach einem Busfehler wird der Messwert ungültig und die Freigabe bestmöglich
versucht. Bei nicht erreichbarem Expander lässt sich die physische Freigabe
nicht softwareseitig garantieren. Erst nach neun vollständig gelesenen Spalten
und geprüfter Konfiguration wird ein neuer Snapshot veröffentlicht.
`valid` bedeutet einen vollständigen frischen Registerscan, keine Chip- oder
Geräteabnahme. Es gibt keinen Acht-Reihen-Teilscan, der als vollständige Matrix
ausgegeben wird. Mehrfachkontakte mit Rechteckmuster werden konservativ als
mehrdeutig markiert; angezeigt werden Rohbeobachtungen, keine behauptete
Ghostingfreiheit. Der Scan ist zeitlich gestaffelt, keine atomare
Momentaufnahme aller Kontakte.

## Vorbereitung für die gemeinsame Sitzung

1. Passendes APK und **Matrix-Prüffirmware** samt Prüfsummen bereithalten.
   Softwareprüfungen und Quellcommit im Paketmanifest abgleichen.
2. Box, reale Kontaktflächen und Orientierung benennen; vorhandene
   Wake-Anordnung erhalten. Alte Berichte nennen unterschiedliche Anordnungen,
   deshalb keine historische Reihenfolge zurückschreiben.
3. Die korrigierte historische Scanrichtung und den offenen IO7-Konflikt im
   Korrekturbericht abgleichen. Physische Unklarheiten gezielt prüfen, wenn
   der begrenzte Kontaktversuch sie erfordert; keinen vollständigen Kabelbaum
   als Vorbedingung der Softwarekorrektur aufnehmen.
4. Erst beim gesondert beauftragten Installationsschritt den chipgebundenen
   vollständigen Flash-Rückweg am tatsächlich angeschlossenen ESP32 prüfen,
   aktuellen Vollflash privat sichern und verifizieren. Vorhandene Schlüssel
   und Partitionstabelle mit dem Rückweg vergleichen.
5. Den vorhandenen Appstand frisch abgleichen: das installierte Matrix-APK
   versteht den unveränderten Matrixstatus bereits. Ein nötiger Appwechsel
   erfolgt mit Datenerhalt und Rücklese-/Hashprüfung vor dem Firmwarewechsel.
   Nur die passende Anwendung ab `0x10000` schreiben und verifizieren.
   Keine automatische Provisionierung, keine Löschung der Appdaten oder
   Schlüsselpartition.
6. Authentifizierte Verbindung und frische Matrixwerte abwarten. Bis dahin
   sind unbekannte Werte korrekt; nicht als unbetätigt werten.

Der bestehende Rückweg ist im
[NFC-Gerätebericht](../nfc/physical-tag-test-2026-10-16.md#installation-und-rückweg)
und [Wake-Gerätebericht](../wake/device-test-2026-10-15.md) beschrieben. Private
Flash- und Zugangsdateien gehören nicht ins Repository oder öffentliche Paket.

## Gemeinsame Prüfschritte

| ID | Aktion und getrennte Bestätigung | Erfolgskriterium |
|---|---|---|
| M1 | Alle Kontakte lösen; Nutzer bestätigt Ruhe, App frisch auslesen | keine beobachtete Aktivierung; unbekannt gilt nicht als Ruhe |
| M2 | Einen eindeutig benannten realen Kontakt mindestens 1 s halten | sichtbare Koordinate und reale Position getrennt protokolliert |
| M3 | Denselben Kontakt lösen und erneut betätigen | frische Rückkehr in Ruhe und bewusst wiederholte gleiche Zuordnung |
| M4 | M2/M3 für jeden vorhandenen Kontakt wiederholen | keine ungeprüfte automatische Hochrechnung auf alle 81 Positionen |
| M5 | Nach bestätigter Verdrahtung zwei Kontakte gleicher Zeile, gleicher Spalte, diagonal halten | tatsächliche Rohbeobachtung festgehalten; keine unerklärten Zusatzkontakte |
| M6 | Nach bestätigter Verdrahtung gezielt drei Rechteckecken betätigen | mögliche Phantomkontakte und Mehrdeutigkeitsanzeige getrennt notiert; keine N-Key-Rollover-Zusage |
| M7 | Verbindung trennen und wiederherstellen | unbekannt bis neue authentisierte Messung; keine alten gedrückten Werte |
| M8 | App neu öffnen, Box verbinden | neue Werte, bestehender Zugang und Wake-Anordnung erhalten |
| M9 | Wake-Eingang und LED Start/Stop kurz gegenprüfen | bestehende Bedienung weiter möglich; keine Matrixwirkung durch LED-Auswahl |
| R1 | Falls temporärer Versuch: vorherige verifizierte Anwendung zurückspielen | Anwendung/Schlüssel unverändert verifiziert; frische authentisierte Wake-Werte |

Bei M2–M4 die reale Position **vor** der erwarteten Softwarekoordinate benennen,
damit eine falsche Zuordnung sichtbar wird. Nur physisch bestätigte fehlende
Kontakte als „nicht bestückt“ kennzeichnen; unbekannte Bestückung und
unbeobachtete Positionen bleiben „offen“. Keine Nummerierung komprimieren und
keine Bestückungsentscheidung aus einem leeren Messwert ableiten. Ein Fehler bleibt
offen, bis seine Ursache geklärt und genau der auslösende Schritt erneut geprüft
wurde. Hosttests und erfolgreiche Installation ersetzen keine dieser Zeilen.

Für das Ergebnis einen datierten Gerätebericht mit Quellcommit, APK-/Firmware-
SHA-256, tatsächlichem Ablauf, Nutzerantworten und bereinigten Appbelegen
anlegen. Das vorbereitete [Zuordnungsblatt](mapping-template.csv) enthält alle
81 logischen Kandidaten und noch **keine** bestätigten Positionen.

## Reproduzierbare Softwareprüfungen

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python -m platformio run -d firmware/led -e larp-led-product -e larp-matrix-test -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:assembleDebug :app:assembleRelease --no-daemon
```

Die ursprünglichen Ergebnisse bleiben im
[Vorbereitungsbericht](preparation-results-2026-10-16.md). Frische Prüfungen und
Artefaktzuordnung der Scanrichtungskorrektur werden getrennt im
[Korrekturbericht](scan-direction-correction-2026-10-16.md) festgehalten.
