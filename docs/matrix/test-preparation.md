# Tastenmatrix: Vorbereitung des gemeinsamen Tests

Stand: **16. Oktober 2026, Europe/Berlin**. Nutzerentscheidung: Tastenmatrix als
nächste Baugruppe nach LED, Wake und NFC. **Vorbereitung, keine Geräteabnahme.**
Basis: `c0f76ee5123ef51252341b21c18a636abb6f6bc2` aus dem NFC-Testzweig.
Die während der Vorbereitung veröffentlichte NFC-Rücklesekorrektur ist
enthalten. Das kombinierte APK ist erneut geprüft, damit der Matrixtest
den korrigierten Appstand erhält.

**Nachtrag aus dem Gerätetest vom 16.10.2026:** App und Prüffirmware wurden
installiert, aber weder der Reedkontakt unten links noch oben rechts wurde
erkannt. Die umgekehrte Abfragerichtung und die B7-Freigabe müssen geklärt
werden. Dieser Stand ist **nicht als funktionierende Matrixprüfung bestätigt**.
Ergebnisse: [Gerätebericht](device-test-2026-10-16.md);
Fortsetzung: [Sitzungsübergabe](../../handoff/matrix-test-2026-10-16.md).

## Ziel und Umfang

Die echte Companion-App zeigt die **81 logischen Positionen A1 bis I9** als
Live-Raster und aktive Koordinaten als Text. Das ist keine Behauptung, dass die
Box 81 bestückte Tasten besitzt. Wir erfassen jeden tatsächlich vorhandenen
Kontakt einzeln und ordnen seine reale Position der beobachteten Koordinate zu.
Die Anzeige verändert weder Pinzuordnung noch Spielkonfiguration.

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

**Ausgelesen am Basiscommit:** `lib/Config/Config.h`,
`src/GameConfiguration/StandardProfile.cpp` und
[Hardwarevertrag](../../Hardware/HARDWAREVERTRAG-V0.1-2026-10-11.md).
Die Leitungszuordnung ist ein Softwarekandidat, keine elektrische Abnahme.

| Logische Leitung | MCP / Anschluss | Neue Scanrolle |
|---|---|---|
| Zeilen 1–8 | 0x21 / B7, B6, B5, B4, B3, B2, B1, B0 | einzeln LOW, sonst hochohmig |
| Zeile 9 | 0x20 / B0 | einzeln LOW, sonst hochohmig |
| Spalte A | 0x20 / B1 | Eingang mit Pull-up |
| Spalten B–H | 0x20 / A0–A6 | Eingänge mit Pull-up |
| Spalte I | 0x20 / B5 | Eingang mit Pull-up |
| Wake 1–4 | 0x20 / B7, B6, B4, B3 | bestehende Wake-Konfiguration erhalten |

SDA21/SCL22 und 100 kHz folgen der vorhandenen Wake-Strecke. Der Matrixscanner
initialisiert keinen zweiten I²C-Bus und benutzt nicht die widersprüchliche
historische `MCPConfig`-Initialisierung.

**Belegt gegen Herstellerquelle am 16.10.2026:** Das
[Microchip-Datenblatt DS20001952D, Seite 1](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf)
bezeichnet A7/B7 beim MCP23017 als reine Ausgänge. Diese Angabe war der Grund
für den Richtungswechsel gegenüber dem ursprünglichen Spalten-LOW-Scanner.
**Die daraus abgeleitete B7-Lösung ist unvollständig:** Der aktuelle Code liest
B7 zwar nicht als Matrixeingang, setzt dessen IODIR-Bit beim Freigeben aber
wieder auf Eingang. Abschnitt 3.5.1, Seite 18 verlangt für IO7 die
Ausgangskonfiguration. Der vorhandene Scanner ist damit kein belegter
datenblattkonformer Ausweg. Die separate Wake-1/B7-Frage bleibt offen.

**Offen:** tatsächliche Chipmarkierungen, Kontaktart, Verdrahtung und
Entkopplungsdioden. Der historische Scanner arbeitete in umgekehrter Richtung.
Vor dem Gerätetest Dioden bzw. ihre Richtung prüfen. Ein ausbleibender Kontakt
beweist bei unbekannter Diodenrichtung keinen defekten Taster; nicht durch
blindes Umstellen auf die alte Treiberschaltung kompensieren.

Nur eine Zeile wird auf LOW geschaltet; keine Scanleitung wird aktiv HIGH
getrieben. Vor Auswahl einer anderen Zeile setzt der Code die bisherige auf
Eingang; die oben genannte Einschränkung von B7 gilt auch für diesen Schritt.
Nach einem Busfehler wird der Messwert ungültig und die Freigabe bestmöglich
versucht. Bei nicht erreichbarem Expander lässt sich die physische Freigabe
nicht softwareseitig garantieren. Das ist kein elektrischer Sicherheitsnachweis.
Mehrfachkontakte mit Rechteckmuster werden als mehrdeutig markiert; angezeigt
werden Rohbeobachtungen, keine behauptete Ghostingfreiheit. Ein Scan ist
zeitlich gestaffelt, keine atomare Momentaufnahme aller Kontakte.

## Vorbereitung für die gemeinsame Sitzung

1. Passendes APK und **Matrix-Prüffirmware** samt Prüfsummen bereithalten.
   Softwareprüfungen und Quellcommit im Paketmanifest abgleichen.
2. Box, reale Kontaktflächen und Orientierung benennen; vorhandene
   Wake-Anordnung erhalten. Alte Berichte nennen unterschiedliche Anordnungen,
   deshalb keine historische Reihenfolge zurückschreiben.
3. Chip, Leitungen und Diodenrichtung anhand der realen Box abgleichen.
   Fehlende Informationen im Protokoll offen lassen; vor unerwarteter
   elektrischer Wirkung abbrechen und die Verdrahtung klären.
4. Erst beim gesondert beauftragten Installationsschritt den chipgebundenen
   vollständigen Flash-Rückweg am tatsächlich angeschlossenen ESP32 prüfen,
   aktuellen Vollflash privat sichern und verifizieren. Vorhandene Schlüssel
   und Partitionstabelle mit dem Rückweg vergleichen.
5. Zuerst APK mit Datenerhalt installieren und zurücklesen/hashprüfen; danach
   nur die passende Anwendung ab `0x10000` schreiben und verifizieren.
   Ältere Apps verstehen den erweiterten Matrixstatus nicht. Keine automatische
   Provisionierung, keine Löschung der Appdaten oder Schlüsselpartition.
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
damit eine falsche Zuordnung sichtbar wird. Nicht vorhandene Positionen als
„nicht bestückt“ kennzeichnen, unbeobachtete als „offen“. Ein Fehler bleibt
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

Geprüfte Ergebnisse und Paketzuordnung werden nach den frischen Läufen im
[Vorbereitungsbericht](preparation-results-2026-10-16.md) festgehalten.
