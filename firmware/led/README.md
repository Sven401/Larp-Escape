# Eigenständige LED-Produktfunktion

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

## Separater Matrix-Prüfmodus

Die Umgebung `larp-matrix-test` ergänzt die 9×9-Kontaktanzeige in der echten
Companion-App. Sie treibt einzelne Zeilen LOW, lässt die übrigen Scanzeilen
hochohmig und liest die Spalten mit Pull-up. Die physische Zuordnung und
Diodenrichtung sind offen. Die Standardumgebung `larp-led-product` aktiviert
keinen Matrixscan. Beide Varianten verwenden dieselbe Partitionstabelle und
denselben vorhandenen BLE-Zugang. App zuerst installieren; die Prüfumgebung
sendet verlängerte Statusnachrichten. Vorbereitung und gemeinsame Abnahmefälle
stehen im [Matrix-Testplan](../../docs/matrix/test-preparation.md).

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
