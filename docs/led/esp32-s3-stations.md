# LED-/Funkvorbereitung für zwei ESP32-S3-Stationen

Stand: **31. Oktober 2026**. Status: **Softwarevorbereitung; kein Upload und
keine Geräteabnahme**. Nutzerauftrag: zwei Waveshare ESP32-S3-Zero als zusätzliche
autonome Stationen, erster LED-Test über die bestehende Handy-App mit den
eingebauten LEDs. „Nähe Glut“ muss zwischen zwei S3-Stationen funktionieren.
[Verbindliche Hardwareentscheidung](../../Hardware/HARDWAREPLANUNG.md#zusätzliche-autonome-stationen-waveshare-esp32-s3-zero).

## Umfang und Zuständigkeit

Basis: Integrationscommit `ec1c520`, eigener Branch
`codex/esp32-s3-waveshare-tests`. Führender Bearbeiter: Firmwareentwickler
`s3_firmware` mit Plattform-/Geräteintegrationsmandat; unabhängiger Gegenprüfer:
`s3_review` mit Qualitätsmandat. PM koordiniert den Auftrag und diese Dokumentation.
Die anfängliche S3-/RMT-Vorbereitung des Koordinators ist ebenfalls Gegenstand
der unabhängigen Prüfung und gilt nicht als selbst freigegeben.

Die Variante `larp-s3-espnow-test` verwendet auf **beiden Stationen dieselbe
Firmware**. Jede Station bietet den bisherigen geschützten BLE-V2-Zugang,
lokale LED-Bedienung und bewusste ESP-NOW-/RSSI-Aufträge. Sie quittiert zusätzlich
gültige LENT-Funkproben der anderen Station, auch ohne eigene Handyverbindung.
Eine Funkprobe startet oder verändert keine LEDs auf dem antwortenden Gerät.
Die App startet den Effekt auf der gerade verbundenen Station; für die umgekehrte
Richtung wird die andere Station verbunden und der Peer gewechselt.

Keine Zentrale und kein gemeinsames WLAN sind dafür nötig. Das ist eine
begrenzte Baugruppen-Testvariante: LED-Stop bei BLE-Abbruch bleibt erhalten,
Neustart oder Wiederverbindung starten keinen Effekt. Dauerhafter autonomer
Spielbetrieb ohne Handy ist noch keine Abnahme dieses Testziels.

## Boardprofil

- Waveshare ESP32-S3-Zero, eingebaute einzelne adressierbare LED an **GPIO21**,
  Reihenfolge **RGB**. Quelle: [Board-Wiki](https://www.waveshare.com/wiki/ESP32-S3-Zero)
  und [Hersteller-FAQ](https://docs.waveshare.com/ESP32-S3-Zero/FAQ), am 31.10.2026 gelesen.
- PlatformIO `espressif32@6.6.0`, Arduino-ESP32 **2.0.14**, eingebettetes ESP-IDF
  **4.4.6** (lokaler S3-SDK-Header ausgelesen); bestehende Versionspins unverändert.
  Das ist kein neuer nativer IDF-Produktpfad.
  [Getrennte Migration und Supportqualifikation](../migration/esp-idf-qualification-2026-10-28.md).
- Boardbasis `esp32-s3-devkitc-1`, ausdrücklich auf **4 MiB Flash, DIO** und
  vorhandene 4-MiB-Partitionstabelle begrenzt. PSRAM wird nicht aktiviert oder
  vorausgesetzt. Das Profil behauptet keine physische DevKitC-Platine.
- Hardware USB Serial/JTAG mit CDC beim Start; kein Warten auf einen USB-Host.
- Die bestehenden Effekte rechnen weiterhin 23 logische Pixel. Die Boardausgabe
  nimmt je Farbkanal deren Maximum und überträgt **genau ein RGB-Pixel** mit der
  bisherigen Grenze 32/255. Das ist eine Vorschau; Ripple-Raumwirkung und
  Gleichheit mit einem realen Streifen sind damit nicht geprüft.
- Audio, Wake/MCP, NFC und Matrix werden nicht initialisiert oder als verfügbar
  beworben. Sensorabhängiger Resonanzkreis steht in diesem Profil nicht bereit.

## Build und Vorbereitung

```powershell
python -m platformio run -d firmware/led -e larp-s3-espnow-test -j 2
python firmware/led/tests/run_output_tests.py
python test/test_led_ble_wire/run.py
```

Builddateien entstehen unter `.pio/led-product/larp-s3-espnow-test/`. Das bisherige
ESP32-Testziel `larp-espnow-test` bleibt getrennt. Der neue S3-Build ist derzeit
eine lokale Prüfung; die unveränderte CI baut ihn noch nicht als eigenes Ziel.

Vor einem **gesondert beauftragten Upload** beide Chips frisch identifizieren,
Flashgröße und Boardausführung abgleichen, vollständige chipgebundene Sicherung
lesen und mit `verify_flash` am gleichen Gerät verifizieren. Den
[Hardware-Rückweg](../../Hardware/HARDWAREPLANUNG.md#verbleibende-hardwarearbeit)
und [bisherigen Sicherungsablauf](../espnow/preparation-2026-10-18.md#sicherung-und-vorbereiteter-softwarestand)
verwenden. Alte ESP32-Abbilder und Schlüssel nie auf einen S3 übertragen.
COM-Nummern sind keine dauerhaften Geräteidentitäten. Kein Flashbefehl wurde
im Rahmen dieser Softwarevorbereitung ausgeführt.

## Geplanter Handy-/Zwei-Geräte-Test

1. Nach freigegebenem Upload auf beiden Geräten die öffentliche STA-MAC per
   USB-Befehl `LARP-STATION-INFO` feststellen. BLE-Gerätekennungen und STA-MAC
   getrennt halten. Den jeweiligen BLE-Zugang wie bisher mit
   `LARP-PROVISION-V1` explizit auslesen und in der App einrichten; Geheimnisse
   nur lokal verwenden, niemals in Testberichte kopieren.
2. App mit Station A verbinden. Funkeln starten, stoppen und bewusst erneut
   starten; anschließend Lichtimpuls und Ripple prüfen. Board-RGB, Schwarz nach
   Stop und Ende der zeitlich begrenzten Effekte tatsächlich beobachten.
3. Station B als ESP-NOW-Peer mit ihrer STA-MAC wählen. Reine Messung starten:
   frische negative RSSI-Werte, steigender Empfängerzähler, kein LED-Start auf B.
4. „Messen und Funkeln steuern“ starten. Abstand/Ausrichtung ändern; RSSI,
   Zielintensität und sichtbare Helligkeit auf A getrennt protokollieren.
   [Bestehende RSSI-Regelung und Grenzen](../espnow/rssi-test.md).
5. B stromlos machen: keine gültige neue Antwort; Intensität fällt nach
   Veraltung sanft auf 20 %. Nach Wiederkehr nur frische Antworten verwenden.
6. Messung beenden, LED-Stop und BLE-Trennung prüfen. Wiederverbindung und
   Neustart dürfen keinen Effekt selbst starten.
7. Rollen tauschen: App mit B verbinden, A als Peer wählen und Schritte 2–6
   wiederholen. Anschließend mit je einem Handy je Station auch gleichzeitige
   Funkaufträge, verlorene Rückantworten und Stop während eines Auftrags prüfen.

Funkproben bleiben das bestehende unverschlüsselte LENT-Laborprotokoll mit
Korrelation, keine produktive Funkanmeldung. Rückantworten sind keine
LED-Startberechtigung und keine bestätigte optische Wirkung.

## Nähe Glut und offene Nachweise

Der bestehende [NearGlow-Kern und Laborvertrag](../../firmware/led/LIVE-NEARGLOW.md)
bleiben erhalten. Dieser Auftrag bereitet Boardausgabe, BLE-Start und die
Funkstrecke vor. Er definiert keine neue Nähe-Glut-Spielsemantik und behauptet
keine vollständige NearGlow-BLE-/Spielintegration. RSSI-gesteuertes Funkeln
ist ein technischer Vorversuch und keine Ersatzabnahme für „Nähe Glut“.

Offen bleiben der konkrete Zwei-S3-Gerätelauf, RGB-/Helligkeitsbeobachtung,
BLE-/WiFi-Koexistenz, gleichzeitige Last, Reichweite und der vollständige
Nähe-Glut-Effekt zwischen beiden autonomen Stationen.

## Softwareprüfstand

Codecommit: **`f7d73324551283275463fa155f22d123492f8f9f`**, Basis `ec1c520`.
Am 31.10.2026 vom Entwickler ausgeführt und vom unabhängigen Reviewer am
eingefrorenen Code mit Exit 0 wiederholt:

- `run_output_tests.py`: 21 LED-Enginefälle, 5 SDK-Modellfälle einschließlich
  GPIO21/Einpixel/RGB/Schwarz-/Fehlerausgabe, Duplexkorrelation und Wake/Matrix.
  Duplex-Gegenfälle: falsch zugeordnetes/spätes Callback, Cancel, Overflow,
  begrenzter Rückantwortslot, fehlerhafte Pakete und Zeitüberlauf.
- `test/test_led_ble_wire/run.py`: **7945 Assertions** bestanden.
- `firmware/led/tests/run_camera_tests.py`: 16 Labor-Ownerfälle, NearGlow
  7 Prüfgruppen mit 2002 Pegel-/Kurven-Unterfällen, Kristallsiegel 11 und
  RSSI 10 Fälle bestanden.
- Unabhängiger Quellreview durch `s3_review`: keine offenen P1/P2-Befunde.
  Diff-/Leerzeichenprüfung und relative Dokumentlinkziele geprüft.

SDK-Builds am gleichen Codecommit erfolgreich (31.10.2026):

| Ziel | RAM | Programm / Anwendungspartition |
| --- | ---: | ---: |
| `larp-s3-espnow-test` | 65.448 Byte | 1.253.565 / 3.145.728 Byte |
| `larp-espnow-test` (bisheriger ESP32) | 59.480 Byte | 1.492.309 / 3.145.728 Byte |

Buildbefehl: `python -m platformio run -d firmware/led -e larp-espnow-test -e larp-s3-espnow-test -j 2`.
Die bestehende Frameworkwarnung in `esp32-hal-uart.c:153` (Rückgabe ohne Wert)
bleibt sichtbar; beide Builds endeten erfolgreich. Rohlog lokal unter
`private/s3-stations-20261031/build.log`; Binärdateien sind regenerierbar in `.pio/`.

Die Hostmodelle belegen keine reale Queue-/Callback-Nebenläufigkeit, BLE-/WiFi-Koexistenz,
RMT-Wellenform oder optische Ausgabe auf den S3-Geräten. Volle/leere Queue,
konkurrierende Callbacks, Funkverlust und späte Rückantworten unter realer Last
bleiben Teil des oben geplanten Gerätelaufs.

Auftragsführung: zwei gezielte Subagenten (Firmware und unabhängiger Review),
keine Geräte-/Mergeaktion. Laufzeit und Tokenaufwand wurden nicht erhoben.
