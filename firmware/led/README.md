# Eigenständige LED-Produktfunktion

Die zusätzliche Variante `larp-espnow-test` ergänzt die bestehende
Baugruppenbedienung um einen bewusst gesendeten ESP-NOW-Test mit korrelierter
Empfangsbestätigung. Sie benötigt die neue App und den separaten
`espnow-app-receiver` auf dem zweiten ESP.
[Vertrag, Buildpaar und Handy-Testablauf](../../docs/espnow/app-test.md).
Die bisherigen Varianten starten weiterhin kein WiFi.

Aktueller konsolidierter Vertrag: [Baugruppen-Wire V2](../../docs/led/ble-wire-v2.md).
Seit **18. Oktober 2026** ergänzt **Resonanzkreis** als vierter Effekt die
manuelle Bedienung mit echter Wake-Reaktion. Verhalten, Upgrade-Paar und
gezielter Sichtlauf: [Resonanzkreis-Prüfplan](../../docs/led/resonance-test-preparation.md).
Die folgenden drei Effekte und Gerätebelege bleiben der historische Umfang.
Alle drei Varianten enthalten Audio; NFC-/Matrix-Tests bleiben getrennt.
Ein gemeinsames App-/Firmwarepaar ist nötig; es wurde hier nicht installiert.
Prüfstand und Herkunft: [Konsolidierungsbericht](../../handoff/KONSOLIDIERUNG-2026-10-17.md).
Die folgenden datierten Abschnitte bewahren die ursprünglichen Einzelstände.

Stand: 15. Oktober 2026. Die echte Companion-App steuert drei eigene Effekte
über den geschützten BLE-Zugang. Dieser Produktbuild enthält zunächst nur die
LED-Funktion; die Spielintegration folgt getrennt. Die Firmware meldet `LED_ONLY`
und keinen erfundenen gestoppten Zustand des bisherigen Spielablaufs.

| Auswahl | Ausgabe nach Start | Ende |
|---|---|---|
| Funkeln | Eigene versetzte weiße Lichtpulse über alle 23 LEDs | Stop oder erkannter Verbindungsverlust |
| Lichtimpuls | Ein gemeinsames weißes Auf- und Abblenden | nach 1,6 Sekunden aus |
| Ripple | Eine blaue Welle von der Mitte zu beiden Enden | nach 1,4 Sekunden aus |

Auswahl allein sendet nichts. Start ersetzt einen laufenden Effekt und startet
auch denselben Effekt bewusst neu. Stop fordert einen neuen vollständigen
Schwarzframe; die Box bestätigt erst dessen abgeschlossene Übertragung. Neue
Verbindungen und Appstarts starten kein Licht. Verbindungsverlust, Verlust des
Statusabonnements oder Protokollfehler verwerfen die Sitzung und fordern Aus.

## Wake-Testvorbereitung

Dieser Stand ergänzt die vier Wake-Rohpegel des ersten MCP als lesende
Testfunktion. Das LED-Modell bleibt `LED_ONLY`; es entsteht keine Spielintegration.
Der [gemeinsame Testplan](../../docs/wake/test-preparation.md) beschreibt
Zuordnung, HIGH-Interpretation, offene B7-Eignung, Live-Fristen und die
zusammengehörige App-/Firmwareinstallation.

## Audio-Testvorbereitung

Der [gemeinsame Audio-Testplan](../../docs/audio/test-preparation.md) führt
durch bewussten Start, Stop und Neustart über die Companion-App. Er trennt
UART-Auftrag, BUSY-Rohpegel und tatsächliche Hörbeobachtung und hält die
offene Verdrahtung sowie den geprüften Rückweg fest. Der
[Ergebnisbogen](../../docs/audio/test-record-template.md) ist eine Vorlage;
aus der Vorbereitung folgt noch keine Geräteabnahme.

## Separater Matrix-Prüfmodus

Stand der Korrektur: **16. Oktober 2026**. Die Umgebung `larp-matrix-test`
ergänzt die 9×9-Kontaktanzeige in der echten Companion-App. Sie verwendet
wieder die ursprüngliche Richtung: Spalten `0x20 B1/A0–A6/B5` einzeln LOW,
Zeilen `0x21 B7–B0` und `0x20 B0` mit Pull-up lesen. Inaktive Spalten bleiben
hochohmig; das historische anschließende HIGH wird nicht übernommen.
Die Koordinaten A1 bis I9 bleiben unverändert. Einige Kontakte sind laut Nutzer
wegen Platzkollisionen bewusst nicht bestückt; die genaue Liste ist unbekannt.

Der historische Prüfmodus setzt ausdrücklich
`LARP_MATRIX_LEGACY_IO7_INPUT=1`. Er liest B7 wie das Original, löst aber den
dokumentierten MCP23017-Konflikt mit dessen Ausgangsvorgabe für IO7 nicht.
Chipidentität, Eignung und physische Zuordnung bleiben offen. Ein gültiger
Snapshot bedeutet einen vollständigen frischen Registerscan über neun Spalten,
keine Geräteabnahme; fehlende Zeilen werden nicht stillschweigend ausgeblendet.

Die Standardumgebung `larp-led-product` aktiviert keinen Matrixscan. Beide
Varianten verwenden dieselbe Partitionstabelle und denselben vorhandenen
BLE-Zugang. Der Matrixstatus bleibt gegenüber dem installierten Matrix-APK
unverändert. Quelle, Änderung und Softwareprüfstand stehen im
[Korrekturbericht](../../docs/matrix/scan-direction-correction-2026-10-16.md),
Vorbereitung und gemeinsame Abnahmefälle im
[Matrix-Testplan](../../docs/matrix/test-preparation.md).
Die Korrektur ist noch nicht installiert oder am Gerät geprüft.

```powershell
python -m platformio run -d firmware/led -e larp-matrix-test -j 2
```

## Aufbau und Ausgabe

`src/LedOutput.cpp` berechnet die Effekte eigenständig. `src/RmtLedBus.cpp`
übersetzt die fertigen Pixelwerte in SDK-RMT-Pulse. `src/ProductMain.cpp` besitzt
Ausgabe und Protokollzustand in einem Arduino-Loop. BLE-Callbacks kopieren nur
begrenzte, sitzungsgebundene Frames in eine Queue. Die Wirkung wird nach
`rmt_wait_tx_done` mit passender Revision quittiert; eine ATT-Schreibantwort
bestätigt nur den Transport.

Der feste Bus verwendet GPIO 15, 23 Pixel, GRB, 800 kHz und maximal 32/255 je
Farbkanal. Das Pulsprofil stammt aus dem am Gerät bestätigten isolierten Versuch
bei `747812b3bc8a8b937fa8b5c40eea6d4a113afe6b`: 40-MHz-RMT, Bit0 10/40 Ticks,
Bit1 24/26 Ticks, 300 Mikrosekunden Low-Latch. Der SDK-Treiber verwendet normale
Interruptlevel 1–3; seine Sendepuffer bleiben bis TX_DONE unverändert.

Die feste Begrenzung ergibt mit dem konservativen Modell 60 mA pro Pixel bei
Vollweiß plus 1 mA Ruhestrom pro Pixel weniger als 200 mA und bleibt unter dem
1000-mA-Budget. Das ist eine Berechnungsgrenze, keine Strommessung. GPIO-/Pixelzahl
sind am bisherigen Aufbau beobachtet, kein vollständiger CAD-Pinvertrag.

Der Build verwendet ausschließlich Arduino ESP32 2.0.14 (PlatformIO-Paket
`3.20014.231204`) auf Espressif32 6.6.0. Keine zusätzliche Effektbibliothek,
Quellenbeschaffung, Weboberfläche oder WLAN-/OTA-Steuerung ist eingerichtet.
Die 4-MiB-Partitionstabelle hält eine 3-MiB-Anwendung und die eigene Partition
`larpkeys` bei `0x310000` mit `0x6000` Byte. Schlüssel werden bei Lesefehlern
nicht stillschweigend neu erzeugt oder gelöscht. Vor jedem Upload den
chipgebundenen vollständigen Flash-Rückweg prüfen.

## Zugang und Einrichtung

Der [eigene BLE-Vertrag](../../docs/led/ble-wire-v1.md) definiert Framing,
HMAC-Domänen, Sitzungs-/Frist-/Revisionsbindung und Antworten. Die Box erzeugt
zufällige ID und Schlüssel und exportiert sie ausschließlich nach dem expliziten
USB-Befehl `LARP-PROVISION-V1`. Startmeldungen enthalten keine Zugangsdaten.

```powershell
python scripts/provision_led_ble.py --port COM3 --output "$env:LOCALAPPDATA\LarpEscape\box-led-access.json"
```

Der Zielordner muss privat, vorhanden und außerhalb Git liegen; eine bestehende
Datei wird nicht überschrieben. Die Datei über einen vertrauenswürdigen lokalen
Weg auf das Handy übertragen und in der App auf der ESP-Seite über
**Box-Zugang importieren** einrichten. Transportkopien anschließend entfernen.
Die App hält den Zugang verschlüsselt mit Android Keystore außerhalb ihrer
automatischen Backups. Die aktuelle eigene Protokolldomäne verlangt die passende
neue App und Firmware; nach dem Architekturwechsel ist der USB-Import erneut
nötig. Der auf der Box gespeicherte zufällige Schlüssel bleibt erhalten.

Dieser begrenzte LED-Zugang implementiert keine allgemeinen Spieler-/GM-Rollen
oder SMP-Bondabnahme. HMAC schützt Integrität und prüft Schlüsselbesitz; es bietet
keine zusätzliche BLE-Linkverschlüsselung oder Live-Relay-Erkennung. Physischer
USB-/Flashzugriff und die Einrichtungsdatei gehören zum vertrauenswürdigen Setup.

## Prüfwege und Geräteabnahme

Der [USB-Kamera-Messweg](../../docs/led/camera-measurement.md) ergänzt eine
getrennte USB-Prüffirmware, automatische Index-/Farbkalibrierung und den
optischen Vergleich mit der echten Effektengine. Der
[Laborlauf vom 18.10.2026](../../docs/led/camera-results-2026-10-18.md)
bestätigt 23/23 Zuordnungen und alle drei Effekte. Das ist keine gemeinsame
BLE-App-/Produktabnahme; der ursprüngliche Flash wurde danach verifiziert
wiederhergestellt.

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python -m platformio run -d firmware/led -e larp-led-product -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:lintRelease :app:assembleDebug :app:assembleRelease
```

Die Wiretests prüfen echte HMAC-Implementierungen gegen unabhängige gemeinsame
Referenzbytes. Die Ausgabetests prüfen echte eigene Pixelberechnung und
asynchronen Busbesitz; der SDK-Test prüft Pulsprofil und Callback-/Pufferbindung.
Der reale Firmwarebuild prüft den SDK-Anschluss. Keine dieser Prüfungen ersetzt
eine Beobachtung am Streifen.

Am installierten eigenen Paar getrennt bestätigen: Start dunkel; USB-Import;
Funkeln, Lichtimpuls und Ripple; automatisches Ende der beiden Einmaleffekte;
Auswahl ohne Wirkung; Wechsel/Neustart über Start; Stop; Trennen während eines
Effekts; Wiederverbinden ohne Lichtstart; falscher Zugang ohne LED-Wirkung.
App-/Firmwarehashes, signierte Boxantwort und Nutzerbeobachtung bleiben getrennt.

Die [Geräteprüfung vom 15. Oktober](../../docs/led/device-test-2026-10-15.md)
belegt Installation, Anmeldung sowie die gemeinsame Sichtbestätigung von Funkeln,
Stop und dem automatischen Ende von Lichtimpuls/Ripple am eigenen Paar. Weitere
Abnahmefälle bleiben dort ausdrücklich offen.

Der [historische Installationsbericht](../../docs/led/install-2026-10-15.md)
bewahrt einen inzwischen zurückgenommenen Architekturversuch. Seine Sichtbelege
werden nicht als Nachweis dieser eigenen Implementierung übernommen. Die
verbindliche Grenze und externe Funktionsreferenz sind in
[PR #44](https://github.com/Sven401/Larp-Escape/pull/44) dokumentiert.
