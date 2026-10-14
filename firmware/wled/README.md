# WLED-LED-Produktfunktion

Stand: 15. Oktober 2026, neuer Produktzweig auf Integrationsbasis
`42610abfef07c971d9902ea6ca5a350ed68981bf`. Implementiert und hostseitig geprüft;
Geräteabnahme dieses WLED-/Android-Paars noch offen. Die endgültigen Builds und
Prüfergebnisse stehen im zugehörigen Pull Request.

## Bestätigter Umfang

Der Nutzer hat für diesen PR WLED als LED-Grundlage, die echte Companion-App
über BLE und einen eigenen, einmal über USB eingerichteten LED-Zugang gewählt.
Die Spielintegration folgt ausdrücklich getrennt. Die Firmware meldet daher
`LED_ONLY`; sie meldet keinen erfundenen gestoppten Zustand des alten Spiels.
Der bisherige Spielbuild bleibt ein eigener Build. Eine spätere Integration muss
laufendes, pausiertes und unbekanntes Spiel für manuelle Starts sperren.

| Auswahl | Ausgabe nach Start | Ende |
|---|---|---|
| Funkeln | WLED Twinklefox, feste weiße Variante | Stop oder erkannter Verbindungsverlust |
| Lichtimpuls | WLED Fade, ein heller/ dunkler Zyklus | selbstständig aus nach rund 1,6 s |
| Ripple | eine mittige Welle über WLED Ripple | selbstständig aus nach der Welle |

Die Auswahl allein sendet nichts. Start ersetzt einen laufenden Effekt und startet
auch denselben Effekt ausdrücklich neu. Stop fordert einen frischen Schwarzframe.
Neue Verbindungen starten kein Licht. Eine getrennte Verbindung, verlorene
Statusanmeldung oder ein Protokollfehler verwirft die Sitzung und fordert Aus.

## Aufbau und Herkunft

Der neue Build liegt unter `firmware/wled/`; der Einstieg ist
`usermods/larp_ble/src/ProductMain.cpp`. Die vorhandene Android-App unter
`companion/android/` enthält die Bedienung auf der ESP-Seite. Sie bleibt dieselbe
App mit Szenen-/NFC-Funktionen, kein weiterer Diagnoseclient.

`scripts/prepare_wled.py` lädt die unveränderten Originalquellen von
[WLED 16.0.1](https://github.com/wled/WLED/tree/29b389df1c1aaec6ff53aea742d17063b985906c)
bei `29b389df1c1aaec6ff53aea742d17063b985906c` in den ignorierten Ordner
`.pio/wled-source-stable` und kopiert den lokalen Adapter samt Buildkonfiguration
hinzu. Fremde Änderungen oder eine andere Revision im Quellcache werden abgewiesen.
Das ist eine Produktvariante mit eingeschränkten Eingängen: Der normale
WLED-Start/Loop und Webserver werden nicht gestartet. Es gibt in dieser Variante
keine WLED-Weboberfläche, WLAN-Steuerung, Preset-Wiederherstellung, Tastersteuerung
oder OTA-Aktualisierung. Der BLE-Zugang ist der einzige LED-Befehlseingang.

WLED übernimmt Segment, Effektberechnung, Gamma, Helligkeit, Strombegrenzung und
die NeoPixelBus-Ausgabe. Es gibt keinen zweiten GPIO-/FastLED-Ausgabepfad.
Der Adapter setzt einen Bus mit **GPIO 15, 23 Pixeln, WLED-Bustyp WS2812_RGB
(800-kHz-WS2811-kompatibel), GRB, Helligkeit 32/255 und 1000 mA
Softwarebegrenzung** auf. Diese feste Konfiguration stammt aus
dem bestätigten isolierten LED-Versuch; sie ist kein allgemeiner CAD-Pinvertrag.

Die einmalige Ripple-Lebensdauer ist ein dünner Adapter um den nativen
`mode_ripple`: Er setzt eine mittige Welle in dessen Runtime und unterdrückt weitere
zufällige Starts. Er enthält keine eigene Wellen-/Pixelberechnung. Die Kopplung
an WLEDs vier Byte großen Ripple-Runtime-Eintrag ist im Quelltext bezeichnet;
ein WLED-Update verlangt eine erneute Prüfung dieses Adapters. Burst verwendet
die native Fade-Zeitbasis mit genau einem Zeitraum. WLEDs native Startlogik wird
nicht nachträglich durch eine Zustandsrücksetzung abgesichert: Sie läuft gar nicht.

Upstream-Herkunft und Lizenz bleiben im heruntergeladenen Original erhalten:
[WLED-Lizenz EUPL 1.2 oder später](https://github.com/wled/WLED/blob/29b389df1c1aaec6ff53aea742d17063b985906c/LICENSE).

Weitere festgelegte Buildquellen:

- PlatformIO Core 6.2.0; Espressif32-Plattform 6.6.0.
- Arduino ESP32 2.0.14, PlatformIO-Paket `3.20014.231204`.
- NeoPixelBus `1d7ff38f14d04a976f9c6e365c83a232bcba04fc`.
- ESPAsyncWebServerWLED Tagobjekt `fb607f9322798a25413e701e470542eca648de6e`,
  Commit `39b830c852054444ea12a8b5d7fcb4fa004a89d7`.
- AsyncTCP 3.4.7. Netzwerkbibliotheken sind Compile-Abhängigkeiten von WLED;
  der Produktstart öffnet keine Netzwerkdienste.

`verify_dependencies.py` prüft die tatsächlich ausgewählten Bibliotheken nach
der Auflösung und schreibt `dependency-provenance.json` in den Buildordner.
Zusätzlich heruntergeladene Registry-Dubletten sind kein Beleg für deren Nutzung.

Die 4-MiB-Partitionstabelle reserviert eine einzelne 3-MiB-Anwendung und enthält
keinen zweiten OTA-Slot. Ein späterer Upload ändert auch die Partitionierung;
vorher den chipgebundenen vollständigen Flash-Rückweg prüfen. Dieser PR beauftragt
und protokolliert keinen neuen Geräteupload.

## Zugang und Einrichtung

Der [BLE-Vertrag](../../docs/led/ble-wire-v1.md) beschreibt Bytes, Fristen und
Sicherheitsgrenzen. Die Box erzeugt eine zufällige ID und einen 256-Bit-Schlüssel,
prüft Speicherung durch Rücklesen und erzeugt bei Lesefehlern keinen Ersatzzugang.
Der explizite lokale USB-Befehl `LARP-PROVISION-V1` exportiert den Zugang. Normale
Startmeldungen enthalten ihn nicht. Es gibt keinen im Produkt fest eingebauten
Testschlüssel und keine Schlüsselübertragung über BLE.

Beispiel nach gesondert beauftragter Installation der Produktfirmware:

```powershell
python -m pip install pyserial==3.5
python scripts/provision_wled_ble.py --port COM3 --output "$env:LOCALAPPDATA\LarpEscape\box-led-access.json"
```

Der Zielordner muss bereits existieren, privat und außerhalb des Repositories
liegen; eine vorhandene Datei wird nicht überschrieben. Unter Windows gelten die
geerbten Benutzerrechte dieses Ordners. Datei über einen vertrauenswürdigen lokalen
Weg zum Handy übertragen, in der App **Box-Zugang importieren** wählen und unnötige
Transportkopien danach entfernen. Die Datei ist selbst ein Zugangsschlüssel.
Die App hält genau einen eingerichteten Zugang, verschlüsselt ihn mit Android
Keystore und speichert ihn außerhalb automatischer App-Backups.

Nach Import Box auswählen/verbinden, gegenseitige Anmeldung abwarten, Effekt
wählen und Start drücken. App und Box prüfen HMAC-SHA256, frische Sitzungsnonces,
Befehlsfolge, eine kurze Befehlsfrist und die erwartete Ausgangsrevision. Eine
Bluetooth-Schreibantwort allein ist keine LED-Erfolgsbestätigung. Die Box sendet
diese erst, wenn WLED einen neuen Frame ausgegeben hat und der Bus wieder frei ist.
Die App verlangt zusätzlich die passende signierte Antwort auf ihren Auftrag.

Dies ist ein begrenzter LED-Zugang, keine Umsetzung der offenen F1-Spieler-/GM-
Rollen, P-256-Identitäten oder SMP-Bondabnahme. BLE-Daten sind hier nicht zusätzlich
verschlüsselt. HMAC schützt Befehlsintegrität und prüft den Schlüsselbesitz;
Vertraulichkeit, Funkverfügbarkeit und Schutz vor einem Live-Relay werden nicht
behauptet. Physischer USB-/Flashzugriff gehört zur vertrauenswürdigen Einrichtung.
Der Schlüssel liegt ohne Flashverschlüsselung in NVS. Eine gezielte Schlüssel-
rotation/Mehrbenutzerverwaltung ist noch nicht Teil dieses Umfangs.

## Wiederholbare Prüfung

```powershell
python test/test_wled_ble_wire/run.py
python firmware/wled/tests/run_output_tests.py
python scripts/prepare_wled.py
python -m platformio run -d .pio/wled-source-stable -e larp-wled-ble -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:assembleDebug :app:assembleRelease
```

Die Drahttests verwenden echte HMAC-Implementierungen und gemeinsame öffentliche
Goldenbytes; die Ausgabetests prüfen den tatsächlichen Adapter gegen einen
kontrollierten WLED-Port. Sie beweisen keine reale RMT-Ausgabe. Der echte ESP32-
Build kompiliert zusätzlich gegen die festgelegten Originalbibliotheken. Die CI
wiederholt diese Prüfungen und die vorhandene Qualitätsprüfung.

## Noch getrennt am Gerät abzunehmen

Der isolierte direkte-RMT-Versuch vom 15.10.2026, Quellenstand
`747812b3bc8a8b937fa8b5c40eea6d4a113afe6b`, wurde vom Nutzer mit vollständig rotem
Streifen, Aus, erneut Rot und Aus beim Trennen bestätigt. Frühere FastLED-Varianten
hatten trotz Befehlsbestätigung nicht zuverlässig abgeschaltet. Das ist ein Beleg
für Verdrahtung und den isolierten BLE-Weg, keine Abnahme dieses WLED-Produktbuilds.

Für das neue Paar noch prüfen: Start dunkel; USB-Import; alle drei Effekte;
selbstständiges Ende von Burst/Ripple; Auswahl ohne Wirkung; Wechsel und Neustart
über Start; Stop; Trennen mitten im Funkeln und direkt nach Start; Wiederverbinden
ohne automatisches Licht; falscher Zugang ohne LED-Wirkung. Dabei echte App-/Firmware-
Hashes und Nutzerbeobachtung getrennt von Protokollantworten festhalten.
